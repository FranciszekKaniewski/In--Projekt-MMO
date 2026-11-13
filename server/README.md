# Serwer gry

Serwer obsługuje ping, rejestrację, logowanie, listę i wybór własnych postaci,
pobieranie map oraz wylogowanie. Używa TLS, PostgreSQL i sesji przypisanej do połączenia.
Wymaga Erlang/OTP 27+, `rebar3` i kompilatora C dla biblioteki Argon2id.
Na Windowsie kompilacja używa GCC z MSYS2 (`C:/msys64/mingw64/bin/gcc.exe`)
lub kompilatora wskazanego zmienną `CC` (ścieżka do programu, bez argumentów).

## Uruchomienie

Na nowej maszynie skopiuj `.env.example` do `.env` i ustaw hasło bazy.
Uruchom bazę i zastosuj migracje 001 i 002 według [db/README.md](db/README.md).
Lokalny certyfikat utwórz jednorazowo w PowerShellu 7:

```powershell
./tools/New-DevCertificate.ps1
```

Pliki `priv/tls` są pomijane przez Git. `ca.crt` służy klientom do weryfikacji
certyfikatu dla `localhost` i `127.0.0.1`; klucz CA nie jest zapisywany.
Certyfikaty z `test/fixtures` są wyłącznie danymi testowymi.
Na wdrożeniu ustaw własny certyfikat i klucz przez `GAME_TLS_CERTFILE`
i `GAME_TLS_KEYFILE`. Serwer przyjmuje wyłącznie TLS 1.2/1.3.

Jeżeli działa poprzednia wersja serwera, zakończ jej powłokę przez `q().`.
W katalogu `server` uruchom:

```powershell
rebar3 shell
```

`rebar3` kompiluje projekt i uruchamia aplikację `server`. Adres i port pochodzą
z `config/sys.config`: domyślnie `127.0.0.1:4000`.
W drugim terminalu, również w tym katalogu:

```powershell
python tools/ping.py
```

Oczekiwany wynik: `{"type": "pong"}`. Opcje `--host` i `--port` pozwalają zmienić
adres. `server_listener:port().` w powłoce Erlanga pokazuje rzeczywisty port.
Port `0` w konfiguracji oznacza, że system wybierze wolny port; używają go testy.
`--cafile` wskazuje zaufany certyfikat CA, domyślnie `priv/tls/ca.crt`.
Oba skrypty (`ping.py` i `auth.py`) czytają `GAME_HOST`, `GAME_PORT` i
`GAME_CAFILE` z `server/.env`. Dla LAN ustaw `GAME_HOST` na adres serwera,
a `GAME_CAFILE` na publiczny plik CA używany przez klientów, np.
`priv/tls/lan/ca.crt`. Ścieżka `GAME_CAFILE` jest względna względem katalogu
`server`. Zmienne środowiskowe mają pierwszeństwo przed `.env`, a argumenty
`--host`, `--port` i `--cafile` pozwalają zmienić je dla jednego wywołania.

Po kliknięciu `Play` klient wybiera postać i pobiera przypisany jej plik TMJ
przez TLS. Klucze `map_expanded` i `map_example2` wskazują odpowiednio na
`Map_expanded.tmj` i `Map_example2.tmj` w katalogu `server/priv/maps`.
Katalog jest częścią aplikacji OTP. Ścieżkę do innego katalogu map można
ustawić przez `GAME_MAP_DIR`. Atlas graficzny należy do zasobów klienta.

## Logowanie i autoryzacja

```powershell
python tools/auth.py register Franek
python tools/auth.py login Franek
```

Hasło jest odczytywane bez wyświetlania i bez argumentów wiersza poleceń.
Po zalogowaniu polecenia `list`, `list AFTER_ID`, `select ID` i `logout`
pozwalają przetestować sesję. Rejestracja tworzy samo konto, bez postaci,
i nie loguje automatycznie. Wiadomości JSON opisuje [protokół](../Shared/protocol.md).

Login ma 3–32 znaki bez białych i sterujących znaków; wielkość liter nie
rozróżnia kont. Nowe hasło ma 8–128 znaków Unicode, co najmniej jedną wielką
literę i jeden znak specjalny (znak interpunkcyjny lub symbol, np. `!`, `@`, `#`).
Spacja nie zastępuje znaku specjalnego. Limit wynosi 512 bajtów UTF-8.
Klient pokazuje wymagania i przyczynę odrzucenia przed wysłaniem rejestracji.
Logowanie weryfikuje zapisany hash, również dla wcześniej utworzonych kont.
`server_password` używa Argon2id v19: 19 MiB pamięci, 2 iteracje, 1 wątek,
losowa sól 16 bajtów. Zapis PHC zawiera parametry i sól.
Przy braku konta serwer weryfikuje hash zastępczy. Błędne dane logowania
dają wspólny błąd `invalid_credentials`.

