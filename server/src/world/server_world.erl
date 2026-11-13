-module(server_world).
-behaviour(gen_server).
-export([start_link/0, enter/2, leave/0, move/3, delivered/1]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2]).

-define(TICK_RATE, 30).
-define(INPUT_RATE, 60).
-define(STEP, (160.0 / ?INPUT_RATE)).
-define(MAX_PENDING_INPUTS, 32).
%% The full JSON snapshot, including Unicode names and bigint IDs, fits one 4 KiB frame.
-define(MAX_PLAYERS_PER_MAP, 10).

start_link() -> gen_server:start_link({local, ?MODULE}, ?MODULE, [], []).
enter(Character, Bounds) -> gen_server:call(?MODULE, {enter, Character, Bounds}).
leave() -> gen_server:call(?MODULE, leave).
move(Sequence, X, Y) -> gen_server:call(?MODULE, {move, Sequence, X, Y}).
delivered(Ref) -> gen_server:cast(?MODULE, {delivered, self(), Ref}).

init([]) ->
    Start = now_ms(),
    schedule_tick(Start, 1),
    {ok, {0, Start, #{}}}.

handle_call({enter, #{id := Id, map_key := Map} = Character, Bounds}, {Pid, _}, {Tick, Start, Players} = State) ->
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
            Player = #{ref => Ref, character => Spawn, bounds => Bounds, inputs => queue:new(),
                       received => 0, applied => 0, waiting => false},
            {reply, {ok, Ref, maps:without([moving, vx, vy], Spawn)}, {Tick, Start, Players#{Pid => Player}}}
    end;
handle_call(leave, {Pid, _}, {Tick, Start, Players}) ->
    case maps:take(Pid, Players) of
        {#{ref := Ref}, Rest} ->
            erlang:demonitor(Ref, [flush]),
            {reply, ok, {Tick, Start, Rest}};
        error -> {reply, ok, {Tick, Start, Players}}
    end;
handle_call({move, Sequence, X, Y}, {Pid, _}, {Tick, Start, Players} = State) ->
    case maps:find(Pid, Players) of
        {ok, #{received := Received}} when Sequence =< Received ->
            {reply, ok, State};
        {ok, #{inputs := Inputs} = Player} ->
            case queue:len(Inputs) < ?MAX_PENDING_INPUTS of
                true ->
                    Updated = Player#{inputs := queue:in({Sequence, X, Y}, Inputs), received := Sequence},
                    {reply, ok, {Tick, Start, Players#{Pid := Updated}}};
                false -> {reply, {error, server_busy}, State}
            end;
        error -> {reply, {error, world_required}, State}
    end.

handle_cast({delivered, Pid, Ref}, {Tick, Start, Players} = State) ->
    case maps:find(Pid, Players) of
        {ok, #{ref := Ref} = Player} ->
            {noreply, {Tick, Start, Players#{Pid := Player#{waiting := false}}}};
        _ -> {noreply, State}
    end.

handle_info(tick, {Tick, Start, Players}) ->
    NextTick = Tick + 1,
    %% Keep the 33/34 ms cadence without running a burst of ticks after a stall.
    Now = now_ms(),
    NextDeadline = Start + (NextTick + 1) * 1000 div ?TICK_RATE,
    NextStart = case NextDeadline =< Now of
        true -> Now - NextTick * 1000 div ?TICK_RATE;
        false -> Start
    end,
    schedule_tick(NextStart, NextTick + 1),
    Updated = maps:map(fun(_, Player) -> step(Player) end, Players),
    {noreply, {NextTick, NextStart, broadcast(NextTick, Updated)}};
handle_info({'DOWN', Ref, process, Pid, _}, {Tick, Start, Players} = State) ->
    case maps:find(Pid, Players) of
        {ok, #{ref := Ref}} -> {noreply, {Tick, Start, maps:remove(Pid, Players)}};
        _ -> {noreply, State}
    end;
handle_info(_, State) -> {noreply, State}.

schedule_tick(Start, Tick) ->
    erlang:send_after(Start + Tick * 1000 div ?TICK_RATE, self(), tick, [{abs, true}]).
now_ms() -> erlang:monotonic_time(millisecond).

step(#{character := Character, bounds := Bounds, inputs := Inputs} = Player) ->
    case queue:out(Inputs) of
        {{value, {Sequence, X, Y}}, Rest} ->
            Next = advance(Character, Bounds, {X, Y}),
            case queue:out(Rest) of
                {{value, {SecondSequence, SecondX, SecondY}}, Remaining} ->
                    Player#{character := advance(Next, Bounds, {SecondX, SecondY}),
                            inputs := Remaining, applied := SecondSequence};
                {empty, _} -> Player#{character := Next, inputs := Rest, applied := Sequence}
            end;
        {empty, _} ->
            Player#{character := Character#{moving := false, vx := 0.0, vy := 0.0}}
    end.

advance(#{position_x := X, position_y := Y, facing := Facing} = Character, {Width, Height}, {Dx, Dy}) ->
    Step = case Dx * Dx + Dy * Dy of 2 -> ?STEP / math:sqrt(2); _ -> ?STEP end,
    NextX = min(max(X + Dx * Step, 10.0), Width - 10.0),
    NextY = min(max(Y + Dy * Step, 5.0), Height - 5.0),
    Character#{position_x := NextX, position_y := NextY, facing := facing(Dx, Dy, Facing),
               moving => NextX /= X orelse NextY /= Y,
               vx => (NextX - X) * ?INPUT_RATE, vy => (NextY - Y) * ?INPUT_RATE}.

facing(0, -1, _) -> 0;
facing(1, -1, _) -> 1;
facing(1, 0, _) -> 2;
facing(1, 1, _) -> 3;
facing(0, 1, _) -> 4;
facing(-1, 1, _) -> 5;
facing(-1, 0, _) -> 6;
facing(-1, -1, _) -> 7;
facing(0, 0, Previous) -> Previous.

broadcast(Tick, Players) ->
    Groups = maps:fold(fun(_, #{character := #{map_key := Map} = Character, applied := Applied}, Acc) ->
        Row = (maps:with([id, name, position_x, position_y, facing, moving, vx, vy], Character))#{input_ack => Applied},
        maps:update_with(Map, fun(Rows) -> [Row | Rows] end, [Row], Acc)
    end, #{}, Players),
    Frames = maps:map(fun(Map, Rows) ->
        json:encode(#{type => <<"world_state">>, map_key => Map, tick => Tick, players => Rows})
    end, Groups),
    maps:map(fun
        (Pid, #{ref := Ref, character := #{map_key := Map}, waiting := false} = Player) ->
            Pid ! {world_state, Ref, maps:get(Map, Frames)},
            Player#{waiting := true};
        (_, Player) -> Player
    end, Players).
