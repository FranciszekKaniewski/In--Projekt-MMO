-module(server_auth_tests).
-include_lib("eunit/include/eunit.hrl").

auth_database_test_() ->
    case os:getenv("SERVER_DB_TESTS") of
        "1" ->
            {setup, fun start/0, fun server_test_support:stop/1,
             fun({Port, _}) ->
                 [{"registration, login, ownership, logout and reconnect", {timeout, 20,
                     fun() -> lifecycle(Port) end}},
                  {"session expiration revokes access", fun() -> expiration(Port) end},
                  {"character lists are paginated and scoped to the account", fun() -> pages(Port) end},
                  {"world simulates and broadcasts movement to both players", {timeout, 15,
                     fun() -> world_movement(Port) end}},
                  {"database connection recovers after disconnect", fun() -> reconnect_db() end},
                  {"database failure never authenticates a connection", fun() -> database_failure(Port) end},
                  {"registration enforces password policy without locking out existing accounts",
                   fun() -> password_policy(Port) end}]
             end};
        _ -> []
    end.

start() ->
    application:load(server),
    ok = server_config:load(),
    %% Never run account fixtures in the developer's game database.
    Database = os:getenv("POSTGRES_TEST_DB"),
    true = is_list(Database) andalso lists:prefix("sdl_auth_test_", Database),
    Options = application:get_env(server, db_options, #{}),
    server_test_support:start([{db_options, Options#{database => Database}},
        {register_ip_limit, 1000}, {login_ip_limit, 1000}, {login_user_limit, 1000}]).

password_policy(Port) ->
    server_test_support:with_client(Port, fun(S) ->
        Name = unique(<<"policy">>),
        lists:foreach(fun(Invalid) ->
            ?assertEqual(error_reply(invalid_registration),
                credentials(S, <<"register">>, Name, Invalid))
        end, [<<"Abcdef!">>, <<"abcdefg!">>, <<"Abcdefgh">>, <<"Abcdefg ">>]),
        Minimum = <<"Abcdefg!">>,
        ?assertEqual(#{<<"type">> => <<"register_ok">>},
            credentials(S, <<"register">>, Name, Minimum)),
        ?assertMatch(#{<<"type">> := <<"login_ok">>},
            credentials(S, <<"login">>, Name, Minimum)),
        request(S, <<"logout">>),
        LegacyName = unique(<<"legacy">>),
        LegacyPassword = <<"legacy long passphrase">>,
        {ok, Hash} = server_password:hash(LegacyPassword),
        {ok, _} = server_accounts:create(LegacyName, Hash),
        ?assertMatch(#{<<"type">> := <<"login_ok">>},
            credentials(S, <<"login">>, LegacyName, LegacyPassword))
    end).

lifecycle(Port) ->
    Username = unique(<<"user">>),
    Other = unique(<<"other">>),
    server_test_support:with_client(Port, fun(S) ->
        register_user(S, Username),
        ?assertEqual(error_reply(authentication_required), request(S, <<"list_characters">>)),
        ?assertEqual(error_reply(registration_failed), credentials(S, <<"register">>, Username, password())),
        ?assertEqual(error_reply(registration_failed), credentials(S, <<"register">>, upper(Username), password())),
        ?assertEqual(error_reply(invalid_credentials), credentials(S, <<"login">>, Username, <<"the incorrect password">>)),
        ?assertEqual(error_reply(invalid_credentials), credentials(S, <<"login">>, Other, password())),
        ?assertEqual(error_reply(invalid_credentials),
                     credentials(S, <<"login">>, <<"x'OR'1'='1">>, password())),
        Login = login(S, upper(Username)),
        UserId = maps:get(<<"user_id">>, Login),
        ?assertEqual(Username, maps:get(<<"username">>, Login)),
        ?assertEqual(error_reply(character_required), request(S, <<"get_map">>)),
        {ok, _, [{Hash}]} = server_db:query("SELECT password_hash FROM public.users WHERE id = $1", [UserId]),
        ?assertMatch(<<"$argon2id$v=19$m=19456,t=2,p=1$", _/binary>>, Hash),
        ?assertNotEqual(password(), Hash),
        OwnCharacter = create_character(UserId),
        server_test_support:with_client(Port, fun(T) ->
            register_user(T, Other),
            OtherId = maps:get(<<"user_id">>, login(T, Other)),
            OtherCharacter = create_character(OtherId),
            ?assertEqual(error_reply(already_authenticated), credentials(S, <<"login">>, Other, password())),
            List = server_test_support:request(S, #{<<"type">> => <<"list_characters">>, <<"user_id">> => OtherId}),
            ?assertMatch(#{<<"characters">> := [#{<<"id">> := OwnCharacter}]}, List),
            ?assertEqual(error_reply(character_not_found), select(S, OtherCharacter)),
            ?assertEqual(error_reply(character_not_found), select(S, 9223372036854775807)),
            ?assertEqual(error_reply(invalid_message), select(S, <<"1">>)),
            ?assertEqual(error_reply(invalid_message), select(S, 9223372036854775808)),
            ?assertMatch(#{<<"type">> := <<"character_selected">>,
                           <<"character">> := #{<<"id">> := OwnCharacter,
                                                <<"map_key">> := <<"map_expanded">>}},
                         select(S, OwnCharacter)),
            check_map_download(S),
            ?assertEqual(error_reply(character_not_found), select(T, OwnCharacter)),
            ?assertEqual(error_reply(character_required), request(T, <<"get_map">>)),
            ?assertMatch(#{<<"type">> := <<"logout_ok">>}, request(S, <<"logout">>)),
            ?assertEqual(error_reply(authentication_required), request(S, <<"get_map">>)),
            ?assertEqual(error_reply(authentication_required), select(S, OwnCharacter)),
            ?assertMatch(#{<<"type">> := <<"characters">>}, request(T, <<"list_characters">>))
        end)
    end),
    server_test_support:with_client(Port, fun(S) ->
        ?assertEqual(error_reply(authentication_required), request(S, <<"list_characters">>)),
        login(S, Username)
    end).

check_map_download(Socket) ->
    #{<<"type">> := <<"map_data">>, <<"map_key">> := <<"map_expanded">>, <<"size">> := Size} =
        request(Socket, <<"get_map">>),
    {ok, Expected} = file:read_file(filename:join([code:priv_dir(server), "maps", "Map_expanded.tmj"])),
    ?assert(Size > 4096),
    ?assertEqual(byte_size(Expected), Size),
    ?assertEqual(Expected, iolist_to_binary(receive_map(Socket, Size, []))),
    ?assertEqual(#{<<"type">> => <<"pong">>}, request(Socket, <<"ping">>)).

receive_map(_, 0, Chunks) -> lists:reverse(Chunks);
receive_map(Socket, Size, Chunks) ->
    {ok, Chunk} = ssl:recv(Socket, 0, 5000),
    ?assertEqual(min(4096, Size), byte_size(Chunk)),
    receive_map(Socket, Size - byte_size(Chunk), [Chunk | Chunks]).

world_movement(Port) ->
    server_test_support:with_client(Port, fun(A) ->
        Name = unique(<<"world">>),
        AId = world_character(A, Name),
        ?assertEqual(error_reply(map_required), request(A, <<"enter_world">>)),
        ?assertMatch(#{<<"type">> := <<"move_error">>, <<"code">> := <<"world_required">>}, request(A, <<"move_input">>)),
        check_map_download(A),
        ?assertMatch(#{<<"type">> := <<"world_joined">>,
                       <<"character">> := #{<<"position_x">> := 1350.0}}, request(A, <<"enter_world">>)),
        server_test_support:with_client(Port, fun(B) ->
            BId = world_character(B, unique(<<"world">>)),
            check_map_download(B),
            ?assertMatch(#{<<"type">> := <<"world_joined">>}, request(B, <<"enter_world">>)),
            Pred = fun(Players) -> maps:is_key(AId, Players) andalso maps:is_key(BId, Players) end,
            _ = await_world(A, Pred, 20),
            _ = await_world(B, Pred, 20),
            ?assertMatch(#{<<"type">> := <<"move_error">>, <<"code">> := <<"invalid_message">>},
                         world_request(A, #{type => <<"move_input">>, seq => 1, dx => 2, dy => 0})),
            Inputs = [#{type => <<"move_input">>, seq => Seq, dx => 1, dy => 0, character_id => BId}
                      || Seq <- lists:seq(1, 4)],
            lists:foreach(fun(Input) -> ok = ssl:send(A, json:encode(Input)) end, Inputs),
            Moving = fun(Players) ->
                case maps:get(AId, Players, #{}) of
                    #{<<"position_x">> := X, <<"position_y">> := 1320.0,
                      <<"facing">> := 2, <<"input_ack">> := Ack} when X > 1350, Ack > 0 -> true;
                    _ -> false
                end
            end,
            _ = await_world(A, Moving, 20),
            Observed = await_world(B, Moving, 20),
            ?assertMatch(#{<<"position_x">> := 1350.0, <<"moving">> := false}, maps:get(BId, Observed)),
            %% Retransmitting an input must not advance the player again.
            ok = ssl:send(A, json:encode(lists:last(Inputs))),
            ok = ssl:send(A, json:encode(#{type => <<"move_input">>, seq => 5, dx => 0, dy => 0})),
            Stopped = fun(Players) ->
                case maps:get(AId, Players, #{}) of
                    #{<<"position_x">> := X, <<"moving">> := false, <<"input_ack">> := 5}
                      when abs(X - (1350 + 4 * 160 / 60)) < 0.00001 -> true;
                    _ -> false
                end
            end,
            Stop1 = await_world(B, Stopped, 20),
            Stop2 = await_world(B, Stopped, 20),
            ?assertEqual(maps:get(AId, Stop1), maps:get(AId, Stop2)),
            ?assertMatch(#{<<"type">> := <<"world_left">>}, world_request(B, #{type => <<"leave_world">>})),
            _ = await_world(A, fun(Players) -> not maps:is_key(BId, Players) end, 20),
            ?assertMatch(#{<<"type">> := <<"world_joined">>}, request(B, <<"enter_world">>)),
            _ = await_world(A, Pred, 20)
        end),
        _ = await_world(A, fun(Players) -> map_size(Players) =:= 1 end, 20)
    end).

world_character(Socket, Name) ->
    register_user(Socket, Name),
    Id = create_character(maps:get(<<"user_id">>, login(Socket, Name))),
    ?assertMatch(#{<<"type">> := <<"character_selected">>}, select(Socket, Id)),
    Id.

world_request(Socket, Message) ->
    ok = ssl:send(Socket, json:encode(Message)),
    await_reply(Socket, 20).

await_reply(_, 0) -> error(no_reply);
await_reply(Socket, Attempts) ->
    case server_test_support:receive_message(Socket) of
        #{<<"type">> := <<"world_state">>} -> await_reply(Socket, Attempts - 1);
        Reply -> Reply
    end.

await_world(_, _, 0) -> error(no_matching_world_state);
await_world(Socket, Match, Attempts) ->
    #{<<"type">> := <<"world_state">>, <<"map_key">> := <<"map_expanded">>,
      <<"tick">> := Tick, <<"players">> := Rows} =
        server_test_support:receive_message(Socket),
    ?assert(is_integer(Tick) andalso Tick > 0),
    Players = maps:from_list([{maps:get(<<"id">>, Row), Row} || Row <- Rows]),
    case Match(Players) of
        true -> Players;
        false -> await_world(Socket, Match, Attempts - 1)
    end.

expiration(Port) ->
    Username = unique(<<"expiry">>),
    server_test_support:with_client(Port, fun(S) ->
        register_user(S, Username),
        ok = application:set_env(server, session_timeout_ms, 80),
        try login(S, Username)
        after application:unset_env(server, session_timeout_ms)
        end,
        ?assertEqual(error_reply(session_expired), server_test_support:receive_message(S)),
        ?assertEqual(error_reply(authentication_required), request(S, <<"list_characters">>)),
        login(S, Username),
        ?assertMatch(#{<<"type">> := <<"characters">>}, request(S, <<"list_characters">>))
    end).

pages(Port) ->
    server_test_support:with_client(Port, fun(S) ->
        Name = unique(<<"pages">>), register_user(S, Name),
        UserId = maps:get(<<"user_id">>, login(S, Name)),
        Ids = [create_character(UserId) || _ <- lists:seq(1, 12)],
        #{<<"characters">> := First, <<"has_more">> := true} = request(S, <<"list_characters">>),
        ?assertEqual(lists:sublist(Ids, 10), [maps:get(<<"id">>, C) || C <- First]),
        #{<<"characters">> := Second, <<"has_more">> := false} =
            server_test_support:request(S, #{<<"type">> => <<"list_characters">>,
                <<"after_id">> => maps:get(<<"id">>, lists:last(First))}),
        ?assertEqual(lists:nthtail(10, Ids), [maps:get(<<"id">>, C) || C <- Second]),
        ?assertEqual(error_reply(invalid_message), server_test_support:request(S,
            #{<<"type">> => <<"list_characters">>, <<"after_id">> => -1}))
    end).

reconnect_db() ->
    {ok, _, [{1}]} = server_db:query("SELECT 1", []),
    #{connection := Connection} = sys:get_state(server_db),
    Monitor = erlang:monitor(process, Connection),
    ok = epgsql:close(Connection),
    receive {'DOWN', Monitor, process, Connection, _} -> ok
    after 1000 -> error(database_connection_not_closed)
    end,
    _ = sys:get_state(server_db),
    ?assertMatch({ok, _, [{1}]}, server_db:query("SELECT 1", [])).

database_failure(Port) ->
    Options = application:get_env(server, db_options, #{}),
    #{connection := Connection} = sys:get_state(server_db),
    Monitor = erlang:monitor(process, Connection),
    epgsql:close(Connection),
    receive {'DOWN', Monitor, process, Connection, _} -> ok
    after 1000 -> error(database_connection_not_closed)
    end,
    application:set_env(server, db_options, Options#{port => 1, timeout => 100}),
    try
        server_test_support:with_client(Port, fun(S) ->
            ?assertEqual(error_reply(service_unavailable),
                credentials(S, <<"login">>, unique(<<"failure">>), password())),
            ?assertEqual(error_reply(authentication_required), request(S, <<"list_characters">>)),
            ?assertEqual(#{<<"type">> => <<"pong">>}, request(S, <<"ping">>))
        end)
    after application:set_env(server, db_options, Options)
    end,
    ?assertMatch({ok, _, [{1}]}, server_db:query("SELECT 1", [])).

guard_test_() ->
    {setup, fun() -> server_test_support:start([{auth_workers, 1},
        {login_user_limit, 1}, {register_ip_limit, 1}]) end,
     fun server_test_support:stop/1,
     fun(_) -> [fun guard_limits/0] end}.

guard_limits() ->
    IP = {192, 0, 2, 1},
    {ok, Lease, _} = server_auth_guard:acquire(login, IP, <<"Alice">>),
    ?assertEqual({error, rate_limited}, server_auth_guard:acquire(login, IP, <<"ALICE">>)),
    ?assertEqual({error, server_busy}, server_auth_guard:acquire(login, IP, <<"Bob">>)),
    server_auth_guard:release(Lease),
    {ok, Second, _} = server_auth_guard:acquire(login, IP, <<"Bob">>),
    server_auth_guard:release(Second),
    {ok, Third, _} = server_auth_guard:acquire(register, IP, <<"First">>),
    server_auth_guard:release(Third),
    ?assertEqual({error, rate_limited}, server_auth_guard:acquire(register, IP, <<"Second">>)),
    Parent = self(),
    {Pid, Monitor} = spawn_monitor(fun() ->
        {ok, _, _} = server_auth_guard:acquire(login, IP, <<"Carol">>),
        Parent ! acquired
    end),
    receive acquired -> ok after 1000 -> error(no_lease) end,
    receive {'DOWN', Monitor, process, Pid, normal} -> ok after 1000 -> error(not_down) end,
    %% DOWN messages to the guard and this test may arrive in either order.
    wait_for_release(IP, 100),
    application:set_env(server, login_ip_limit, 1),
    OtherIP = {192, 0, 2, 2},
    {ok, IPLease, _} = server_auth_guard:acquire(login, OtherIP, <<"Eve">>),
    server_auth_guard:release(IPLease),
    ?assertEqual({error, rate_limited}, server_auth_guard:acquire(login, OtherIP, <<"Frank">>)).

wait_for_release(IP, Remaining) ->
    case server_auth_guard:acquire(login, IP, <<"Dave">>) of
        {ok, Lease, _} -> server_auth_guard:release(Lease);
        {error, server_busy} when Remaining > 0 -> timer:sleep(10), wait_for_release(IP, Remaining - 1);
        Other -> error({lease_not_released, Other})
    end.

request(Socket, Type) -> server_test_support:request(Socket, #{<<"type">> => Type}).
credentials(Socket, Type, Name, Password) -> server_test_support:request(Socket,
    #{<<"type">> => Type, <<"username">> => Name, <<"password">> => Password}).
register_user(Socket, Name) -> ?assertEqual(#{<<"type">> => <<"register_ok">>},
    credentials(Socket, <<"register">>, Name, password())).
login(Socket, Name) ->
    Reply = credentials(Socket, <<"login">>, Name, password()),
    ?assertMatch(#{<<"type">> := <<"login_ok">>}, Reply), Reply.
select(Socket, Id) -> server_test_support:request(Socket,
    #{<<"type">> => <<"select_character">>, <<"character_id">> => Id}).
create_character(UserId) ->
    {ok, 1, _, [{Id}]} = server_db:query(
        "INSERT INTO public.characters (user_id, name) VALUES ($1, $2) RETURNING id",
        [UserId, unique(<<"Hero">>)]), Id.
unique(Prefix) -> <<Prefix/binary, (binary:encode_hex(crypto:strong_rand_bytes(8)))/binary>>.
upper(Name) -> string:uppercase(Name).
password() -> <<"A long test passphrase 123!">>.
error_reply(Code) -> server_protocol:error_message(Code).
