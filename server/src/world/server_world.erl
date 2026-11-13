-module(server_world).
-behaviour(gen_server).
-export([start_link/0, enter/2, leave/0, move/2, delivered/1]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2]).

-define(TICK_MS, 50).
-define(STEP, 8.0). % 160 world units per second.
-define(INPUT_TIMEOUT_MS, 1000).
%% The full JSON snapshot, including Unicode names and bigint IDs, fits one 4 KiB frame.
-define(MAX_PLAYERS_PER_MAP, 10).

start_link() -> gen_server:start_link({local, ?MODULE}, ?MODULE, [], []).
enter(Character, Bounds) -> gen_server:call(?MODULE, {enter, Character, Bounds}).
leave() -> gen_server:call(?MODULE, leave).
move(X, Y) -> gen_server:call(?MODULE, {move, X, Y}).
delivered(Ref) -> gen_server:cast(?MODULE, {delivered, self(), Ref}).

init([]) ->
    schedule_tick(),
    {ok, {0, #{}}}.

handle_call({enter, #{id := Id, map_key := Map} = Character, Bounds}, {Pid, _}, {Tick, Players} = State) ->
    Characters = [Active || #{character := Active} <- maps:values(Players)],
    AlreadyActive = lists:any(fun(#{id := OtherId}) -> OtherId =:= Id end, Characters),
    OnMap = [Active || #{map_key := OtherMap} = Active <- Characters, OtherMap =:= Map],
    case {maps:is_key(Pid, Players), AlreadyActive, length(OnMap)} of
        {true, _, _} -> {reply, {error, already_in_world}, State};
        {_, true, _} -> {reply, {error, character_in_world}, State};
        {_, _, Count} when Count >= ?MAX_PLAYERS_PER_MAP -> {reply, {error, world_full}, State};
        _ ->
            Ref = erlang:monitor(process, Pid),
            Spawn = advance(Character, Bounds, {0, 0}),
            Player = #{ref => Ref, character => Spawn, bounds => Bounds, direction => {0, 0},
                       input_at => now_ms(), waiting => false},
            {reply, {ok, Ref, maps:remove(moving, Spawn)}, {Tick, Players#{Pid => Player}}}
    end;
handle_call(leave, {Pid, _}, {Tick, Players}) ->
    case maps:take(Pid, Players) of
        {#{ref := Ref}, Rest} ->
            erlang:demonitor(Ref, [flush]),
            {reply, ok, {Tick, Rest}};
        error -> {reply, ok, {Tick, Players}}
    end;
handle_call({move, X, Y}, {Pid, _}, {Tick, Players} = State) ->
    case maps:find(Pid, Players) of
        {ok, Player} ->
            Updated = Player#{direction => {X, Y}, input_at => now_ms()},
            {reply, ok, {Tick, Players#{Pid := Updated}}};
        error -> {reply, {error, world_required}, State}
    end.

handle_cast({delivered, Pid, Ref}, {Tick, Players} = State) ->
    case maps:find(Pid, Players) of
        {ok, #{ref := Ref} = Player} ->
            {noreply, {Tick, Players#{Pid := Player#{waiting := false}}}};
        _ -> {noreply, State}
    end.

handle_info(tick, {Tick, Players}) ->
    schedule_tick(),
    Now = now_ms(),
    Updated = maps:map(fun(_, Player) -> step(Player, Now) end, Players),
    NextTick = Tick + 1,
    case NextTick rem 2 of
        0 -> {noreply, {NextTick, broadcast(Updated)}};
        1 -> {noreply, {NextTick, Updated}}
    end;
handle_info({'DOWN', Ref, process, Pid, _}, {Tick, Players} = State) ->
    case maps:find(Pid, Players) of
        {ok, #{ref := Ref}} -> {noreply, {Tick, maps:remove(Pid, Players)}};
        _ -> {noreply, State}
    end;
handle_info(_, State) -> {noreply, State}.

schedule_tick() -> erlang:send_after(?TICK_MS, self(), tick).
now_ms() -> erlang:monotonic_time(millisecond).

step(#{character := Character, bounds := Bounds, direction := Direction, input_at := At} = Player, Now) ->
    Input = case Now - At < ?INPUT_TIMEOUT_MS of true -> Direction; false -> {0, 0} end,
    Player#{character := advance(Character, Bounds, Input)}.

advance(#{position_x := X, position_y := Y, facing := Facing} = Character, {Width, Height}, {Dx, Dy}) ->
    Step = case Dx * Dx + Dy * Dy of 2 -> ?STEP / math:sqrt(2); _ -> ?STEP end,
    NextX = min(max(X + Dx * Step, 10.0), Width - 10.0),
    NextY = min(max(Y + Dy * Step, 5.0), Height - 5.0),
    Character#{position_x := NextX, position_y := NextY, facing := facing(Dx, Dy, Facing),
               moving => NextX /= X orelse NextY /= Y}.

facing(0, -1, _) -> 0;
facing(1, -1, _) -> 1;
facing(1, 0, _) -> 2;
facing(1, 1, _) -> 3;
facing(0, 1, _) -> 4;
facing(-1, 1, _) -> 5;
facing(-1, 0, _) -> 6;
facing(-1, -1, _) -> 7;
facing(0, 0, Previous) -> Previous.

broadcast(Players) ->
    Groups = maps:fold(fun(_, #{character := #{map_key := Map} = Character}, Acc) ->
        Row = maps:with([id, name, position_x, position_y, facing, moving], Character),
        maps:update_with(Map, fun(Rows) -> [Row | Rows] end, [Row], Acc)
    end, #{}, Players),
    Frames = maps:map(fun(Map, Rows) ->
        json:encode(#{type => <<"world_state">>, map_key => Map, players => Rows})
    end, Groups),
    maps:map(fun
        (Pid, #{ref := Ref, character := #{map_key := Map}, waiting := false} = Player) ->
            Pid ! {world_state, Ref, maps:get(Map, Frames)},
            Player#{waiting := true};
        (_, Player) -> Player
    end, Players).