Zweryfikowane `user_id` i wybrana postać pozostają w procesie połączenia.
Lista i wybór postaci korzystają z tego `user_id`, a nie z pola klienta.
Wybór cudzej i nieistniejącej postaci daje ten sam `character_not_found`.
Wylogowanie i wygaśnięcie usuwają konto oraz wybór postaci z sesji.
Rozłączenie lub restart serwera wymaga ponownego logowania.
Po pobraniu mapy `enter_world` dodaje postać do symulacji. Serwer oblicza
ruch i rozsyła pozycje wszystkim aktywnym postaciom na tej samej mapie.

`config/sys.config` ustala: 60 s na logowanie, 8 h ważności sesji,
5 prób logowania/min/konto, 30/min/IP i 5 rejestracji/h/IP.
Limity obejmują również udane próby i obowiązują między połączeniami.
Maksymalnie 4 operacje hashowania/weryfikacji działają jednocześnie;
serwer przyjmuje maksymalnie 256 połączeń. Limity są lokalne dla tego
węzła Erlanga i zerują się przy restarcie.

Zależność `jargon` 1.1.0 jest przypięta w `rebar.lock`.
`tools/build_jargon.escript` buduje jej referencyjny kod C bez powłoki Unix
i poprawia numer wersji w wrapperze z dziesiętnego `13` na `0x13` (v19).
Test sprawdza wynik względem opublikowanego wektora Argon2id.

## PostgreSQL w Dockerze

Baza PostgreSQL działa w kontenerze, a serwer Erlanga lokalnie przez
`rebar3 shell`. Compose zawiera wyłącznie usługę bazy danych. Instrukcja konfiguracji bazy i dane połączenia znajdują
się w [Docs/database.md](../Docs/database.md).

W katalogu `server`, gdzie znajdują się `compose.yaml` i lokalny plik `.env`:

```powershell
docker compose up -d --wait db
```

Na nowej maszynie skopiuj `.env.example` do `.env` i ustaw hasło.
Plik `.env` jest pomijany przez Git. Baza jest domyślnie dostępna na
`127.0.0.1:5432`; dane przechowuje nazwany wolumen. `docker compose down`
zachowuje dane bazy. `server_config` czyta `.env`; zmienne środowiska procesu
mają pierwszeństwo. `server_db` utrzymuje jedno połączenie `epgsql`,
serializuje zapytania i odtwarza połączenie po awarii. Błąd bazy daje
`service_unavailable`, bez uwierzytelnienia klienta.

Migracje kont i postaci opisuje [db/README.md](db/README.md).
Tabela `public.characters` pozwala zapisać wiele postaci na konto oraz mapę,
pozycję i kierunek zgodne z klientem SDL.

## Architektura backendu

Backend jest jedną aplikacją OTP. W `src/` pliki `server.app.src`,
`server_app.erl`, `server_sup.erl` i `server_config.erl` odpowiadają za
uruchomienie, nadzór i konfigurację. Moduły zachowują prefiks `server_`.

| Katalog w `src/` | Odpowiedzialność |
|---|---|
| `net/` | Nasłuch TLS, procesy połączeń, sesje i kierowanie żądań JSON |
| `auth/` | Rejestracja, logowanie, hasła, limity prób i zapytania do tabeli kont |
| `characters/` | Lista postaci i odczyt postaci należącej do zalogowanego konta |
| `db/` | Utrzymywanie połączenia z PostgreSQL i wykonywanie zapytań |
| `world/` | Udostępnianie TMJ, stan aktywnych postaci i symulacja ruchu |

`server_protocol` przekazuje operacje do `server_auth` lub `server_characters`.
Moduły `server_accounts` i `server_characters` wykonują zapytania przez `server_db`.
Własność postaci jest sprawdzana z użyciem identyfikatora konta z sesji połączenia.

`server_world` liczy ruch na podstawie kierunku z klienta, używa granic
z udostępnionego TMJ i monitoruje procesy połączeń. Trwałe dane postaci
odczytuje `server_characters`.

## Moduł, proces i gen_server

Moduł to plik z funkcjami, np. `server_connection.erl`. Proces to uruchomiona,
niezależna jednostka wykonania, mająca własny stan i skrzynkę wiadomości.
Wiele procesów może wykonywać kod tego samego modułu. Tutaj każdy klient ma
osobny proces wykonujący `server_connection`.

`gen_server` zapewnia gotową pętlę obsługi wiadomości. Nasz moduł implementuje
funkcje, które ta pętla wywołuje:

