"""Utwórz n kont testowych i po jednej postaci w bazie z server/.env."""

import argparse
import getpass
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


TOOLS_DIR = Path(__file__).resolve().parents[1]
SERVER_DIR = TOOLS_DIR.parent
sys.path.insert(0, str(TOOLS_DIR))

from auth import password_error


# Use OTP's existing Argon2id implementation and database configuration.
SEED = r"""
process_flag(sensitive, true),
try
    {ok, _} = application:ensure_all_started(crypto),
    {ok, _} = application:ensure_all_started(jargon),
    {ok, _} = application:ensure_all_started(epgsql),
    Password = unicode:characters_to_binary(os:getenv("GAME_TEST_PASSWORD")),
    Prefix = os:getenv("GAME_TEST_PREFIX"),
    Count = list_to_integer(os:getenv("GAME_TEST_COUNT")),
    Sql = <<"WITH account AS ("
            "INSERT INTO public.users (username, password_hash) VALUES ($1, $2) RETURNING id"
            ") INSERT INTO public.characters (user_id, name) SELECT id, $1 FROM account">>,
    {ok, Connection} = epgsql:connect(server_config:db_options()),
    Result = try
        epgsql:with_transaction(Connection, fun(C) ->
            lists:foreach(fun(Index) ->
                Suffix = integer_to_list(Index),
                Padding = lists:duplicate(max(0, 3 - length(Suffix)), $0),
                Name = iolist_to_binary([Prefix, $_, Padding, Suffix]),
                {ok, Hash} = server_password:hash(Password),
                case epgsql:equery(C, Sql, [Name, Hash]) of
                    {ok, 1} -> ok;
                    {error, {error, _, _, unique_violation, _, _}} ->
                        error({account_or_character_exists, Name});
                    {error, {error, _, _, Code, _, _}} ->
                        error({insert_failed, Name, Code});
                    _ -> error({insert_failed, Name})
                end
            end, lists:seq(1, Count))
        end, #{reraise => false, ensure_committed => true})
    after
        epgsql:close(Connection)
    end,
    case Result of
        ok -> halt(0);
        {rollback, Reason} -> error(Reason)
    end
catch
    error:{account_or_character_exists, Existing} ->
        io:format(standard_error, "Konto lub postac ~s juz istnieje. Uzyj innego --prefix.~n", [Existing]),
        halt(1);
    Class:Failure ->
        io:format(standard_error, "Nie udalo sie utworzyc danych testowych (~p:~p).~n", [Class, Failure]),
        halt(1)
end.
"""


def account_parser(description):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("n", type=int, help="liczba kont testowych")
    parser.add_argument("--prefix", default="test_user", help="prefiks loginów (domyślnie test_user)")
    return parser


def parse_args(parser):
    args = parser.parse_args()
    if args.n < 1:
        parser.error("n musi być dodatnie")
    if not re.fullmatch(r"[a-z][a-z0-9_]*", args.prefix) or len(username(args.prefix, args.n)) > 32:
        parser.error("prefiks musi zaczynać się małą literą ASCII i mieścić z numerem w 32 znakach")
    return args


def username(prefix, index):
    return f"{prefix}_{index:03d}"


def test_password():
    password = os.environ.get("GAME_TEST_PASSWORD") or getpass.getpass("Hasło kont testowych: ")
    error = password_error(password)
    if error:
        raise ValueError(error)
    return password


def main():
    args = parse_args(account_parser(__doc__))
    try:
        password = test_password()
        rebar3 = shutil.which("rebar3")
        erl = shutil.which("erl")
        if not rebar3 or not erl:
            raise RuntimeError("Wymagane są rebar3 i Erlang w PATH.")
        subprocess.run([rebar3, "compile"], cwd=SERVER_DIR, check=True)
        libraries = SERVER_DIR.glob("_build/default/lib/*/ebin")
        paths = [str(path.relative_to(SERVER_DIR)) for path in sorted(libraries)]
        env = {
            **os.environ,
            "GAME_TEST_PASSWORD": password,
            "GAME_TEST_PREFIX": args.prefix,
            "GAME_TEST_COUNT": str(args.n),
        }
        subprocess.run(
            [erl, "-noshell", "-pa", *paths, "-eval", SEED],
            cwd=SERVER_DIR, env=env, check=True,
        )
    except subprocess.CalledProcessError as error:
        print(f"Generator zakończył się kodem {error.returncode}.", file=sys.stderr)
        return 1
    except (OSError, EOFError, RuntimeError, ValueError) as error:
        print(f"Błąd generatora: {error}", file=sys.stderr)
        return 1
    print(f"Utworzono {args.n} kont z jedną postacią: "
          f"{username(args.prefix, 1)} – {username(args.prefix, args.n)}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
