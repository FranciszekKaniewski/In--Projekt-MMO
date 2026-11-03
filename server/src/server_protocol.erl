-module(server_protocol).
-export([handle/2, error_message/1, clear_session/1]).

handle(Data, State) ->
    Now = erlang:monotonic_time(millisecond),
    case maps:get(session_deadline, State, undefined) of
        Deadline when is_integer(Deadline), Deadline =< Now ->
            {error_message(session_expired), clear_session(State)};
        _ ->
            case decode(Data) of
                {ok, #{<<"type">> := Type} = Message} when is_binary(Type) ->
                    dispatch(Type, Message, State);
                {ok, _} -> {error_message(invalid_message), State};
                error -> {error_message(invalid_json), State}
            end
    end.

dispatch(<<"ping">>, _, State) -> {#{<<"type">> => <<"pong">>}, State};
dispatch(Type, Message, #{user := undefined} = State)
  when Type =:= <<"register">>; Type =:= <<"login">> ->
    Username = maps:get(<<"username">>, Message, undefined),
    Password = maps:get(<<"password">>, Message, undefined),
    case Type of
        <<"register">> ->
            case server_auth:register(maps:get(ip, State), Username, Password) of
                {ok, _} -> {#{<<"type">> => <<"register_ok">>}, State};
                {error, Code} -> {error_message(Code), State}
            end;
        <<"login">> ->
            case server_auth:login(maps:get(ip, State), Username, Password) of
                {ok, User} ->
                    Lifetime = application:get_env(server, session_timeout_ms, 28800000),
                    Deadline = erlang:monotonic_time(millisecond) + Lifetime,
                    {#{<<"type">> => <<"login_ok">>,
                       <<"user_id">> => maps:get(id, User),
                       <<"username">> => maps:get(username, User),
                       <<"expires_in_ms">> => Lifetime},
                     State#{user => User, character_id => undefined,
                            session_deadline => Deadline}};
                {error, Code} -> {error_message(Code), State}
            end
    end;
dispatch(Type, _, State) when Type =:= <<"register">>; Type =:= <<"login">> ->
    {error_message(already_authenticated), State};
dispatch(Type, _, #{user := undefined} = State)
  when Type =:= <<"list_characters">>; Type =:= <<"select_character">>;
       Type =:= <<"logout">> ->
    {error_message(authentication_required), State};
dispatch(<<"list_characters">>, Message, #{user := #{id := UserId}} = State) ->
    After = maps:get(<<"after_id">>, Message, 0),
    case valid_id(After, 0) of
        true ->
            case server_accounts:list_characters(UserId, After) of
                {ok, Characters, More} ->
                    {#{<<"type">> => <<"characters">>, <<"characters">> => Characters,
                       <<"has_more">> => More}, State};
                {error, Code} -> {error_message(Code), State}
            end;
        false -> {error_message(invalid_message), State}
    end;
dispatch(<<"select_character">>, Message, #{user := #{id := UserId}} = State) ->
    Id = maps:get(<<"character_id">>, Message, undefined),
    case valid_id(Id, 1) of
        true ->
            case server_accounts:select_character(UserId, Id) of
                {ok, Character} ->
                    {#{<<"type">> => <<"character_selected">>, <<"character">> => Character},
                     State#{character_id => Id}};
                {error, Code} -> {error_message(Code), State}
            end;
        false -> {error_message(invalid_message), State}
    end;
dispatch(<<"logout">>, _, State) ->
    {#{<<"type">> => <<"logout_ok">>}, clear_session(State)};
dispatch(_, _, State) -> {error_message(unknown_message_type), State}.

valid_id(Value, Minimum) ->
    is_integer(Value) andalso Value >= Minimum andalso Value =< 9223372036854775807.

clear_session(State) ->
    State#{user => undefined, character_id => undefined, session_deadline => undefined}.

decode(Data) ->
    try json:decode(Data) of Message -> {ok, Message}
    catch error:_ -> error
    end.

error_message(Code) ->
    #{<<"type">> => <<"error">>, <<"code">> => atom_to_binary(Code, utf8)}.
