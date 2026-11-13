-module(server_connection).
-behaviour(gen_server).
-export([start_link/1, activate/1]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2, terminate/2, format_status/1]).

start_link(Socket) -> gen_server:start_link(?MODULE, Socket, []).
activate(Pid) -> gen_server:cast(Pid, activate).

init(Socket) ->
    %% Hide credentials from process introspection and redact crash reports below.
    process_flag(sensitive, true),
    Timer = erlang:start_timer(5000, self(), activate),
    {ok, #{socket => Socket, ready => false, timer => Timer,
           user => undefined, character_id => undefined, session_deadline => undefined}}.

handle_call(_, _, State) -> {reply, {error, unsupported_request}, State}.

handle_cast(activate, #{ready := false, socket := Socket, timer := Timer} = State) ->
    erlang:cancel_timer(Timer),
    case ssl:handshake(Socket, 5000) of
        {ok, SecureSocket} ->
            case {ssl:peername(SecureSocket), ssl:setopts(SecureSocket, [{active, once}])} of
                {{ok, {IP, _}}, ok} ->
                    {noreply, start_timer(authentication,
                        State#{socket => SecureSocket, ready => true, ip => IP})};
                _ -> {stop, normal, State}
            end;
        {error, _} -> {stop, normal, State}
    end;
handle_cast(_, State) -> {noreply, State}.

handle_info({ssl, Socket, Data}, #{socket := Socket, ready := true} = State) ->
    {Reply, Updated} = server_protocol:handle(Data, State),
    Next = update_timer(State, Updated),
    case send_reply(Socket, Reply) of
        ok ->
            case ssl:setopts(Socket, [{active, once}]) of
                ok -> {noreply, Next};
                {error, _} -> {stop, normal, Next}
            end;
        {error, _} -> {stop, normal, Next}
    end;
handle_info({ssl_closed, Socket}, #{socket := Socket} = State) -> {stop, normal, State};
handle_info({ssl_error, Socket, _}, #{socket := Socket} = State) -> {stop, normal, State};
handle_info({world_state, Ref, Frame}, #{world_ref := Ref, socket := Socket} = State) ->
    case ssl:send(Socket, Frame) of
        ok -> server_world:delivered(Ref), {noreply, State};
        {error, _} -> {stop, normal, State}
    end;
handle_info({timeout, Timer, session}, #{timer := Timer, socket := Socket} = State) ->
    case ssl:send(Socket, json:encode(server_protocol:error_message(session_expired))) of
        ok -> {noreply, start_timer(authentication, server_protocol:clear_session(State))};
        _ -> {stop, normal, State}
    end;
handle_info({timeout, Timer, _}, #{timer := Timer} = State) -> {stop, normal, State};
handle_info(_, State) -> {noreply, State}.

send_reply(_, no_reply) -> ok;
send_reply(Socket, {map_data, Header, Bytes}) ->
    case send_reply(Socket, Header) of
        ok -> send_map(Socket, Bytes);
        Error -> Error
    end;
send_reply(Socket, Reply) -> ssl:send(Socket, json:encode(Reply)).

send_map(Socket, <<Chunk:4096/binary, Rest/binary>>) when byte_size(Rest) > 0 ->
    case ssl:send(Socket, Chunk) of
        ok -> send_map(Socket, Rest);
        Error -> Error
    end;
send_map(Socket, Bytes) -> ssl:send(Socket, Bytes).

update_timer(#{session_deadline := Previous}, #{session_deadline := Previous} = Next) -> Next;
update_timer(#{timer := Timer}, Next) ->
    erlang:cancel_timer(Timer),
    Kind = case maps:get(user, Next) of undefined -> authentication; _ -> session end,
    start_timer(Kind, Next).

start_timer(authentication, State) ->
    State#{timer => erlang:start_timer(
        application:get_env(server, auth_timeout_ms, 60000), self(), authentication)};
start_timer(session, #{session_deadline := Deadline} = State) ->
    Delay = max(0, Deadline - erlang:monotonic_time(millisecond)),
    State#{timer => erlang:start_timer(Delay, self(), session)}.

terminate(_, #{socket := Socket, timer := Timer}) ->
    erlang:cancel_timer(Timer),
    ssl:close(Socket).

format_status(Status) ->
    Status#{state => redacted, message => redacted, reason => redacted, log => []}.
