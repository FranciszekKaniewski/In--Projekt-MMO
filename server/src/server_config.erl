-module(server_config).
-export([load/0]).

%% Process environment wins over .env. Never print configuration with secrets.
load() ->
    Dotenv = dotenv(),
    set_default(db_options, #{
        host => env("POSTGRES_HOST", "127.0.0.1", Dotenv),
        port => list_to_integer(env("POSTGRES_PORT", "5432", Dotenv)),
        database => env("POSTGRES_DB", "sdl_game", Dotenv),
        username => env("POSTGRES_USER", "game_dev", Dotenv),
        password => fun() -> env("POSTGRES_PASSWORD", "", Dotenv) end,
        timeout => 3000
    }),
    set_default(tls_certfile, env("GAME_TLS_CERTFILE", "priv/tls/server.crt", Dotenv)),
    set_default(tls_keyfile, env("GAME_TLS_KEYFILE", "priv/tls/server.key", Dotenv)),
    ok.

set_default(Key, Value) ->
    case application:get_env(server, Key) of
        undefined -> application:set_env(server, Key, Value);
        {ok, _} -> ok
    end.

env(Name, Default, Dotenv) ->
    case os:getenv(Name) of false -> maps:get(Name, Dotenv, Default); Value -> Value end.

dotenv() ->
    case file:read_file(".env") of
        {ok, Bytes} ->
            lists:foldl(fun parse_line/2, #{}, string:split(binary_to_list(Bytes), "\n", all));
        {error, enoent} -> #{}
    end.

parse_line(Line, Values) ->
    case string:trim(Line) of
        [] -> Values;
        [$# | _] -> Values;
        Clean ->
            case string:split(Clean, "=") of
                [Name, Value] -> Values#{string:trim(Name) => unquote(string:trim(Value))};
                _ -> Values
            end
    end.

unquote([$" | Rest] = Value) -> unquote_end(Rest, $", Value);
unquote([$' | Rest] = Value) -> unquote_end(Rest, $', Value);
unquote(Value) -> Value.

unquote_end(Rest, Quote, Original) ->
    case lists:reverse(Rest) of [Quote | Tail] -> lists:reverse(Tail); _ -> Original end.
