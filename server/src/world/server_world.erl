-module(server_world).
-behaviour(gen_server).
-export([start_link/0, enter/3, leave/0, detach/0, move/3, delivered/1]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2]).

-define(TICK_RATE, 30).
-define(INPUT_RATE, 60).
-define(STEP, (160.0 / ?INPUT_RATE)).
-define(MAX_PENDING_INPUTS, 32).
%% The full JSON snapshot, including Unicode names and bigint IDs, fits one 4 KiB frame.
-define(MAX_PLAYERS_PER_MAP, 10).

start_link() -> gen_server:start_link({local, ?MODULE}, ?MODULE, [], []).
enter(UserId, #{id := Id, map_key := Map} = Character, Bounds) ->
    case gen_server:call(?MODULE, {reserve, UserId, Character, Bounds}) of
        {ok, Ref} ->
            %% Read after reserving the character, so an older session's checkpoint has finished.
            case server_characters:find_owned(UserId, Id) of
                {ok, #{map_key := Map} = Current} ->
                    gen_server:call(?MODULE, {activate, Ref, Current});
                {ok, _} -> cancel_reservation(Ref, {error, map_required});
                Error -> cancel_reservation(Ref, Error)
            end;
        Error -> Error
    end.

cancel_reservation(Ref, Error) ->
    ok = gen_server:call(?MODULE, {cancel, Ref}),
    Error.

leave() -> gen_server:call(?MODULE, leave, infinity).
detach() -> gen_server:cast(?MODULE, {detach, self()}).
move(Sequence, X, Y) -> gen_server:call(?MODULE, {move, Sequence, X, Y}).
delivered(Ref) -> gen_server:cast(?MODULE, {delivered, self(), Ref}).

init([]) ->
    Start = now_ms(),
    schedule_tick(Start, 1),
    {ok, {0, Start, #{}}}.

handle_call({reserve, UserId, #{id := Id, map_key := Map} = Character, Bounds}, {Pid, _},
            {Tick, Start, Players} = State) ->
    Entries = maps:values(Players),
    AlreadyActive = lists:any(fun(#{character := #{id := OtherId}}) -> OtherId =:= Id end, Entries),
    OnMap = [Player || #{character := #{map_key := OtherMap}} = Player <- Entries,
                       OtherMap =:= Map, occupies_slot(Player)],
    case {maps:is_key(Pid, Players), AlreadyActive, length(OnMap)} of
        {true, _, _} -> {reply, {error, already_in_world}, State};
        {_, true, _} -> {reply, {error, character_in_world}, State};
        {_, _, Count} when Count >= ?MAX_PLAYERS_PER_MAP -> {reply, {error, world_full}, State};
        _ ->
            Ref = erlang:monitor(process, Pid),
            Player = #{ref => Ref, user_id => UserId, character => Character, bounds => Bounds,
                       inputs => queue:new(), received => 0, applied => 0, waiting => false, status => reserved},
            {reply, {ok, Ref}, {Tick, Start, Players#{Pid => Player}}}
    end;
handle_call({activate, Ref, Character}, {Pid, _}, {Tick, Start, Players}) ->
    #{ref := Ref, bounds := Bounds, status := reserved} = Player = maps:get(Pid, Players),
    Spawn = advance(Character, Bounds, {0, 0}),
    Active = Player#{character := Spawn, status := active},
    {reply, {ok, Ref, maps:without([moving, vx, vy], Spawn)}, {Tick, Start, Players#{Pid := Active}}};
handle_call({cancel, Ref}, {Pid, _}, {Tick, Start, Players}) ->
    #{ref := Ref, status := reserved} = maps:get(Pid, Players),
    {reply, ok, {Tick, Start, remove_player(Pid, Ref, Players)}};
handle_call(leave, {Pid, _} = From, {Tick, Start, Players} = State) ->
    case maps:find(Pid, Players) of
        {ok, #{status := active} = Player} ->
            {noreply, {Tick, Start, Players#{Pid := save(Pid, Player, From)}}};
        _ -> {reply, ok, State}
    end;
handle_call({move, Sequence, X, Y}, {Pid, _}, {Tick, Start, Players} = State) ->
    case maps:find(Pid, Players) of
        {ok, #{status := active, received := Received}} when Sequence =< Received ->
            {reply, ok, State};
        {ok, #{status := active, inputs := Inputs} = Player} ->
            case queue:len(Inputs) < ?MAX_PENDING_INPUTS of
                true ->
                    Updated = Player#{inputs := queue:in({Sequence, X, Y}, Inputs), received := Sequence},
                    {reply, ok, {Tick, Start, Players#{Pid := Updated}}};
                false -> {reply, {error, server_busy}, State}
            end;
        _ -> {reply, {error, world_required}, State}
    end.

handle_cast({detach, Pid}, {Tick, Start, Players}) ->
    {noreply, {Tick, Start, detach(Pid, Players)}};
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
        {ok, #{ref := Ref}} -> {noreply, {Tick, Start, detach(Pid, Players)}};
        _ -> {noreply, State}
    end;
handle_info({position_saved, Pid, Ref, Result}, {Tick, Start, Players}) ->
    {noreply, {Tick, Start, finish_save(Pid, Ref, Result, Players)}};
handle_info({retry_position_save, Pid, Ref}, {Tick, Start, Players} = State) ->
    case maps:find(Pid, Players) of
        {ok, #{ref := Ref, status := retrying} = Player} ->
            {noreply, {Tick, Start, Players#{Pid := save(Pid, Player, disconnected)}}};
        _ -> {noreply, State}
    end;
handle_info(_, State) -> {noreply, State}.

remove_player(Pid, Ref, Players) ->
    erlang:demonitor(Ref, [flush]),
    maps:remove(Pid, Players).

detach(Pid, Players) ->
    case maps:find(Pid, Players) of
        {ok, #{status := active} = Player} ->
            Players#{Pid := save(Pid, Player, disconnected)};
        {ok, #{status := {saving, _}} = Player} ->
            Players#{Pid := Player#{status := {saving, disconnected}}};
        {ok, #{status := reserved, ref := Ref}} ->
            remove_player(Pid, Ref, Players);
        _ -> Players
    end.

save(Pid, #{ref := Ref, user_id := UserId, character := Character} = Player, From) ->
    World = self(),
    spawn(fun() ->
        Result = server_characters:save_position(UserId, Character),
        World ! {position_saved, Pid, Ref, Result}
    end),
    Player#{status := {saving, From},
            character := Character#{moving := false, vx := 0.0, vy := 0.0}}.

finish_save(Pid, Ref, Result, Players) ->
    case maps:find(Pid, Players) of
        {ok, #{ref := Ref, status := {saving, disconnected}, character := #{id := Id}} = Player} ->
            case Result of
                {error, position_save_failed} ->
                    logger:warning("Position save failed for disconnected character ~B; retrying", [Id]),
                    erlang:send_after(2000, self(), {retry_position_save, Pid, Ref}),
                    Players#{Pid := Player#{status := retrying}};
                _ -> remove_player(Pid, Ref, Players)
            end;
        {ok, #{ref := Ref, status := {saving, From}} = Player} ->
            gen_server:reply(From, Result),
            case Result of
                ok -> remove_player(Pid, Ref, Players);
                {error, _} -> Players#{Pid := Player#{status := active}}
            end;
        _ -> Players
    end.

schedule_tick(Start, Tick) ->
    erlang:send_after(Start + Tick * 1000 div ?TICK_RATE, self(), tick, [{abs, true}]).
now_ms() -> erlang:monotonic_time(millisecond).

step(#{status := active, character := Character, bounds := Bounds, inputs := Inputs} = Player) ->
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
    end;
step(Player) -> Player.

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

occupies_slot(#{status := reserved}) -> true;
occupies_slot(Player) -> visible(Player).

visible(#{status := active}) -> true;
visible(#{status := {saving, {_, _}}}) -> true;
visible(_) -> false.

broadcast(Tick, Players) ->
    Visible = maps:filter(fun(_, Player) -> visible(Player) end, Players),
    Groups = maps:fold(fun(_, #{character := #{map_key := Map} = Character, applied := Applied}, Acc) ->
        Row = (maps:with([id, name, position_x, position_y, facing, moving, vx, vy], Character))#{input_ack => Applied},
        maps:update_with(Map, fun(Rows) -> [Row | Rows] end, [Row], Acc)
    end, #{}, Visible),
    Frames = maps:map(fun(Map, Rows) ->
        json:encode(#{type => <<"world_state">>, map_key => Map, tick => Tick, players => Rows})
    end, Groups),
    maps:map(fun
        (Pid, #{ref := Ref, status := active, character := #{map_key := Map}, waiting := false} = Player) ->
            Pid ! {world_state, Ref, maps:get(Map, Frames)},
            Player#{waiting := true};
        (_, Player) -> Player
    end, Players).
