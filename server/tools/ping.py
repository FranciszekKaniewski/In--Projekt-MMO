"""Send a framed JSON ping over verified TLS (Python 3)."""

import argparse
import json
import socket
import ssl
import struct
import sys
from pathlib import Path


DEFAULT_CA = Path(__file__).resolve().parent.parent / "priv/tls/ca.crt"


def connect(host, port, cafile):
    context = ssl.create_default_context(cafile=str(cafile))
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    raw = socket.create_connection((host, port), timeout=5)
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
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4000)
    parser.add_argument("--cafile", type=Path, default=DEFAULT_CA)
    args = parser.parse_args()

    try:
        with connect(args.host, args.port, args.cafile) as connection:
            reply = request(connection, {"type": "ping"})
            print(json.dumps(reply, ensure_ascii=False))
            return 0 if reply == {"type": "pong"} else 1
    except (OSError, ValueError) as error:
        print(f"Ping failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
