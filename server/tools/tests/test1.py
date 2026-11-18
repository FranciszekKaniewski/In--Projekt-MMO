"""Symuluj graczy poruszających się i piszących na czacie przez m sekund."""

import asyncio
from collections import Counter, deque
from contextlib import nullcontext, suppress
import json
import multiprocessing
from multiprocessing.connection import wait
import random
import signal
import ssl
import struct
import sys

from createTestUsers import account_parser, parse_args, test_password, username
from ping import connection_settings


MAX_FRAME_SIZE = 4096
MAX_WORLD_STATE_SIZE = 128 * 1024
MAX_MAP_SIZE = 1024 * 1024
LOGINS_PER_MINUTE = 30
MOVE_INTERVAL = 1 / 30
MAX_UNACKNOWLEDGED_INPUTS = 8
STOP = (0, 0)
DIRECTIONS = ((1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1))
CHAT_LINES = (
    "Cześć, ktoś jest w okolicy?",
    "Idę zobaczyć, co jest dalej.",
    "Chwila, rozglądam się.",
    "Ktoś chce się przejść?",
    "Spotkajmy się za chwilę.",
    "Wracam w stronę startu.",
    "Gdzie teraz idziecie?",
    "Ładna okolica!",
)


class Client:
    def __init__(self, reader, writer, timeout, name):
        self.reader = reader
        self.writer = writer
        self.timeout = timeout
        self.name = name
        self.character_id = None
        self.position = None
        self.moved = False
        self.sequence = 0
        self.ack = 0
        self.sent_chat = Counter()
        self.received_chat = Counter()
        self.updated = asyncio.Event()

    async def send(self, kind, **fields):
        payload = json.dumps({"type": kind, **fields}, ensure_ascii=False).encode("utf-8")
        self.writer.write(struct.pack("!I", len(payload)) + payload)
        await self.writer.drain()

    async def frame(self, max_size=MAX_FRAME_SIZE):
        size, = struct.unpack("!I", await self.reader.readexactly(4))
        if not 0 < size <= max_size:
            raise RuntimeError(f"{self.name}: nieprawidłowy rozmiar ramki {size}")
        return await self.reader.readexactly(size)

    async def receive(self, max_size=MAX_FRAME_SIZE):
        try:
            frame = await asyncio.wait_for(self.frame(max_size), self.timeout)
        except asyncio.TimeoutError:
            raise RuntimeError(f"{self.name}: serwer nie przysłał wiadomości w terminie") from None
        message = json.loads(frame)
        if len(frame) > MAX_FRAME_SIZE and message["type"] != "world_state":
            raise RuntimeError(f"{self.name}: zbyt duża ramka {message['type']}")
        if message["type"] in {"error", "move_error", "chat_error"}:
            code = message["code"]
            if code == "rate_limited":
                raise RuntimeError(
                    f"{self.name}: rate_limited. Sprawdź application:get_env(server, login_ip_limit). "
                    "w powłoce Erlanga i dopasuj --logins-per-minute. "
                    "Jeśli limity się zgadzają, odczekaj minutę po poprzednich próbach."
                )
            raise RuntimeError(f"{self.name}: {code}")
        return message

    async def expect(self, kind):
        message = await self.receive()
        if message["type"] != kind:
            raise RuntimeError(f"{self.name}: oczekiwano {kind}, odebrano {message['type']}")
        return message

    async def prepare(self, password):
        await self.send("login", username=self.name, password=password)
        await self.expect("login_ok")
        await self.send("list_characters")
        reply = await self.expect("characters")
        characters = reply["characters"]
        if reply["has_more"] or len(characters) != 1 or characters[0]["name"] != self.name:
            raise RuntimeError(f"{self.name}: oczekiwano jednej postaci o nazwie konta")
        self.character_id = characters[0]["id"]
        await self.send("select_character", character_id=self.character_id)
        selected = (await self.expect("character_selected"))["character"]
        await self.send("get_map")
        header = await self.expect("map_data")
        remaining = header["size"]
        if header["map_key"] != selected["map_key"] or not 0 < remaining <= MAX_MAP_SIZE:
            raise RuntimeError(f"{self.name}: nieprawidłowy nagłówek mapy")
        while remaining:
            chunk = await asyncio.wait_for(self.frame(), self.timeout)
            if len(chunk) != min(remaining, MAX_FRAME_SIZE):
                raise RuntimeError(f"{self.name}: nieprawidłowa ramka mapy")
            remaining -= len(chunk)

    async def enter(self):
        await self.send("enter_world")
        character = (await self.expect("world_joined"))["character"]
        if character["id"] != self.character_id:
            raise RuntimeError(f"{self.name}: serwer wczytał inną postać")
        self.position = character["position_x"], character["position_y"]

    async def watch(self, character_ids):
        while True:
            message = await self.receive(MAX_WORLD_STATE_SIZE)
            kind = message["type"]
            if kind == "logout_ok":
                return
            if kind == "world_state":
                for player in message["players"]:
                    if player["id"] == self.character_id:
                        position = player["position_x"], player["position_y"]
                        self.moved |= position != self.position
                        self.position = position
                        self.ack = player["input_ack"]
                        self.updated.set()
                        break
            elif kind == "chat_message" and message["character_id"] in character_ids:
                self.received_chat[message["character_id"], message["name"], message["text"]] += 1
                self.updated.set()

    async def move(self, direction):
        self.sequence += 1
        dx, dy = direction
        await self.send("move_input", seq=self.sequence, dx=dx, dy=dy)

    async def play(self, start, duration, direction):
        clock = asyncio.get_running_loop().time
        deadline = start + duration
        direction_start = start
        turn_at = start + random.uniform(0.7, 2.0)
        turn_position = self.position
        chat_window = min(duration, 6)
        chat_at = start + random.uniform(0.2, 0.65) * chat_window
        tick_at = start
        while (now := clock()) < deadline:
            blocked = direction != STOP and now - direction_start >= 0.35 and self.position == turn_position
            if now >= turn_at or blocked:
                choices = DIRECTIONS if blocked else (*DIRECTIONS, STOP)
                direction = random.choice([choice for choice in choices if choice != direction])
                direction_start, turn_position = now, self.position
                turn_at = now + random.uniform(0.7, 2.0)
            if now >= chat_at:
                text = random.choice(CHAT_LINES)
                await self.send("chat_send", text=text)
                self.sent_chat[self.character_id, self.name, text] += 1
                chat_at = now + random.uniform(3.0, 7.0)
            # The server consumes two movement inputs per 30 Hz world tick.
            available = MAX_UNACKNOWLEDGED_INPUTS - (self.sequence - self.ack)
            for _ in range(min(2, available)):
                await self.move(direction)
            tick_at = max(tick_at + MOVE_INTERVAL, now)
            await asyncio.sleep(max(0, min(tick_at, deadline) - clock()))
        await self.move(STOP)

    async def wait_for_delivery(self, expected_chat):
        while self.ack < self.sequence or expected_chat - self.received_chat:
            self.updated.clear()
            await self.updated.wait()

    async def close(self):
        self.writer.close()
        with suppress(OSError, asyncio.TimeoutError):
            await asyncio.wait_for(self.writer.wait_closed(), self.timeout)


