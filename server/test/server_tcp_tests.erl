-module(server_tcp_tests).

-include_lib("eunit/include/eunit.hrl").

tcp_test_() ->
    {setup, fun start_server/0, fun stop_server/1,
     fun({Port, _PreviousEnv}) ->
         [{"ping and repeated requests", fun() -> ping_pong(Port) end},
          {"multiple simultaneous clients", fun() -> multiple_clients(Port) end},
          {"fragmented header and body", fun() -> fragmented_frame(Port) end},
          {"multiple frames in one write", fun() -> coalesced_frames(Port) end},
          {"invalid messages keep the connection usable", fun() -> invalid_messages(Port) end},
          {"a body at the size limit is accepted", fun() -> maximum_frame(Port) end},
          {"an oversized frame closes only its connection", fun() -> oversized_frame(Port) end},
          {"disconnect removes its worker", fun() -> disconnect_cleanup(Port) end},
          {"a failed worker does not affect another client", fun() -> worker_failure(Port) end},
          %% Last: a listener configured with port 0 may restart on another port.
          {"listener restart preserves existing clients", fun() -> listener_failure(Port) end}]
     end}.

start_server() ->
    case application:load(server) of
        ok -> ok;
        {error, {already_loaded, server}} -> ok
    end,
    PreviousEnv = [{Key, application:get_env(server, Key)} || Key <- [ip, port]],
    ok = application:set_env(server, ip, {127, 0, 0, 1}),
    ok = application:set_env(server, port, 0),
    {ok, _Apps} = application:ensure_all_started(server),
    {server_listener:port(), PreviousEnv}.

stop_server({_Port, PreviousEnv}) ->
    ok = application:stop(server),
    lists:foreach(fun
        ({Key, {ok, Value}}) -> application:set_env(server, Key, Value);
        ({Key, undefined}) -> application:unset_env(server, Key)
    end, PreviousEnv).

ping_pong(Port) ->
    with_client(Port, fun(Socket) ->
        assert_pong(Socket),
        assert_pong(Socket)
    end).

multiple_clients(Port) ->
    with_client(Port, fun(First) ->
        with_client(Port, fun(Second) ->
            assert_pong(First),
            assert_pong(Second),
            ok = gen_tcp:close(First),
            assert_pong(Second)
        end)
    end).

fragmented_frame(Port) ->
    with_raw_client(Port, fun(Socket) ->
        <<FirstByte:1/binary, HeaderRest:3/binary, Body/binary>> = frame(ping()),
        <<BodyStart:3/binary, BodyRest/binary>> = Body,
        ok = gen_tcp:send(Socket, FirstByte),
        ?assertEqual({error, timeout}, gen_tcp:recv(Socket, 0, 20)),
        ok = gen_tcp:send(Socket, [HeaderRest, BodyStart]),
        ?assertEqual({error, timeout}, gen_tcp:recv(Socket, 0, 20)),
        ok = gen_tcp:send(Socket, BodyRest),
        ok = inet:setopts(Socket, [{packet, 4}]),
        ?assertEqual(pong(), receive_message(Socket))
    end).

coalesced_frames(Port) ->
    with_raw_client(Port, fun(Socket) ->
        ok = gen_tcp:send(Socket, [frame(ping()), frame(ping())]),
        ok = inet:setopts(Socket, [{packet, 4}]),
        ?assertEqual(pong(), receive_message(Socket)),
        ?assertEqual(pong(), receive_message(Socket))
    end).

invalid_messages(Port) ->
    Cases = [{<<"not json">>, <<"invalid_json">>},
             {<<"{\"type\":\"ping\"}garbage">>, <<"invalid_json">>},
             {<<"{\"type\":\"", 255, "\"}">>, <<"invalid_json">>},
             {<<"{}">>, <<"invalid_message">>},
             {<<"[]">>, <<"invalid_message">>},
             {<<"null">>, <<"invalid_message">>},
             {<<"{\"type\":1}">>, <<"invalid_message">>},
             {<<"{\"type\":\"unknown\"}">>, <<"unknown_message_type">>},
             {<<"{\"type\":\"join\",\"name\":\"Guest\"}">>, <<"unknown_message_type">>},
             {<<"{\"type\":\"chat_send\",\"text\":\"hello\"}">>, <<"unknown_message_type">>}],
    with_client(Port, fun(Socket) ->
        lists:foreach(fun({Data, Code}) ->
            ok = gen_tcp:send(Socket, Data),
            ?assertEqual(#{<<"type">> => <<"error">>, <<"code">> => Code},
                         receive_message(Socket)),
            assert_pong(Socket)
        end, Cases)
    end).

