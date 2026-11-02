# Serwer gry — ping → pong

Serwer TCP przyjmuje wiele połączeń i odpowiada na komunikat `ping` komunikatem
`pong`. Wymaga Erlang/OTP 27 lub nowszego (moduł `json`) oraz `rebar3`.

## Uruchomienie

Jeżeli działa poprzednia wersja serwera, zakończ jej powłokę przez `q().`.
W terminalu otwartym w tym katalogu uruchom:

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
| `handle_info/2` | Dla pozostałych wiadomości, m.in. danych TCP i timerów |
| `terminate/2` | Przy obsługiwanym zakończeniu procesu; sprząta zasoby |

`-behaviour(gen_server).` deklaruje używany mechanizm OTP, a `-export([...]).`
wskazuje funkcje dostępne spoza modułu. Dane TCP trafiają do `handle_info/2`,
a nie do `handle_call/3`.

## Krok 1: uruchomienie aplikacji

`rebar.config` wskazuje aplikację `server` i plik konfiguracji powłoki.
`server.app.src` opisuje aplikację i wskazuje moduł startowy `server_app`.
OTP wywołuje `server_app:start/2`, który uruchamia `server_sup:start_link/0`.

Supervisor uruchamia i nadzoruje swoje procesy potomne:

- `server_connection_sup` nadzoruje procesy obsługujące klientów.
- `server_listener` otwiera port i uruchamia proces przyjmujący połączenia.

`rest_for_one` w głównym supervisorze oznacza: gdy trzeba odtworzyć proces,
odtwarzamy też jego później uruchomione zależności. Dlatego awaria supervisora
połączeń odtwarza także listener. Awaria samego listenera zachowuje istniejące
połączenia. `intensity => 5, period => 10` ogranicza liczbę restartów, aby nie
uruchamiać bez końca stale psującego się podsystemu.

## Krok 2: otwarcie portu

`server_listener:init/1` odczytuje konfigurację i wywołuje `gen_tcp:listen/2`.
Wynikiem jest socket nasłuchujący, z którego można przyjmować nowe połączenia.

Istotne opcje:

- `binary`: treść otrzymujemy jako binaria Erlanga.
- `{packet, 4}`: wiadomość ma czterobajtowy nagłówek długości.
- `{packet_size, 4096}`: treść jednej wiadomości może mieć najwyżej 4096 bajtów.
- `{active, false}`: na początku dane nie są dostarczane jako wiadomości do procesu.
- `{nodelay, true}`: małe wysyłane wiadomości nie czekają na agregowanie przez Nagle'a.

`spawn_link` uruchamia powiązany proces wykonujący `accept_loop/1`.
To w nim blokuje się `gen_tcp:accept/1`; sam `gen_server` listenera pozostaje
zdolny do obsługi innych wiadomości, np. zapytania o port.

## Krok 3: połączenie klienta

`ping.py` wywołuje `socket.create_connection`. `gen_tcp:accept/1` po stronie
serwera zwraca wtedy nowy socket reprezentujący połączenie z tym klientem.
Socket nasłuchujący pozostaje otwarty, więc można przyjmować następnych klientów.

Listener prosi `server_connection_sup` o uruchomienie `server_connection`.
`supervisor:start_child/2` tworzy osobny proces połączenia. `make_ref()` daje
unikalny identyfikator dziecka supervisora, a zwrócony `Pid` identyfikuje proces.
To lekkie procesy Erlanga działające we wspólnej maszynie wirtualnej.

## Krok 4: przekazanie socketu

Zaakceptowany socket początkowo należy do procesu, który wykonał `accept`.
`gen_tcp:controlling_process(Socket, Pid)` przekazuje go procesowi połączenia.
To właściciel otrzymuje później wiadomości o danych TCP.

Dopiero po przekazaniu socketu listener wywołuje `server_connection:activate/1`.
Ta funkcja wysyła `cast`, a `handle_cast/2` ustawia `{active, once}`:
jedna pełna ramka zostanie dostarczona do procesu jako wiadomość.

Stan procesu wygląda w przybliżeniu tak:

```erlang
#{socket => Socket, ready => true, timer => Timer}
```

`init/1` wcześniej ustawił pięciosekundowy timer oczekiwania na aktywację.
Aktywacja go anuluje. Ten timer zabezpiecza przekazanie socketu; nie jest
limitem bezczynności już aktywnego klienta.

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
{tcp, Socket, Data}
```

`gen_server` wybiera pasującą funkcję `handle_info/2` w `server_connection`.
`Data` zawiera już sam JSON. Funkcja wywołuje `server_protocol:reply(Data)`.

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

`server_connection` wywołuje `gen_tcp:send(Socket, Reply)`.
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
wyklucza odpowiedzi TCP — wysłaliśmy ją już samodzielnie przez `gen_tcp:send`.
Nie odpowiadamy tu na żadne oczekujące `gen_server:call`.

## Krok 8: błędy i rozłączenie

Nieznany typ, niepoprawna struktura i uszkodzony JSON dają odpowiedź błędu
opisaną w [protokole](../Shared/protocol.md). Połączenie pozostaje wtedy otwarte.
Ramka przekraczająca limit długości powoduje zamknięcie połączenia.

Po zamknięciu klienta do serwera trafia `{tcp_closed, Socket}`.
`{stop, normal, State}` kończy proces połączenia, a jego `terminate/2` zamyka
socket i anuluje timer. Specyfikacja `restart => temporary` oznacza, że proces
połączenia nie zostanie odtworzony ze starym, zamkniętym socketem.

## Testy

```powershell
rebar3 eunit
```

Dziesięć testów sprawdza prawdziwe połączenia TCP: ping, wielu klientów,
fragmentację i łączenie ramek, błędy, granice rozmiaru, rozłączenia i izolację
awarii. Komunikaty `join` i `chat_send` są obecnie nieobsługiwanymi typami.

Port gry używa własnego protokołu TCP. HTTP POST w Postmanie ma inny format
wiadomości; skrypt `ping.py` wysyła ramkę oczekiwaną przez ten serwer.
