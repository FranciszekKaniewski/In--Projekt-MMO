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
       Type =:= <<"get_map">>; Type =:= <<"logout">>; Type =:= <<"enter_world">>;
       Type =:= <<"move_input">>; Type =:= <<"leave_world">> ->
    {error_message(authentication_required), State};
dispatch(Type, _, #{world_ref := _} = State)
  when Type =:= <<"select_character">>; Type =:= <<"get_map">>; Type =:= <<"enter_world">> ->
    {error_message(already_in_world), State};
dispatch(<<"list_characters">>, Message, #{user := #{id := UserId}} = State) ->
    After = maps:get(<<"after_id">>, Message, 0),
    case valid_id(After, 0) of
        true ->
            case server_characters:list(UserId, After) of
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
            case server_characters:find_owned(UserId, Id) of
                {ok, Character} ->
                    {#{<<"type">> => <<"character_selected">>, <<"character">> => Character},
                     maps:remove(map_bounds, State#{character_id => Id, character => Character})};
                {error, Code} -> {error_message(Code), State}
            end;
        false -> {error_message(invalid_message), State}
    end;
dispatch(<<"get_map">>, _, #{character := #{map_key := MapKey}} = State) ->
    case server_maps:read(MapKey) of
        {ok, Bytes} ->
            case server_maps:bounds(Bytes) of
                {ok, Bounds} ->
                    Header = #{<<"type">> => <<"map_data">>, <<"map_key">> => MapKey,
                               <<"size">> => byte_size(Bytes)},
                    {{map_data, Header, Bytes}, State#{map_bounds => Bounds}};
                {error, Code} -> {error_message(Code), State}
            end;
        {error, Code} -> {error_message(Code), State}
    end;
dispatch(<<"get_map">>, _, State) -> {error_message(character_required), State};
dispatch(<<"enter_world">>, _, #{character := Character, map_bounds := Bounds} = State) ->
    case server_world:enter(Character, Bounds) of
        {ok, Ref, Spawn} ->
            {#{type => <<"world_joined">>, character => Spawn}, State#{world_ref => Ref}};
        {error, Code} -> {error_message(Code), State}
    end;
dispatch(<<"enter_world">>, _, State) -> {error_message(map_required), State};
dispatch(<<"move_input">>, Message, #{world_ref := _} = State) ->
    case Message of
        #{<<"seq">> := Sequence, <<"dx">> := X, <<"dy">> := Y}
          when is_integer(X), X >= -1, X =< 1, is_integer(Y), Y >= -1, Y =< 1 ->
            case valid_id(Sequence, 1) of
                true ->
                    case server_world:move(Sequence, X, Y) of
                        ok -> {no_reply, State};
                        {error, Code} -> {move_error(Code), State}
                    end;
                false -> {move_error(invalid_message), State}
            end;
        _ -> {move_error(invalid_message), State}
    end;
dispatch(<<"move_input">>, _, State) -> {move_error(world_required), State};
dispatch(<<"leave_world">>, _, State) ->
    {#{type => <<"world_left">>}, leave_world(State)};
dispatch(<<"logout">>, _, State) ->
    {#{<<"type">> => <<"logout_ok">>}, clear_session(State)};
dispatch(_, _, State) -> {error_message(unknown_message_type), State}.

valid_id(Value, Minimum) ->
    is_integer(Value) andalso Value >= Minimum andalso Value =< 9223372036854775807.

clear_session(State) ->
    Left = leave_world(State),
    maps:without([character, map_bounds],
                 Left#{user => undefined, character_id => undefined, session_deadline => undefined}).

leave_world(#{world_ref := _} = State) ->
    ok = server_world:leave(),
    maps:remove(world_ref, State);
leave_world(State) -> State.

decode(Data) ->
    try json:decode(Data) of Message -> {ok, Message}
    catch error:_ -> error
    end.

error_message(Code) ->
    #{<<"type">> => <<"error">>, <<"code">> => atom_to_binary(Code, utf8)}.

move_error(Code) ->
    (error_message(Code))#{<<"type">> := <<"move_error">>}.
