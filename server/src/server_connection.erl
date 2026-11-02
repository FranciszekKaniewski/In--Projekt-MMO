-module(server_connection).
-behaviour(gen_server).

-export([start_link/1, activate/1]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2, terminate/2]).

start_link(Socket) ->
    gen_server:start_link(?MODULE, Socket, []).

activate(Pid) ->
    gen_server:cast(Pid, activate).

init(Socket) ->
    Timer = erlang:start_timer(5000, self(), activate),
    {ok, #{socket => Socket, ready => false, timer => Timer}}.

handle_call(_Request, _From, State) ->
    {reply, {error, unsupported_request}, State}.

handle_cast(activate, #{ready := false, socket := Socket, timer := Timer} = State) ->
    erlang:cancel_timer(Timer),
    case inet:setopts(Socket, [{active, once}]) of
        ok -> {noreply, State#{ready => true}};
        {error, _Reason} -> {stop, normal, State}
    end;
handle_cast(_Message, State) ->
    {noreply, State}.

handle_info({tcp, Socket, Data}, #{socket := Socket, ready := true} = State) ->
    Reply = server_protocol:reply(Data),
    case gen_tcp:send(Socket, Reply) of
        ok ->
            %% Receive one complete frame at a time, then rearm reception.
            case inet:setopts(Socket, [{active, once}]) of
                ok -> {noreply, State};
                {error, _Reason} -> {stop, normal, State}
            end;
        {error, _Reason} ->
            {stop, normal, State}
    end;
handle_info({tcp_closed, Socket}, #{socket := Socket} = State) ->
    {stop, normal, State};
handle_info({tcp_error, Socket, _Reason}, #{socket := Socket} = State) ->
    {stop, normal, State};
handle_info({timeout, Timer, activate}, #{ready := false, timer := Timer} = State) ->
    {stop, normal, State};
handle_info(_Message, State) ->
    {noreply, State}.

terminate(_Reason, #{socket := Socket, timer := Timer}) ->
    erlang:cancel_timer(Timer),
    gen_tcp:close(Socket).
