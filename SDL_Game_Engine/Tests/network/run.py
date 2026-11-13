"""Exercise the native game client against local TLS peers (no game database)."""

import concurrent.futures
import json
from pathlib import Path
import socket
import ssl
import struct
import subprocess
import tempfile
import threading
import time


ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT.parent / "server/test/fixtures"
CLIENT = ROOT / "build/tests/network/client.exe"
CHARACTER_ID = 9007199254741009
MAP = ROOT.parent / "server/priv/maps/Map_expanded.tmj"
CHARACTER = {"id": CHARACTER_ID, "name": "Żółw", "map_key": "map_expanded",
             "position_x": 1350, "position_y": 1320, "facing": 4, "level": 1, "experience": 0}


def receive_exact(connection, count):
    result = bytearray()
    while len(result) < count:
        part = connection.recv(count - len(result))
        if not part:
            raise EOFError
        result.extend(part)
    return bytes(result)


def receive(connection):
    count = struct.unpack("!I", receive_exact(connection, 4))[0]
    assert count <= 4096
    return json.loads(receive_exact(connection, count))


def send(connection, message):
    body = json.dumps(message, ensure_ascii=False).encode("utf-8")
    frame = struct.pack("!I", len(body)) + body
    # Split both the length header and multibyte UTF-8 names across TLS records.
    for offset in range(0, len(frame), 7):
        connection.sendall(frame[offset:offset + 7])


