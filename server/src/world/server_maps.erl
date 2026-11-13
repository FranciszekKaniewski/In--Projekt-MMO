-module(server_maps).
-export([read/1]).

-define(MAX_MAP_BYTES, 1048576).

read(<<"map_expanded">>) -> read_file("Map_expanded.tmj");
read(<<"map_example2">>) -> read_file("Map_example2.tmj");
read(_) -> {error, map_not_found}.

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
