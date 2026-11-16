-module(server_maps).
-export([read/1, geometry/1, move/4, spawn_position/3]).

-define(MAX_MAP_BYTES, 1048576).

read(<<"map_expanded">>) -> read_file("Map_expanded.tmj");
read(<<"map_example2">>) -> read_file("Map_example2.tmj");
read(_) -> {error, map_not_found}.

geometry(Bytes) ->
    try parse_geometry(json:decode(Bytes))
    catch error:_ -> {error, map_unavailable}
    end.

parse_geometry(#{<<"width">> := Columns, <<"height">> := Rows,
                 <<"tilewidth">> := TileWidth, <<"tileheight">> := TileHeight,
                 <<"layers">> := Layers} = Map)
  when is_integer(Columns), Columns > 0, is_integer(Rows), Rows > 0,
       is_integer(TileWidth), TileWidth > 0, is_integer(TileHeight), TileHeight > 0,
       Columns * TileWidth >= 20, Columns * TileWidth =< 4096,
       Rows * TileHeight >= 10, Rows * TileHeight =< 4096, is_list(Layers) ->
    Blocked = collision_mask(Map, Layers, Columns, Rows),
    {ok, #{grid => {Columns, Rows}, tile => {TileWidth, TileHeight}, blocked => Blocked}};
parse_geometry(_) -> {error, map_unavailable}.

collision_mask(Map, Layers, Columns, Rows) ->
    case [Layer || #{<<"name">> := <<"Collision">>} = Layer <- Layers] of
        [] -> <<>>;
        [#{<<"type">> := <<"tilelayer">>, <<"width">> := Columns, <<"height">> := Rows,
           <<"data">> := Gids} = Layer] when is_list(Gids), length(Gids) =:= Columns * Rows ->
            true = lists:all(fun(Key) -> maps:get(Key, Layer, 0) == 0 end,
                             [<<"x">>, <<"y">>, <<"offsetx">>, <<"offsety">>]),
            #{<<"tilesets">> := [#{<<"tilecount">> := TileCount}]} = Map,
            true = lists:all(fun(Gid) -> is_integer(Gid) andalso Gid >= 0 andalso Gid =< TileCount end, Gids),
            list_to_binary([case Gid of 0 -> 0; _ -> 1 end || Gid <- Gids]);
        _ -> error(invalid_collision_layer)
    end.

move(Map, {X, Y}, Destination, {HalfX, HalfY} = HalfSize) ->
    {TargetX, TargetY} = clamp_to_map(Map, Destination, HalfSize),
    case Map of
        #{blocked := <<>>} -> {TargetX, TargetY};
        #{grid := {Columns, _}, tile := {TileWidth, TileHeight}, blocked := Blocked} ->
            %% Sweep one axis at a time so the other axis can still move along the shore.
            NextX = clip_axis(X, TargetX, HalfX, TileWidth, 1,
                              tile_span(Y - HalfY, Y + HalfY, TileHeight), Columns, Blocked),
            NextY = clip_axis(Y, TargetY, HalfY, TileHeight, Columns,
                              tile_span(NextX - HalfX, NextX + HalfX, TileWidth), 1, Blocked),
            {NextX, NextY}
    end.

clip_axis(From, To, _, _, _, _, _, _) when From == To -> To;
clip_axis(From, To, Half, Tile, Stride, Across, AcrossStride, Blocked) ->
    %% Only the leading edge sweeps new tiles. An edge touching a tile does not overlap it.
    {Cells, Edge} = case To > From of
        true -> {tile_span(From + Half, To + Half, Tile), -Half};
        false -> {lists:reverse(tile_span(To - Half, From - Half, Tile)), Tile + Half}
    end,
    case lists:dropwhile(fun(Cell) ->
        not blocked_across(Blocked, Cell * Stride, Across, AcrossStride)
    end, Cells) of
        [Cell | _] -> Cell * Tile + Edge;
        [] -> To
    end.

tile_span(Start, End, Tile) ->
    lists:seq(floor(Start / Tile), ceil(End / Tile) - 1).

blocked_across(Blocked, Offset, Across, Stride) ->
    lists:any(fun(Cell) -> binary:at(Blocked, Offset + Cell * Stride) =/= 0 end, Across).

clamp_to_map(#{grid := {Columns, Rows}, tile := {TileWidth, TileHeight}}, {X, Y}, {HalfX, HalfY}) ->
    {min(max(X, HalfX), Columns * TileWidth - HalfX),
     min(max(Y, HalfY), Rows * TileHeight - HalfY)}.

spawn_position(Map, Saved, HalfSize) ->
    Position = clamp_to_map(Map, Saved, HalfSize),
    case position_open(Map, Position, HalfSize) of
        true -> {ok, Position};
        false -> nearest_land(Map, Position, HalfSize)
    end.

position_open(#{blocked := <<>>}, _, _) -> true;
position_open(#{grid := {Columns, _}, tile := {TileWidth, TileHeight}, blocked := Blocked},
              {X, Y}, {HalfX, HalfY}) ->
    SpanX = tile_span(X - HalfX, X + HalfX, TileWidth),
    SpanY = tile_span(Y - HalfY, Y + HalfY, TileHeight),
    not lists:any(fun(Row) -> blocked_across(Blocked, Row * Columns, SpanX, 1) end, SpanY).

nearest_land(#{grid := {Columns, Rows}, tile := {TileWidth, TileHeight}, blocked := Blocked} = Map,
             {X, Y}, HalfSize) ->
    %% Old saves can point into water. Pick the closest tile centre that fits the whole hitbox.
    Find = fun(Index, Best) ->
        case binary:at(Blocked, Index) of
            0 ->
                {CX, CY} = Candidate = clamp_to_map(Map, {(Index rem Columns + 0.5) * TileWidth,
                                                          (Index div Columns + 0.5) * TileHeight}, HalfSize),
                Distance = (CX - X) * (CX - X) + (CY - Y) * (CY - Y),
                case (Best =:= none orelse Distance < element(1, Best)) andalso
                     position_open(Map, Candidate, HalfSize) of
                    true -> {Distance, Candidate};
                    false -> Best
                end;
            _ -> Best
        end
    end,
    case lists:foldl(Find, none, lists:seq(0, Columns * Rows - 1)) of
        none -> {error, map_unavailable};
        {_, Position} -> {ok, Position}
    end.

read_file(Name) ->
    {ok, Path} = application:get_env(server, maps_dir),
    case file:open(filename:join(Path, Name), [read, binary, raw]) of
        {ok, File} ->
            try file:read(File, ?MAX_MAP_BYTES + 1) of
                {ok, Bytes} when byte_size(Bytes) > 0, byte_size(Bytes) =< ?MAX_MAP_BYTES ->
                    {ok, Bytes};
                _ -> {error, map_unavailable}
            after
                file:close(File)
            end;
        {error, _} -> {error, map_unavailable}
    end.