def run_case(mode, ca_file, tls_version):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(FIXTURES / "server.crt", FIXTURES / "server.key")
    context.minimum_version = context.maximum_version = tls_version
    if tls_version == ssl.TLSVersion.TLSv1_3:
        context.num_tickets = 2
    host = "127.0.0.2" if mode == "hostname" else "127.0.0.1"
    stop = threading.Event()
    requests = []
    errors = []
    lock = threading.Lock()

    def peer(raw, number):
        try:
            with context.wrap_socket(raw, server_side=True) as connection:
                while not stop.is_set():
                    message = receive(connection)
                    with lock:
                        requests.append((number, message["type"]))
                    kind = message["type"]
                    if mode == "cancel" and number == 0:
                        time.sleep(0.5)
                        send(connection, {"type": "error", "code": "invalid_credentials"})
                        return
                    if mode == "timeout":
                        stop.wait(1.5)
                        return
                    if mode == "oversized":
                        connection.sendall(struct.pack("!I", 4097))
                        return
                    if mode == "invalid_credentials":
                        send(connection, {"type": "error", "code": "invalid_credentials"})
                        return
                    if kind == "login":
                        assert message["username"] == "tester" and message["password"] == "Pass123!"
                        send(connection, {"type": "login_ok", "user_id": 1, "username": "tester", "expires_in_ms": 28800000})
                    elif kind == "list_characters":
                        assert message["after_id"] == 0
                        send(connection, {"type": "characters", "characters": [{"id": CHARACTER_ID, "name": "Żółw"}], "has_more": False})
                    elif kind == "select_character":
                        assert message["character_id"] == CHARACTER_ID
                        send(connection, {"type": "character_selected", "character": CHARACTER})
                        if mode == "expired":
                            time.sleep(0.1)
                            send(connection, {"type": "error", "code": "session_expired"})
                    elif kind == "get_map" and mode in {"map", "map_wrong_key", "map_truncated"}:
                        assert message == {"type": "get_map"}
                        data = MAP.read_bytes()
                        key = "map_example2" if mode == "map_wrong_key" else "map_expanded"
                        send(connection, {"type": "map_data", "map_key": key, "size": len(data)})
                        if mode == "map_wrong_key":
                            return
                        if mode == "map_truncated":
                            data = data[:4096]
                        for offset in range(0, len(data), 4096):
                            chunk = data[offset:offset + 4096]
                            connection.sendall(struct.pack("!I", len(chunk)) + chunk)
                        if mode == "map_truncated":
                            return
                    elif kind == "enter_world" and mode == "world":
                        send(connection, {"type": "world_joined", "character": dict(CHARACTER, position_x=1352)})
                    elif kind == "move_input" and mode == "world":
                        seq = message["seq"]
                        assert seq in (1, 2)
                        assert message == {"type": "move_input", "seq": seq, "dx": int(seq == 1), "dy": 0}
                        send(connection, {"type": "world_state", "map_key": "map_expanded", "tick": 9 + seq,
                                          "players": [
                            dict(CHARACTER, position_x=1352 + 8 * seq, facing=2, moving=seq == 1,
                                 input_ack=seq, vx=160 if seq == 1 else 0, vy=0),
                            dict(CHARACTER, id=CHARACTER_ID + 1, name="Peer", moving=False,
                                 input_ack=0, vx=0, vy=0)]})
                    elif kind == "leave_world" and mode == "world":
                        send(connection, {"type": "world_left"})
                    else:
                        raise AssertionError("Unexpected request type")
        except (EOFError, ssl.SSLError, ConnectionError, TimeoutError):
            pass
        except Exception as error:
            errors.append(error)
        finally:
            raw.close()

    with socket.socket() as listener, concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        listener.bind((host, 0))
        listener.listen()
        listener.settimeout(0.1)

        def accept():
            count = 0
            while not stop.is_set():
                try:
                    raw, _ = listener.accept()
                except TimeoutError:
                    continue
                raw.settimeout(3)
                pool.submit(peer, raw, count)
                count += 1

        accept_thread = threading.Thread(target=accept)
        accept_thread.start()
        try:
            result = subprocess.run([str(CLIENT), str(listener.getsockname()[1]), host,
                                     str(ca_file), mode], cwd=ROOT, text=True, capture_output=True,
                                    encoding="utf-8", timeout=12)
        finally:
            stop.set()
            accept_thread.join(timeout=2)
    assert result.returncode == 0, result.stdout + result.stderr
    assert not errors, errors
    if mode in {"untrusted", "hostname", "missing_ca"}:
        assert not requests, "Credentials were sent before certificate validation"
    if mode in {"tls12", "tls13", "expired", "unicode_path"}:
        assert [kind for _, kind in requests] == ["login", "list_characters", "select_character"]
        assert {number for number, _ in requests} == {0}, "Scenes would require a new login connection"
    if mode.startswith("map"):
        assert [kind for _, kind in requests] == ["login", "list_characters", "select_character", "get_map"]
        assert {number for number, _ in requests} == {0}
    if mode == "world":
        assert [kind for _, kind in requests] == ["login", "list_characters", "select_character",
            "enter_world", "move_input", "move_input", "list_characters", "leave_world", "list_characters"]
    print(result.stdout.strip())


def main():
    with tempfile.TemporaryDirectory(dir=CLIENT.parent) as directory:
        wrong_ca = Path(directory) / "wrong-ca.crt"
        system_root = next(cert for cert, encoding, _ in ssl.enum_certificates("ROOT") if encoding == "x509_asn")
        wrong_ca.write_text(ssl.DER_cert_to_PEM_cert(system_root), encoding="ascii")
        unicode_ca = Path(directory) / "Zażółć 🔐" / "zaufany-Żółw.crt"
        unicode_ca.parent.mkdir()
        unicode_ca.write_bytes((FIXTURES / "ca.crt").read_bytes())
        for mode in ["tls12", "tls13", "unicode_path", "invalid_credentials", "hostname", "untrusted", "oversized", "expired", "cancel", "timeout", "missing_ca", "map", "map_wrong_key", "map_truncated", "world"]:
            ca = wrong_ca if mode == "untrusted" else Path(directory) / "missing.crt" if mode == "missing_ca" else FIXTURES / "ca.crt"
            if mode == "unicode_path":
                ca = unicode_ca
            version = ssl.TLSVersion.TLSv1_2 if mode == "tls12" else ssl.TLSVersion.TLSv1_3
            run_case(mode, ca, version)
    print("15 native TLS scenarios passed.")


if __name__ == "__main__":
    main()
