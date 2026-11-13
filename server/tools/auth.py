"""Register or log in to the game server over verified TLS."""

import argparse
import getpass
import json
import sys
import unicodedata

from ping import add_connection_arguments, connect, request


PASSWORD_REQUIREMENTS = (
    "Hasło musi mieć 8–128 znaków, co najmniej jedną wielką literę "
    "i jeden znak specjalny (np. !, @, #). Maksymalnie 512 bajtów UTF-8."
)


def password_error(password):
    if not 8 <= len(password) <= 128 or len(password.encode("utf-8")) > 512:
        return "Hasło musi mieć od 8 do 128 znaków i maksymalnie 512 bajtów UTF-8."
    categories = [unicodedata.category(char) for char in password]
    if "Lu" not in categories:
        return "Hasło musi zawierać co najmniej jedną wielką literę."
    if not any(category[0] in {"P", "S"} for category in categories):
        return "Hasło musi zawierać co najmniej jeden znak specjalny (np. !, @, #)."
    return None


def show(reply):
    print(json.dumps(reply, ensure_ascii=False, indent=2))
    if reply.get("code") == "invalid_registration":
        print("Login musi mieć 3–32 znaki, bez białych i sterujących znaków.", file=sys.stderr)
        print(PASSWORD_REQUIREMENTS, file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["register", "login"])
    parser.add_argument("username")
    add_connection_arguments(parser)
    args = parser.parse_args()
    if args.action == "register":
        print(PASSWORD_REQUIREMENTS)
    password = getpass.getpass("Hasło: ")
    if args.action == "register":
        error = password_error(password)
        if error:
            print(error, file=sys.stderr)
            return 1
        if password != getpass.getpass("Powtórz hasło: "):
            print("Hasła są różne.", file=sys.stderr)
            return 1
    try:
        with connect(args.host, args.port, args.cafile) as connection:
            reply = request(connection, {"type": args.action,
                                         "username": args.username, "password": password})
            del password
            show(reply)
            if reply.get("type") == "error":
                return 1
            if args.action == "register":
                return 0
            print("Polecenia: list [after_id], select ID, logout")
            while True:
                command = input("> ").split()
                if command == ["logout"]:
                    show(request(connection, {"type": "logout"}))
                    return 0
                if command and command[0] == "list" and len(command) <= 2:
                    message = {"type": "list_characters"}
                    if len(command) == 2 and command[1].isdigit():
                        message["after_id"] = int(command[1])
                    elif len(command) == 2:
                        continue
                elif len(command) == 2 and command[0] == "select" and command[1].isdigit():
                    message = {"type": "select_character", "character_id": int(command[1])}
                else:
                    print("Użyj: list [after_id], select ID lub logout")
                    continue
                show(request(connection, message))
    except (OSError, ValueError, EOFError, KeyboardInterrupt) as error:
        print(f"Połączenie zakończone: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
