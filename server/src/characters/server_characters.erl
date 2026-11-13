-module(server_characters).
-export([list/2, find_owned/2]).

list(UserId, AfterId) ->
    %% Bounded response fits the protocol's 4096-byte frame limit.
    case server_db:query(
        "SELECT id, name FROM public.characters WHERE user_id = $1 AND id > $2 "
        "ORDER BY id LIMIT 11", [UserId, AfterId]) of
        {ok, _, Rows} ->
            {Page, Rest} = lists:split(min(10, length(Rows)), Rows),
            {ok, [#{id => Id, name => Name} || {Id, Name} <- Page], Rest =/= []};
        _ -> {error, service_unavailable}
    end.

find_owned(UserId, CharacterId) ->
    case server_db:query(
        "SELECT id, name, map_key, position_x, position_y, facing, level, experience "
        "FROM public.characters WHERE id = $1 AND user_id = $2", [CharacterId, UserId]) of
        {ok, _, [{Id, Name, Map, X, Y, Facing, Level, XP}]} ->
            {ok, #{id => Id, name => Name, map_key => Map,
                   position_x => X, position_y => Y,
                   facing => Facing, level => Level, experience => XP}};
        {ok, _, []} -> {error, character_not_found};
        _ -> {error, service_unavailable}
    end.
