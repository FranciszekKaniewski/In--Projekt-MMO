-module(server_listener).
-behaviour(gen_server).

-export([start_link/0, port/0]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2, terminate/2]).

start_link() ->
    gen_server:start_link({local, ?MODULE}, ?MODULE, [], []).

port() ->
    gen_server:call(?MODULE, port).

init([]) ->
    {ok, IP} = application:get_env(server, ip),
    {ok, Port} = application:get_env(server, port),
    {ok, Certfile} = application:get_env(server, tls_certfile),
    {ok, Keyfile} = application:get_env(server, tls_keyfile),
    Options = [binary, {ip, IP}, {packet, 4}, {packet_size, 4096},
               {active, false}, {reuseaddr, true}, {nodelay, true},
               {send_timeout, 5000}, {send_timeout_close, true}],
    TLS = [{certfile, Certfile}, {keyfile, Keyfile},
           {versions, ['tlsv1.2', 'tlsv1.3']}, {verify, verify_none}],
    case ssl:listen(Port, Options ++ TLS) of
        {ok, ListenSocket} ->
            {ok, {_, ActualPort}} = ssl:sockname(ListenSocket),
            %% Accept transport here; each connection performs its own handshake.
            %% If the acceptor crashes, its listener is restarted by OTP.
            spawn_link(fun() -> accept_loop(ListenSocket) end),
            logger:notice("Game TLS server listening on ~s:~B",
                          [inet:ntoa(IP), ActualPort]),
            {ok, #{socket => ListenSocket, port => ActualPort}};
        {error, Reason} ->
            {stop, {listen_failed, Reason}}
    end.

handle_call(port, _From, State) ->
    {reply, maps:get(port, State), State};
handle_call(_Request, _From, State) ->
    {reply, {error, unsupported_request}, State}.

handle_cast(_Message, State) ->
    {noreply, State}.

handle_info(_Message, State) ->
    {noreply, State}.

terminate(_Reason, State) ->
    ssl:close(maps:get(socket, State)).

accept_loop(ListenSocket) ->
    case ssl:transport_accept(ListenSocket) of
        {ok, Socket} ->
            start_connection(Socket),
            accept_loop(ListenSocket);
        {error, closed} ->
            ok;
        {error, Reason} ->
            exit({accept_failed, Reason})
    end.

start_connection(Socket) ->
    Count = proplists:get_value(active, supervisor:count_children(server_connection_sup)),
    case Count < application:get_env(server, max_connections, 256) of
        true -> start_worker(Socket);
        false -> ssl:close(Socket)
    end.

start_worker(Socket) ->
    case server_connection_sup:start_connection(Socket) of
        {ok, Pid} ->
            %% Keep the socket passive until its new owner is ready.
            case ssl:controlling_process(Socket, Pid) of
                ok ->
                    server_connection:activate(Pid);
                {error, _Reason} ->
                    ssl:close(Socket),
                    gen_server:stop(Pid)
            end;
        {error, Reason} ->
            ssl:close(Socket),
            logger:warning("Cannot start client connection: ~p", [Reason])
    end.
