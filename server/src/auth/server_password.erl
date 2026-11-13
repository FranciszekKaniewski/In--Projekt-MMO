-module(server_password).
-export([hash/1, verify/2, valid_username/1, valid_password/1, valid_login_password/1]).

hash(Password) ->
    case jargon:hash(Password, crypto:strong_rand_bytes(16), argon2id, 2, 19456, 1, 32) of
        {ok, _, Encoded} -> {ok, Encoded};
        {error, _} -> {error, service_unavailable}
    end.

verify(Password, Encoded) ->
    case jargon:verify(Encoded, Password) of
        {ok, Match} -> Match;
        {error, _} -> false
    end.

valid_username(Value) when is_binary(Value), byte_size(Value) =< 128 ->
    case unicode:characters_to_list(Value) of
        Chars when is_list(Chars), length(Chars) >= 3, length(Chars) =< 32 ->
            re:run(Value, "[\\p{Z}\\p{Cc}]", [unicode, {capture, none}]) =:= nomatch;
        _ -> false
    end;
valid_username(_) -> false.

%% Registration policy: 8-128 Unicode characters, uppercase, punctuation/symbol.
valid_password(Value) when is_binary(Value), byte_size(Value) =< 512 ->
    case unicode:characters_to_list(Value) of
        Chars when is_list(Chars), length(Chars) >= 8, length(Chars) =< 128 ->
            re:run(Value, "\\p{Lu}", [unicode, {capture, none}]) =:= match andalso
            re:run(Value, "[\\p{P}\\p{S}]", [unicode, {capture, none}]) =:= match;
        _ -> false
    end;
valid_password(_) -> false.

%% Login verifies an existing hash, including passwords predating the policy.
valid_login_password(Value) when is_binary(Value), byte_size(Value) =< 512 ->
    case unicode:characters_to_list(Value) of
        Chars when is_list(Chars) -> length(Chars) >= 1 andalso length(Chars) =< 128;
        _ -> false
    end;
valid_login_password(_) -> false.
