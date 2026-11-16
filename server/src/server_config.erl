-module(server_config).
-export([load/0, db_options/0]).

%% OTP tests can inject their listener and database settings before starting the app.
load() ->
    Directory = filename:absname("."),
    Dotenv = dotenv(),
    Overrides = application:get_env(server, connection_overrides, #{}),
    Readers = #{
        db_options => fun() -> db_options(Dotenv) end,
        ip => fun() -> bind_ip(Dotenv) end,
        port => fun() -> port("GAME_PORT", Dotenv) end,
        tls_certfile => fun() -> filename:absname(required("GAME_TLS_CERTFILE", Dotenv), Directory) end,
        tls_keyfile => fun() -> filename:absname(required("GAME_TLS_KEYFILE", Dotenv), Directory) end
    },
    Connection = maps:map(fun(Key, Read) ->
        case maps:find(Key, Overrides) of
            {ok, Value} -> Value;
            error -> Read()
        end
    end, Readers),
    MapDir = maps:get("GAME_MAP_DIR", Dotenv, filename:join(code:priv_dir(server), "maps")),
    maps:foreach(fun(Key, Value) -> ok = application:set_env(server, Key, Value) end,
                 Connection#{maps_dir => filename:absname(MapDir, Directory)}).

db_options() -> db_options(dotenv()).

db_options(Dotenv) ->
    Password = required("POSTGRES_PASSWORD", Dotenv),
    #{
        host => required("POSTGRES_HOST", Dotenv),
        port => port("POSTGRES_PORT", Dotenv),
        database => required("POSTGRES_DB", Dotenv),
        username => required("POSTGRES_USER", Dotenv),
        password => fun() -> Password end,
        timeout => 3000
    }.

required(Name, Dotenv) ->
    case maps:get(Name, Dotenv, "") of
        "" -> error({missing_dotenv_setting, Name});
        Value -> Value
    end.

port(Name, Dotenv) ->
    case string:to_integer(required(Name, Dotenv)) of
        {Number, []} when Number >= 1, Number =< 65535 -> Number;
        _ -> error({invalid_dotenv_setting, Name})
    end.

bind_ip(Dotenv) ->
    Name = "GAME_BIND_IP",
    case inet:parse_ipv4strict_address(required(Name, Dotenv)) of
        {ok, IP} -> IP;
        _ -> error({invalid_dotenv_setting, Name})
    end.

dotenv() ->
    case file:read_file(".env") of
        {ok, <<239, 187, 191, Bytes/binary>>} -> parse_env(Bytes);
        {ok, Bytes} -> parse_env(Bytes);
        {error, enoent} -> #{};
        {error, Reason} -> error({cannot_read_dotenv, Reason})
    end.

parse_env(Bytes) ->
    case unicode:characters_to_list(Bytes) of
        Characters when is_list(Characters) ->
            lists:foldl(fun parse_line/2, #{}, string:split(Characters, "\n", all));
        _ -> error(invalid_dotenv_encoding)
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
