-module(server_accounts).
-export([create/2, find/1]).

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
