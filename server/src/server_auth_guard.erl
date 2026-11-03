-module(server_auth_guard).
-behaviour(gen_server).
-export([start_link/0, acquire/3, release/1]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2, format_status/1]).

start_link() -> gen_server:start_link({local, ?MODULE}, ?MODULE, [], []).
acquire(Action, IP, Username) -> gen_server:call(?MODULE, {acquire, Action, IP, Username}).
release(Lease) -> gen_server:cast(?MODULE, {release, Lease, self()}).

init([]) ->
    {ok, Dummy} = server_password:hash(crypto:strong_rand_bytes(32)),
    erlang:send_after(60000, self(), sweep),
    {ok, #{buckets => #{}, leases => #{}, dummy => Dummy}}.

handle_call({acquire, Action, IP, Username}, {Pid, _}, State) ->
    Now = erlang:monotonic_time(millisecond),
    Keys = limits(Action, IP, Username),
    Buckets = maps:filter(fun(_, {_, Until}) -> Until > Now end, maps:get(buckets, State)),
    Available = lists:all(fun({Key, Limit, _}) ->
        {Count, _} = maps:get(Key, Buckets, {0, 0}), Count < Limit
    end, Keys),
    Leases = maps:get(leases, State),
    case {Available, map_size(Buckets) < 10000,
          map_size(Leases) < application:get_env(server, auth_workers, 4)} of
        {false, _, _} -> {reply, {error, rate_limited}, State#{buckets => Buckets}};
        {_, false, _} -> {reply, {error, server_busy}, State#{buckets => Buckets}};
        {_, _, false} -> {reply, {error, server_busy}, State#{buckets => Buckets}};
        {true, true, true} ->
            Updated = lists:foldl(fun({Key, _, Window}, Acc) ->
                {Count, Until} = maps:get(Key, Acc, {0, Now + Window}),
                Acc#{Key => {Count + 1, Until}}
            end, Buckets, Keys),
            Lease = erlang:monitor(process, Pid),
            {reply, {ok, Lease, maps:get(dummy, State)},
             State#{buckets => Updated, leases => Leases#{Lease => Pid}}}
    end;
handle_call(_, _, State) -> {reply, {error, unsupported_request}, State}.

limits(login, IP, Username) ->
    Normalized = string:casefold(unicode:characters_to_list(Username)),
    [{{login_ip, IP}, application:get_env(server, login_ip_limit, 30), 60000},
     {{login_user, Normalized}, application:get_env(server, login_user_limit, 5), 60000}];
limits(register, IP, _) ->
    [{{register_ip, IP}, application:get_env(server, register_ip_limit, 5), 3600000}].

handle_cast({release, Lease, Pid}, #{leases := Leases} = State) ->
    case maps:find(Lease, Leases) of
        {ok, Pid} -> erlang:demonitor(Lease, [flush]),
                     {noreply, State#{leases => maps:remove(Lease, Leases)}};
        _ -> {noreply, State}
    end;
handle_cast(_, State) -> {noreply, State}.

handle_info({'DOWN', Lease, process, _, _}, #{leases := Leases} = State) ->
    {noreply, State#{leases => maps:remove(Lease, Leases)}};
handle_info(sweep, #{buckets := Buckets} = State) ->
    Now = erlang:monotonic_time(millisecond),
    erlang:send_after(60000, self(), sweep),
    {noreply, State#{buckets => maps:filter(fun(_, {_, Until}) -> Until > Now end, Buckets)}};
handle_info(_, State) -> {noreply, State}.

format_status(Status) ->
    Status#{state => redacted, message => redacted, reason => redacted, log => []}.
