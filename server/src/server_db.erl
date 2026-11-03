-module(server_db).
-behaviour(gen_server).
-export([start_link/0, query/2]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2, terminate/2, format_status/1]).

start_link() -> gen_server:start_link({local, ?MODULE}, ?MODULE, [], []).

query(SQL, Params) ->
    try gen_server:call(?MODULE, {query, SQL, Params}, 10000)
    catch exit:_ -> {error, unavailable}
    end.

init([]) ->
    process_flag(trap_exit, true),
    {ok, #{connection => undefined}}.

handle_call({query, SQL, Params}, _From, State) ->
    case connection(State) of
        {ok, Connection} ->
            try epgsql:equery(Connection, SQL, Params) of
                Result -> {reply, Result, State#{connection => Connection}}
            catch _:_ ->
                close(Connection),
                {reply, {error, unavailable}, State#{connection => undefined}}
            end;
        {error, _} -> {reply, {error, unavailable}, State}
    end;
handle_call(_, _, State) -> {reply, {error, unsupported_request}, State}.

handle_cast(_, State) -> {noreply, State}.
handle_info({'EXIT', Connection, _}, #{connection := Connection} = State) ->
    {noreply, State#{connection => undefined}};
handle_info(_, State) -> {noreply, State}.

connection(#{connection := Pid}) when is_pid(Pid) ->
    case is_process_alive(Pid) of true -> {ok, Pid}; false -> connect() end;
connection(_) -> connect().

connect() ->
    try epgsql:connect(application:get_env(server, db_options, #{})) of
        {ok, Pid} ->
            case epgsql:squery(Pid, "SET statement_timeout = '3s'; SET lock_timeout = '1s'") of
                [{ok, [], []}, {ok, [], []}] -> {ok, Pid};
                _ -> close(Pid), {error, unavailable}
            end;
        Error -> Error
    catch _:_ -> {error, unavailable}
    end.

terminate(_, #{connection := Pid}) when is_pid(Pid) -> close(Pid);
terminate(_, _) -> ok.

close(Pid) ->
    try epgsql:close(Pid) catch _:_ -> ok end,
    ok.

format_status(Status) ->
    Status#{state => redacted, message => redacted, reason => redacted, log => []}.
