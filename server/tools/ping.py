"""Send a framed JSON ping over verified TLS (Python 3)."""

import argparse
import json
import socket
import ssl
import struct
import sys
from pathlib import Path


SERVER_DIR = Path(__file__).resolve().parent.parent


def connection_settings():
    path = SERVER_DIR / ".env"
    keys = {"GAME_HOST", "GAME_PORT", "GAME_CAFILE", "GAME_TIMEOUT_MS"}
    values = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        name, separator, value = line.strip().partition("=")
        name = name.strip()
        if not separator or name not in keys:
            continue
        value = value.strip()
        if len(value) >= 2 and value[0] in "\"'" and value[-1] == value[0]:
            value = value[1:-1]
        values[name] = value

    for key in sorted(keys):
        if not values.get(key):
            raise ValueError(f"Brak {key} w {path}")
    return (values["GAME_HOST"], number(values, "GAME_PORT", 65535),
            path.parent / values["GAME_CAFILE"], number(values, "GAME_TIMEOUT_MS", 30000) / 1000)


def number(values, key, maximum):
    value = values[key]
    if value.isascii() and value.isdecimal() and 1 <= int(value) <= maximum:
        return int(value)
    raise ValueError(f"{key} w .env musi być liczbą całkowitą od 1 do {maximum}")


def connect(host, port, cafile, timeout):
    context = ssl.create_default_context(cafile=str(cafile))
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    raw = socket.create_connection((host, port), timeout=timeout)
    try:
        return context.wrap_socket(raw, server_hostname=host)
    except BaseException:
        raw.close()
        raise


def request(connection, message):
    payload = json.dumps(message, ensure_ascii=False).encode("utf-8")
    if len(payload) > 4096:
        raise ValueError("Request exceeds 4096 bytes")
    connection.sendall(struct.pack("!I", len(payload)) + payload)
    size = struct.unpack("!I", receive_exact(connection, 4))[0]
    if size > 4096:
        raise ValueError("Server reply exceeds 4096 bytes")
    return json.loads(receive_exact(connection, size))


def receive_exact(connection, size):
    result = bytearray()
    while len(result) < size:
        chunk = connection.recv(size - len(result))
        if not chunk:
            raise ConnectionError("Server closed the connection before replying")
        result.extend(chunk)
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()

    try:
        with connect(*connection_settings()) as connection:
            reply = request(connection, {"type": "ping"})
            print(json.dumps(reply, ensure_ascii=False))
            return 0 if reply == {"type": "pong"} else 1
    except (OSError, ValueError) as error:
        print(f"Ping failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
