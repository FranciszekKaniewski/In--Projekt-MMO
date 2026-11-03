-module(server_accounts).
-export([create/2, find/1, list_characters/2, select_character/2]).

create(Username, PasswordHash) ->
    case server_db:query(
        "INSERT INTO public.users (username, password_hash) VALUES ($1, $2) "
        "ON CONFLICT DO NOTHING RETURNING id, username", [Username, PasswordHash]) of
        {ok, 1, _, [{Id, Name}]} -> {ok, #{id => Id, username => Name}};
        {ok, 0, _, []} -> {error, registration_failed};
        _ -> {error, service_unavailable}
    end.

find(Username) ->
    case server_db:query(
        "SELECT id, username, password_hash FROM public.users WHERE lower(username) = lower($1)",
        [Username]) of
        {ok, _, [{Id, Name, Hash}]} -> {ok, #{id => Id, username => Name}, Hash};
        {ok, _, []} -> not_found;
        _ -> {error, service_unavailable}
    end.

list_characters(UserId, AfterId) ->
    %% Bounded response fits the protocol's 4096-byte frame limit.
    case server_db:query(
        "SELECT id, name FROM public.characters WHERE user_id = $1 AND id > $2 "
        "ORDER BY id LIMIT 11", [UserId, AfterId]) of
        {ok, _, Rows} ->
            {Page, Rest} = lists:split(min(10, length(Rows)), Rows),
            {ok, [#{<<"id">> => Id, <<"name">> => Name} || {Id, Name} <- Page], Rest =/= []};
        _ -> {error, service_unavailable}
    end.

select_character(UserId, CharacterId) ->
    case server_db:query(
        "SELECT id, name, map_key, position_x, position_y, facing, level, experience "
        "FROM public.characters WHERE id = $1 AND user_id = $2", [CharacterId, UserId]) of
        {ok, _, [{Id, Name, Map, X, Y, Facing, Level, XP}]} ->
            {ok, #{<<"id">> => Id, <<"name">> => Name, <<"map_key">> => Map,
                   <<"position_x">> => X, <<"position_y">> => Y,
                   <<"facing">> => Facing, <<"level">> => Level, <<"experience">> => XP}};
        {ok, _, []} -> {error, character_not_found};
        _ -> {error, service_unavailable}
    end.