| Funkcja | Kiedy się wykonuje |
|---|---|
| `init/1` | Przy uruchomieniu procesu; tworzy początkowy stan |
| `handle_call/3` | Gdy inny proces wywoła `gen_server:call`; oczekuje odpowiedzi |
| `handle_cast/2` | Gdy inny proces wywoła `gen_server:cast`; nie czeka na odpowiedź |
| `handle_info/2` | Dla pozostałych wiadomości, m.in. danych TLS i timerów |
| `terminate/2` | Przy obsługiwanym zakończeniu procesu; sprząta zasoby |

`-behaviour(gen_server).` deklaruje używany mechanizm OTP, a `-export([...]).`
wskazuje funkcje dostępne spoza modułu. Dane TLS trafiają do `handle_info/2`,
a nie do `handle_call/3`.

## Krok 1: uruchomienie aplikacji

`rebar.config` wskazuje aplikację `server` i plik konfiguracji powłoki.
`server.app.src` opisuje aplikację i wskazuje moduł startowy `server_app`.
OTP wywołuje `server_app:start/2`, który uruchamia `server_sup:start_link/0`.

Supervisor uruchamia i nadzoruje swoje procesy potomne:

- `server_db` utrzymuje połączenie z PostgreSQL.
- `server_auth_guard` ogranicza próby logowania i liczbę operacji Argon2.
- `server_connection_sup` nadzoruje procesy obsługujące klientów.
- `server_listener` otwiera port i uruchamia proces przyjmujący połączenia.

`rest_for_one` w głównym supervisorze oznacza: gdy trzeba odtworzyć proces,
odtwarzamy też jego później uruchomione zależności. Dlatego awaria supervisora
połączeń odtwarza także listener. Awaria samego listenera zachowuje istniejące
połączenia. `intensity => 5, period => 10` ogranicza liczbę restartów, aby nie
uruchamiać bez końca stale psującego się podsystemu.

## Krok 2: otwarcie portu

`server_listener:init/1` odczytuje konfigurację i wywołuje `ssl:listen/2`.
Wynikiem jest socket nasłuchujący, z którego można przyjmować nowe połączenia.

Istotne opcje:

- `binary`: treść otrzymujemy jako binaria Erlanga.
- `{packet, 4}`: wiadomość ma czterobajtowy nagłówek długości.
- `{packet_size, 4096}`: treść jednej wiadomości może mieć najwyżej 4096 bajtów.
- `{active, false}`: na początku dane nie są dostarczane jako wiadomości do procesu.
- `{nodelay, true}`: małe wysyłane wiadomości nie czekają na agregowanie przez Nagle'a.

`spawn_link` uruchamia powiązany proces wykonujący `accept_loop/1`.
To w nim blokuje się `ssl:transport_accept/1`; sam `gen_server` listenera pozostaje
zdolny do obsługi innych wiadomości, np. zapytania o port.

## Krok 3: połączenie klienta

`ping.py` otwiera TCP i następnie TLS z weryfikacją certyfikatu serwera. `ssl:transport_accept/1` po stronie
serwera zwraca wtedy nowy socket reprezentujący połączenie z tym klientem.
Socket nasłuchujący pozostaje otwarty, więc można przyjmować następnych klientów.

Listener prosi `server_connection_sup` o uruchomienie `server_connection`.
`supervisor:start_child/2` tworzy osobny proces połączenia. `make_ref()` daje
unikalny identyfikator dziecka supervisora, a zwrócony `Pid` identyfikuje proces.
To lekkie procesy Erlanga działające we wspólnej maszynie wirtualnej.

## Krok 4: przekazanie socketu

Zaakceptowany socket początkowo należy do procesu, który wykonał `accept`.
`ssl:controlling_process(Socket, Pid)` przekazuje go procesowi połączenia.
To właściciel otrzymuje później wiadomości o danych TCP.

Dopiero po przekazaniu socketu listener wywołuje `server_connection:activate/1`.
Ta funkcja wysyła `cast`, a `handle_cast/2` wykonuje handshake TLS
z limitem 5 s i ustawia `{active, once}`:
jedna pełna ramka zostanie dostarczona do procesu jako wiadomość.

Stan procesu wygląda w przybliżeniu tak:

```erlang
#{socket => Socket, ready => true, timer => Timer,
  user => undefined, character_id => undefined, session_deadline => undefined}
```

`init/1` wcześniej ustawił pięciosekundowy timer oczekiwania na aktywację.
Aktywacja go anuluje. Ten timer zabezpiecza przekazanie socketu; nie jest
limitem bezczynności już aktywnego klienta. Po handshake rusza osobny
timer na logowanie; po logowaniu zastępuje go timer ważności sesji.