maximum_frame(Port) ->
    Base = #{<<"type">> => <<"ping">>, <<"padding">> => <<>>},
    EmptySize = iolist_size(json:encode(Base)),
    Padding = binary:copy(<<"a">>, 4096 - EmptySize),
    Data = json:encode(Base#{<<"padding">> => Padding}),
    ?assertEqual(4096, iolist_size(Data)),
    with_client(Port, fun(Socket) ->
        ok = gen_tcp:send(Socket, Data),
        ?assertEqual(pong(), receive_message(Socket))
    end).

oversized_frame(Port) ->
    with_client(Port, fun(Healthy) ->
        with_raw_client(Port, fun(Socket) ->
            %% The server rejects the length without waiting for the body.
            ok = gen_tcp:send(Socket, <<4097:32/unsigned-big>>),
            ?assertEqual({error, closed}, gen_tcp:recv(Socket, 0, 1000))
        end),
        assert_pong(Healthy)
    end).

disconnect_cleanup(Port) ->
    await(fun() -> connection_count() =:= 0 end),
    {ok, Socket} = connect(Port, 4),
    assert_pong(Socket),
    [{_Id, Pid, worker, _Modules}] = supervisor:which_children(server_connection_sup),
    Monitor = erlang:monitor(process, Pid),
    ok = gen_tcp:close(Socket),
    receive
        {'DOWN', Monitor, process, Pid, Reason} -> ?assertEqual(normal, Reason)
    after 1000 ->
        error(connection_not_removed)
    end,
    await(fun() -> connection_count() =:= 0 end).

worker_failure(Port) ->
    await(fun() -> connection_count() =:= 0 end),
    with_client(Port, fun(First) ->
        assert_pong(First),
        [{_Id, Pid, worker, _Modules}] = supervisor:which_children(server_connection_sup),
        with_client(Port, fun(Second) ->
            assert_pong(Second),
            Monitor = erlang:monitor(process, Pid),
            exit(Pid, kill),
            receive
                {'DOWN', Monitor, process, Pid, killed} -> ok
            after 1000 ->
                error(worker_did_not_exit)
            end,
            ?assertEqual({error, closed}, gen_tcp:recv(First, 0, 1000)),
            assert_pong(Second),
            await(fun() -> connection_count() =:= 1 end),
            with_client(Port, fun assert_pong/1)
        end)
    end).

listener_failure(Port) ->
    with_client(Port, fun(Socket) ->
        assert_pong(Socket),
        Listener = whereis(server_listener),
        exit(Listener, kill),
        await(fun() ->
            Current = whereis(server_listener),
            is_pid(Current) andalso Current =/= Listener
        end),
        NewPort = server_listener:port(),
        assert_pong(Socket),
        with_client(NewPort, fun assert_pong/1)
    end).

with_client(Port, Fun) ->
    with_socket(Port, 4, Fun).

with_raw_client(Port, Fun) ->
    with_socket(Port, 0, Fun).

with_socket(Port, Packet, Fun) ->
    {ok, Socket} = connect(Port, Packet),
    try Fun(Socket)
    after gen_tcp:close(Socket)
    end.

connect(Port, Packet) ->
    gen_tcp:connect({127, 0, 0, 1}, Port,
                    [binary, {packet, Packet}, {active, false}], 1000).

assert_pong(Socket) ->
    ok = gen_tcp:send(Socket, ping()),
    ?assertEqual(pong(), receive_message(Socket)).

ping() -> <<"{\"type\":\"ping\"}">>.
pong() -> #{<<"type">> => <<"pong">>}.

receive_message(Socket) ->
    {ok, Data} = gen_tcp:recv(Socket, 0, 1000),
    json:decode(Data).

frame(Data) ->
    <<(byte_size(Data)):32/unsigned-big, Data/binary>>.

connection_count() ->
    proplists:get_value(active, supervisor:count_children(server_connection_sup)).

await(Predicate) ->
    await(Predicate, 100).

await(Predicate, Attempts) ->
    case Predicate() of
        true -> ok;
        false when Attempts > 0 ->
            timer:sleep(10),
            await(Predicate, Attempts - 1);
        false -> error(condition_not_reached)
    end.