async def exchange(peer, phase, value=None):
    if peer is None:
        return [value]
    peer.send((phase, value))
    return await asyncio.to_thread(peer.recv)


async def finish_batch(clients, activities, readers, timeout, peer):
    await asyncio.gather(*activities)
    sent_chat = Counter()
    for client in clients:
        sent_chat.update(client.sent_chat)
    expected_chat = Counter()
    for counts in await exchange(peer, "played", sent_chat):
        expected_chat.update(counts)
    for client in clients:
        await asyncio.wait_for(client.wait_for_delivery(expected_chat), timeout)
    for client in clients:
        await client.send("logout")
    await asyncio.wait_for(asyncio.gather(*readers), timeout)
    stationary = [client.name for client in clients if not client.moved]
    if stationary:
        raise RuntimeError(f"Postacie nie zmieniły pozycji: {', '.join(stationary)}")


async def run_batch(settings, names, password, duration, login_times, peer, login_lock):
    host, port, cafile, timeout = settings
    context = ssl.create_default_context(cafile=str(cafile))
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    clock = asyncio.get_running_loop().time
    clients, tasks = [], []
    try:
        for index, name in enumerate(names, start=1):
            if len(login_times) == login_times.maxlen and (wait := login_times[0] + 60 - clock()) > 0:
                if wait >= 1:
                    print(f"Czekam {wait:.0f} s na odnowienie limitu logowań.", flush=True)
                await asyncio.sleep(wait)
            with login_lock:
                reader, writer = await asyncio.wait_for(
                    asyncio.open_connection(host, port, ssl=context, ssl_handshake_timeout=timeout), timeout
                )
                client = Client(reader, writer, timeout, name)
                clients.append(client)
                await client.prepare(password)
                login_times.append(clock())
            if peer is None and len(names) > 50 and (index % 50 == 0 or index == len(names)):
                print(f"Zalogowano i przygotowano {index}/{len(names)} postaci.", flush=True)
        # Allow login-rate pauses to finish before filling the map with active characters.
        own_ids = {client.character_id for client in clients}
        character_ids = set().union(*await exchange(peer, "prepared", own_ids))
        readers = []
        for client in clients:
            await client.enter()
            reader = asyncio.create_task(client.watch(character_ids))
            readers.append(reader)
            tasks.append(reader)
        await exchange(peer, "entered")
        if peer is None:
            print(f"W grze: {len(clients)} postaci przez {duration} s.", flush=True)
        directions = random.sample(DIRECTIONS, len(DIRECTIONS))
        start = clock()
        activities = [
            asyncio.create_task(client.play(start, duration, directions[index % len(directions)]))
            for index, client in enumerate(clients)
        ]
        tasks.extend(activities)
        tasks.append(asyncio.create_task(finish_batch(clients, activities, readers, timeout, peer)))
        await asyncio.gather(*tasks)
    finally:
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        await asyncio.gather(*(client.close() for client in clients))