## Krok 5: wysłanie ping

Python koduje `{"type": "ping"}` jako UTF-8 i oblicza długość w bajtach.
`struct.pack("!I", len(payload))` tworzy czterobajtową liczbę bez znaku,
w kolejności big-endian. `sendall` wysyła nagłówek i treść:

```text
[4 bajty długości][JSON w UTF-8]
```

TCP jest strumieniem bajtów. Jeden odczyt może zwrócić fragment wiadomości lub
kilka wiadomości naraz. Dzięki `{packet, 4}` Erlang składa kompletną ramkę i
usuwa nagłówek przed przekazaniem treści do naszego kodu.

## Krok 6: odebranie i rozpoznanie wiadomości

Do procesu połączenia trafia wiadomość:

```erlang
{ssl, Socket, Data}
```

`gen_server` wybiera pasującą funkcję `handle_info/2` w `server_connection`.
`Data` zawiera już sam JSON. Funkcja wywołuje `server_protocol:handle(Data, State)`, zwracające odpowiedź i nowy stan sesji.

`server_protocol` dekoduje JSON do mapy i dopasowuje wzorzec:

```erlang
{ok, #{<<"type">> := <<"ping">>}}
```

`#{...}` oznacza mapę. `<<"type">>` i `<<"ping">>` to binarne napisy.
`:=` we wzorcu wymaga istnienia danego klucza z pasującą wartością.
Klucze i napisy z JSON są binariami; `ok` jest atomem, czyli stałą nazwą.
Dopasowanie tworzy mapę odpowiedzi:

```erlang
#{<<"type">> => <<"pong">>}
```

Tutaj `=>` umieszcza parę klucz–wartość w tworzonej mapie.
`json:encode/1` zamienia odpowiedź na dane, które można wysłać przez socket.
Protokół rozpoznaje treść; sam nie zarządza połączeniem ani nie wywołuje `send`.

## Krok 7: odpowiedź i następna wiadomość

`server_connection` wywołuje `ssl:send(Socket, Reply)`.
Opcja `{packet, 4}` automatycznie dodaje nagłówek długości do odpowiedzi.
Python odczytuje najpierw cztery bajty, potem wskazaną liczbę bajtów treści,
a następnie dekoduje JSON i wypisuje wynik.

`receive_exact` w Pythonie powtarza `recv`, aż uzbiera całość. Chroni to przed
odczytaniem tylko fragmentu nagłówka lub treści.

Po wysłaniu odpowiedzi proces serwera ponownie ustawia `{active, once}`.
Bez tego kolejne dane nie byłyby dostarczane jako wiadomości. Zwraca też:

```erlang
{noreply, State}
```

To informacja dla `gen_server`: kontynuuj z tym stanem. Nazwa `noreply` nie
wyklucza odpowiedzi TCP — wysłaliśmy ją już samodzielnie przez `ssl:send`.
Nie odpowiadamy tu na żadne oczekujące `gen_server:call`.

## Krok 8: błędy i rozłączenie

Nieznany typ, niepoprawna struktura i uszkodzony JSON dają odpowiedź błędu
opisaną w [protokole](../Shared/protocol.md). Połączenie pozostaje wtedy otwarte.
Ramka przekraczająca limit długości powoduje zamknięcie połączenia.

Po zamknięciu klienta do serwera trafia `{ssl_closed, Socket}`.
`{stop, normal, State}` kończy proces połączenia, a jego `terminate/2` zamyka
socket i anuluje timer. Specyfikacja `restart => temporary` oznacza, że proces
połączenia nie zostanie odtworzony ze starym, zamkniętym socketem.

## Testy

```powershell
rebar3 eunit
```

Testy sprawdzają prawdziwe połączenia TLS: ping, wielu klientów,
fragmentację i łączenie ramek, błędy, granice rozmiaru, rozłączenia i izolację
awarii, certyfikaty, timeouty i ograniczanie prób logowania. Komunikaty `join` i `chat_send` są obecnie nieobsługiwanymi typami.

Pełne testy kont wymagają działającego Dockera i lokalnego `.env`:

```powershell
./tools/Test-Database.ps1
```

Skrypt tworzy osobną bazę, stosuje migracje, uruchamia testy i usuwa bazę
w bloku `finally`. Sprawdza również logowanie, izolację kont, wylogowanie,
wygaśnięcie sesji i awarię bazy. Nie zmienia danych `sdl_game`.

Port gry używa własnego protokołu po TLS. HTTP POST w Postmanie ma inny format
wiadomości; skrypt `ping.py` wysyła ramkę oczekiwaną przez ten serwer.
