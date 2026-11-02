-module(server_listener).
-behaviour(gen_server).

-export([start_link/0, port/0]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2, terminate/2]).

start_link() ->
    gen_server:start_link({local, ?MODULE}, ?MODULE, [], []).

port() ->
    gen_server:call(?MODULE, port).

init([]) ->
    IP = application:get_env(server, ip, {127, 0, 0, 1}),
    Port = application:get_env(server, port, 4000),
    Options = [binary, {ip, IP}, {packet, 4}, {packet_size, 4096},
               {active, false}, {reuseaddr, true}, {nodelay, true},
               {send_timeout, 5000}, {send_timeout_close, true}],
    case gen_tcp:listen(Port, Options) of
        {ok, ListenSocket} ->
            {ok, ActualPort} = inet:port(ListenSocket),
            %% accept/1 blocks in this linked process, never in a callback.
            %% If the acceptor crashes, its listener is restarted by OTP.
            spawn_link(fun() -> accept_loop(ListenSocket) end),
            logger:notice("Game server listening on ~s:~B",
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
    gen_tcp:close(maps:get(socket, State)).

accept_loop(ListenSocket) ->
    case gen_tcp:accept(ListenSocket) of
        {ok, Socket} ->
            start_connection(Socket),
            accept_loop(ListenSocket);
        {error, closed} ->
            ok;
        {error, Reason} ->
            exit({accept_failed, Reason})
    end.

start_connection(Socket) ->
    case server_connection_sup:start_connection(Socket) of
        {ok, Pid} ->
            %% Keep the socket passive until its new owner is ready.
            case gen_tcp:controlling_process(Socket, Pid) of
                ok ->
                    server_connection:activate(Pid);
                {error, _Reason} ->
                    gen_tcp:close(Socket),
                    gen_server:stop(Pid)
            end;
        {error, Reason} ->
            gen_tcp:close(Socket),
            logger:warning("Cannot start client connection: ~p", [Reason])
    end.