async def run(settings, args, password, index=0, workers=1, peer=None, login_lock=None):
    # The workers share one source-IP budget across the whole run.
    budget, remainder = divmod(args.logins_per_minute, workers)
    login_times = deque(maxlen=budget + (index < remainder))
    lock = login_lock if login_lock is not None else nullcontext()
    for start in range(0, args.n, args.batch_size):
        end = min(start + args.batch_size, args.n)
        names = [username(args.prefix, number) for number in range(start + index + 1, end + 1, workers)]
        await run_batch(settings, names, password, args.m, login_times, peer, lock)
        await exchange(peer, "finished")
        if peer is None:
            print(f"OK {end}/{args.n} – potwierdzono ruch i dostarczenie czatu.", flush=True)


def worker_entry(settings, args, password, index, workers, peer, login_lock):
    signal.signal(signal.SIGINT, signal.SIG_IGN)

    async def execute():
        try:
            await run(settings, args, password, index, workers, peer, login_lock)
        except Exception as error:
            with suppress(OSError):
                peer.send(("error", str(error) or type(error).__name__))

    with peer:
        asyncio.run(execute())


def receive_workers(peers, phase):
    pending = set(peers)
    results = []
    while pending:
        for peer in wait(pending):
            try:
                received, value = peer.recv()
            except EOFError:
                raise RuntimeError(f"Proces {peers[peer]} zakończył się przed końcem testu.") from None
            if received == "error":
                raise RuntimeError(f"Proces {peers[peer]}: {value}")
            if received != phase:
                raise RuntimeError(f"Nieoczekiwana wiadomość z procesu {peers[peer]}.")
            results.append(value)
            pending.remove(peer)
    return results


def run_parallel(settings, args, password, workers):
    context = multiprocessing.get_context("spawn")
    login_lock = context.Lock()
    peers, processes = {}, []
    try:
        for index in range(workers):
            parent, child = context.Pipe()
            peers[parent] = index + 1
            with child:
                process = context.Process(
                    target=worker_entry, args=(settings, args, password, index, workers, child, login_lock)
                )
                process.start()
                processes.append(process)
        for start in range(0, args.n, args.batch_size):
            end = min(start + args.batch_size, args.n)
            for phase in ("prepared", "entered", "played", "finished"):
                results = receive_workers(peers, phase)
                if phase == "entered":
                    print(f"W grze: {end - start} postaci przez {args.m} s ({workers} procesy).", flush=True)
                if phase == "finished":
                    print(f"OK {end}/{args.n} – potwierdzono ruch i dostarczenie czatu.", flush=True)
                for peer in peers:
                    peer.send(results)
        for process in processes:
            process.join()
            if process.exitcode:
                raise RuntimeError(f"Proces Pythona zakończył się kodem {process.exitcode}.")
    finally:
        for peer in peers:
            peer.close()
        for process in processes:
            if process.is_alive():
                process.terminate()
            process.join()


def main():
    parser = account_parser(__doc__)
    parser.add_argument("m", type=int, nargs="?", default=30, help="czas w sekundach na grupę (domyślnie 30)")
    parser.add_argument("--batch-size", type=int, default=10, help="liczba postaci w grupie (domyślnie 10)")
    parser.add_argument(
        "--logins-per-minute", type=int, default=LOGINS_PER_MINUTE,
        help="budżet logowań skryptu zgodny z login_ip_limit serwera (domyślnie 30)",
    )
    parser.add_argument("--workers", type=int, default=1, help="liczba osobnych procesów Pythona (domyślnie 1)")
    args = parse_args(parser)
    if args.m < 1:
        parser.error("m musi być dodatnie")
    if args.batch_size < 1:
        parser.error("--batch-size musi być dodatnie")
    if args.logins_per_minute < 1:
        parser.error("--logins-per-minute musi być dodatnie")
    if args.workers < 1:
        parser.error("--workers musi być dodatnie")
    try:
        settings = connection_settings()
        password = test_password()
        workers = min(args.workers, args.n, args.batch_size, args.logins_per_minute)
        if workers == 1:
            asyncio.run(run(settings, args, password))
        else:
            run_parallel(settings, args, password, workers)
    except asyncio.TimeoutError:
        print("Test nieudany: przekroczono limit czasu połączenia lub potwierdzenia akcji.", file=sys.stderr)
        return 1
    except (OSError, EOFError, KeyError, RuntimeError, ValueError) as error:
        print(f"Test nieudany: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130
    return 0


if __name__ == "__main__":
    sys.exit(main())
