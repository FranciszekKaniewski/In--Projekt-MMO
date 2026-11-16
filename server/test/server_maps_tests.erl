-module(server_maps_tests).
-include_lib("eunit/include/eunit.hrl").

-define(HALF_SIZE, {10.0, 5.0}).

shore_and_lake_test() ->
    Map = load_map("Map_expanded.tmj"),
    Start = {1350.0, 1320.0},
    Shore = {2358.0, 1320.0},
    ?assertEqual(Shore, server_maps:move(Map, Start, {2560.0, 1320.0}, ?HALF_SIZE)),
    ?assertEqual(Shore, server_maps:move(Map, Shore, {2360.0, 1320.0}, ?HALF_SIZE)),
    ?assertEqual({2358.0, 1322.0}, server_maps:move(Map, Shore, {2360.0, 1322.0}, ?HALF_SIZE)),
    ?assertEqual({2356.0, 1320.0}, server_maps:move(Map, Shore, {2356.0, 1320.0}, ?HALF_SIZE)),
    ?assertEqual({362.0, 1320.0}, server_maps:move(Map, Start, {0.0, 1320.0}, ?HALF_SIZE)),
    ?assertEqual({1350.0, 997.0}, server_maps:move(Map, Start, {1350.0, 800.0}, ?HALF_SIZE)),
    ?assertEqual({1350.0, 2011.0}, server_maps:move(Map, Start, {1350.0, 2560.0}, ?HALF_SIZE)).

saved_position_test() ->
    Map = load_map("Map_example2.tmj"),
    Dry = {640.0, 640.0},
    Shore = {1174.0, 640.0},
    Water = {1220.0, 640.0},
    ?assertEqual({ok, Dry}, server_maps:spawn_position(Map, Dry, ?HALF_SIZE)),
    ?assertEqual(Shore, server_maps:move(Map, Dry, Water, ?HALF_SIZE)),
    ?assertEqual({ok, Shore}, server_maps:spawn_position(Map, Shore, ?HALF_SIZE)),
    {ok, Relocated} = server_maps:spawn_position(Map, Water, ?HALF_SIZE),
    ?assertNotEqual(Water, Relocated),
    ?assertEqual({ok, Relocated}, server_maps:spawn_position(Map, Relocated, ?HALF_SIZE)).

malformed_collision_layer_test() ->
    {ok, Bytes} = file:read_file(map_path("Map_example2.tmj")),
    #{<<"layers">> := Layers} = Data = json:decode(Bytes),
    Collision = lists:last(Layers),
    Invalid = Data#{<<"layers">> := [Collision#{<<"data">> := [1]}]},
    ?assertEqual({error, map_unavailable}, server_maps:geometry(json:encode(Invalid))).

load_map(Name) ->
    {ok, Bytes} = file:read_file(map_path(Name)),
    {ok, Map} = server_maps:geometry(Bytes),
    Map.

map_path(Name) ->
    filename:join([code:priv_dir(server), "maps", Name]).
