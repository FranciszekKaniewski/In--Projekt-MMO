-module(server_test_support).
-export([start/1, stop/1, connect/2, request/2, receive_message/1, with_client/2]).

start(Overrides) ->
    application:load(server),
    Previous = application:get_all_env(server),
    Defaults = [{ip, {127, 0, 0, 1}}, {port, 0},
                {tls_certfile, filename:absname("test/fixtures/server.crt")},
                {tls_keyfile, filename:absname("test/fixtures/server.key")},
                {auth_timeout_ms, 60000}],
    lists:foreach(fun({K, V}) -> application:set_env(server, K, V) end,
                  Defaults ++ Overrides),
    {ok, _} = application:ensure_all_started(server),
    {server_listener:port(), Previous}.

stop({_Port, Previous}) ->
    ok = application:stop(server),
    lists:foreach(fun({K, _}) -> application:unset_env(server, K) end,
                  application:get_all_env(server)),
    lists:foreach(fun({K, V}) -> application:set_env(server, K, V) end, Previous).

connect(Port, Packet) ->
    ssl:connect({127, 0, 0, 1}, Port,
                [binary, {packet, Packet}, {active, false}, {verify, verify_peer},
                 {cacertfile, filename:absname("test/fixtures/ca.crt")},
                 {server_name_indication, "localhost"}], 3000).

request(Socket, Message) ->
    ok = ssl:send(Socket, json:encode(Message)),
    receive_message(Socket).

receive_message(Socket) ->
    {ok, Data} = ssl:recv(Socket, 0, 5000),
    json:decode(Data).

with_client(Port, Fun) ->
    {ok, Socket} = connect(Port, 4),
    try Fun(Socket) after ssl:close(Socket) end.
