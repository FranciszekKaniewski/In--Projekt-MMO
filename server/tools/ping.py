"""Send a framed JSON ping to the Erlang game server (Python 3)."""

import argparse
import json
import socket
import struct
import sys


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
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4000)
    args = parser.parse_args()
    payload = json.dumps({"type": "ping"}).encode("utf-8")

    try:
        with socket.create_connection((args.host, args.port), timeout=3) as connection:
            connection.sendall(struct.pack("!I", len(payload)) + payload)
            size = struct.unpack("!I", receive_exact(connection, 4))[0]
            if size > 4096:
                raise ValueError("Server reply exceeds 4096 bytes")
            reply = json.loads(receive_exact(connection, size))
            print(json.dumps(reply, ensure_ascii=False))
            return 0 if reply == {"type": "pong"} else 1
    except (OSError, ValueError) as error:
        print(f"Ping failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
