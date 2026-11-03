-module(server_security_tests).
-include_lib("eunit/include/eunit.hrl").

passwords_test() ->
    %% Published reference Argon2 test vector (argon2/src/test.c).
    Expected = <<"$argon2id$v=19$m=256,t=2,p=1$c29tZXNhbHQ$"
                 "nf65EOgLrQMR/uIPnA4rEsF5h7TKyQwu9U1bMCHGi/4">>,
    {ok, _, Actual} = jargon:hash(<<"password">>, <<"somesalt">>, argon2id, 2, 256, 1, 32),
    ?assertEqual(Expected, Actual),
    ?assertEqual({ok, true}, jargon:verify(Expected, <<"password">>)),
    Password = <<"a sufficiently long password">>,
    {ok, First} = server_password:hash(Password),
    {ok, Second} = server_password:hash(Password),
    ?assertNotEqual(First, Second),
    ?assert(server_password:verify(Password, First)),
    ?assertNot(server_password:verify(<<"another password">>, First)),
    ?assert(server_password:valid_username(unicode:characters_to_binary("Żółw"))),
    ?assertNot(server_password:valid_username(<<"a b">>)),
    ?assertNot(server_password:valid_username(unicode:characters_to_binary([97, 160, 98]))),
    ?assertNot(server_password:valid_username(<<"abc\n">>)),
    ?assertNot(server_password:valid_username(<<255>>)),
    ?assertNot(server_password:valid_password(<<"Abcdef!">>)),
    ?assert(server_password:valid_password(<<"Abcdefg!">>)),
    ?assertNot(server_password:valid_password(<<"abcdefg!">>)),
    ?assertNot(server_password:valid_password(<<"Abcdefgh">>)),
    ?assertNot(server_password:valid_password(<<"Abcdefg1">>)),
    ?assertNot(server_password:valid_password(<<"Abcdefg ">>)),
    ?assert(server_password:valid_password(unicode:characters_to_binary("Żabcdef!"))),
    ?assertNot(server_password:valid_password(<<255>>)),
    Maximum = <<"A!", (binary:copy(<<"a">>, 126))/binary>>,
    ?assert(server_password:valid_password(Maximum)),
    ?assertNot(server_password:valid_password(<<Maximum/binary, "a">>)),
    ?assert(server_password:valid_login_password(<<"legacy long passphrase">>)),
    ?assertNot(server_password:valid_login_password(<<>>)),
    ?assertNot(server_password:valid_login_password(<<255>>)).

tls_test_() ->
    {setup, fun() -> server_test_support:start([]) end, fun server_test_support:stop/1,
     fun({Port, _}) ->
         [{"protected messages need authentication", fun() -> protected(Port) end},
          {"untrusted certificate is rejected", fun() -> untrusted(Port) end},
          {"hostname mismatch is rejected", fun() -> wrong_hostname(Port) end},
          {"an unfinished handshake does not block clients", fun() -> slow_peer(Port) end}]
     end}.

protected(Port) ->
    server_test_support:with_client(Port, fun(S) ->
        lists:foreach(fun(Type) ->
            Reply = server_test_support:request(S, #{<<"type">> => Type,
                         <<"user_id">> => 1, <<"character_id">> => 1}),
            ?assertEqual(error_reply(authentication_required), Reply)
        end, [<<"list_characters">>, <<"select_character">>, <<"logout">>]),
        ?assertEqual(error_reply(invalid_credentials), server_test_support:request(S,
            #{<<"type">> => <<"login">>, <<"username">> => [], <<"password">> => 3})),
        ?assertEqual(error_reply(invalid_registration), server_test_support:request(S,
            #{<<"type">> => <<"register">>, <<"username">> => <<"test">>,
              <<"password">> => <<"short">>}))
    end).

untrusted(Port) ->
    ?assertMatch({error, {tls_alert, {unknown_ca, _}}}, ssl:connect({127, 0, 0, 1}, Port,
        [{verify, verify_peer}, {cacerts, []}, {server_name_indication, "localhost"}], 3000)).

wrong_hostname(Port) ->
    ?assertMatch({error, {tls_alert, {bad_certificate, _}}}, ssl:connect({127, 0, 0, 1}, Port,
        [{verify, verify_peer}, {cacertfile, filename:absname("test/fixtures/ca.crt")},
         {server_name_indication, "wrong.example"}], 3000)).

slow_peer(Port) ->
    {ok, Raw} = gen_tcp:connect({127, 0, 0, 1}, Port, [{active, false}], 1000),
    try
        server_test_support:with_client(Port, fun(S) ->
            ?assertEqual(#{<<"type">> => <<"pong">>},
                server_test_support:request(S, #{<<"type">> => <<"ping">>}))
        end)
    after gen_tcp:close(Raw)
    end.

authentication_timeout_test_() ->
    {setup, fun() -> server_test_support:start([{auth_timeout_ms, 80}]) end,
     fun server_test_support:stop/1,
     fun({Port, _}) -> [fun() ->
         server_test_support:with_client(Port, fun(S) ->
             ?assertEqual({error, closed}, ssl:recv(S, 0, 1000))
         end)
     end] end}.

expired_state_test() ->
    State = #{user => #{id => 1}, character_id => 1,
              session_deadline => erlang:monotonic_time(millisecond) - 1},
    {Reply, Next} = server_protocol:handle(<<"{\"type\":\"list_characters\"}">>, State),
    ?assertEqual(error_reply(session_expired), Reply),
    ?assertEqual(undefined, maps:get(user, Next)),
    ?assertEqual(undefined, maps:get(character_id, Next)).

error_reply(Code) -> server_protocol:error_message(Code).
