# Protokół gry

Transport: TLS 1.2/1.3 nad TCP. Adres nasłuchu i port ustawia `.env`.
Klient weryfikuje certyfikat serwera i nazwę hosta. Lokalny certyfikat CA
znajduje się w `server/priv/tls/ca.crt`; przygotowanie opisuje README serwera.
Każde nowe połączenie rozpoczyna się bez zalogowanego konta.

## Ramka

Wewnątrz TLS każda wiadomość zawiera 4 bajty długości treści (unsigned,
big-endian), a następnie obiekt JSON w UTF-8. Limit treści wynosi 4096 bajtów,
z wyjątkiem zdarzeń `world_state`, które mogą mieć do 128 KiB (131 072 bajty).
Dane mapy opisane poniżej są przesyłane jako osobne ramki z bajtami pliku,
po maksymalnie 4096 bajtów.
Długość nie obejmuje nagłówka. `{packet, 4}` w Erlangu składa pełną ramkę
i usuwa nagłówek; klient musi sam obsłużyć fragmentację i łączenie danych.
Ramka powyżej limitu zamyka połączenie.

## Wiadomości

Poniższe przykłady to treść JSON, bez nagłówka długości.
Nieznane dodatkowe pola są ignorowane; pole `user_id` klienta nie określa konta.

| Żądanie | Dostęp | Odpowiedź sukcesu |
|---|---|---|
| `ping` | Zawsze | `pong` |
| `register` | Przed logowaniem | `register_ok` |
| `login` | Przed logowaniem | `login_ok` |
| `list_characters` | Zalogowane konto | `characters` |
| `select_character` | Zalogowane konto, własna postać | `character_selected` |
| `get_map` | Zalogowane konto, wybrana postać | `map_data` i ramki z plikiem TMJ |
| `enter_world` | Zalogowane konto, pobrana mapa postaci | `world_joined` |
| `move_input` | Postać w świecie | `input_ack` w kolejnym stanie świata |
| `chat_send` | Postać w świecie | `chat_message` rozsyłane do graczy |
| `leave_world` | Zalogowane konto | `world_left` |
| `logout` | Zalogowane konto | `logout_ok` |

```json
{"type":"ping"}
{"type":"pong"}
```

Rejestracja tworzy konto bez postaci i nie loguje automatycznie.
Login: 3–32 znaki Unicode bez białych i sterujących znaków; konta nie
rozróżniają wielkości liter. Nowe hasło: 8–128 znaków Unicode, co najmniej jedna
wielka litera i jeden znak specjalny (interpunkcja lub symbol, np. `!`, `@`, `#`),
do 512 bajtów UTF-8. Spacja nie spełnia wymagania znaku specjalnego.
Wymagania składu hasła dotyczą rejestracji; logowanie sprawdza zapisany hash.

```json
{"type":"register","username":"Franek","password":"Przykladowe dlugie haslo!"}
{"type":"register_ok"}
{"type":"login","username":"Franek","password":"Przykladowe dlugie haslo!"}
{"type":"login_ok","user_id":7,"username":"Franek","expires_in_ms":28800000}
```

Serwer przypisuje konto do bieżącego połączenia. Kolejne wiadomości nie
zawierają hasła ani tokenu. Ponowne połączenie wymaga ponownego logowania.
Zmiana konta na istniejącym połączeniu wymaga wcześniejszego `logout`.

Lista obejmuje wyłącznie własne postacie, do 10 na stronę, rosnąco po `id`.
Przy `has_more: true` przekaż `after_id` równy ostatniemu otrzymanemu ID.

```json
{"type":"list_characters"}
{"type":"characters","characters":[{"id":12,"name":"Rycerz"}],"has_more":false}
{"type":"list_characters","after_id":12}
```

Wybór postaci sprawdza w bazie jej właściciela i zapamiętuje dane postaci w sesji.
Symulacja zaczyna się po `enter_world`.

```json
{"type":"select_character","character_id":12}
{"type":"character_selected","character":{"id":12,"name":"Rycerz","map_key":"map_expanded","position_x":1350.0,"position_y":1320.0,"facing":4,"level":1,"experience":0}}
{"type":"logout"}
{"type":"logout_ok"}
```

Wylogowanie usuwa konto i wybór postaci z sesji, pozostawiając połączenie.
Domyślnie klient ma 60 s na zalogowanie, a sesja jest ważna 8 godzin.
Ping i błędne próby nie przedłużają czasu na logowanie ani ważności sesji.
Po wygaśnięciu serwer może sam wysłać `session_expired`; klient musi obsługiwać
wiadomości serwera niezależnie od oczekiwania na odpowiedź na żądanie.
Następnie klient ma ponownie 60 s na logowanie.

## Pobieranie mapy

Po `character_selected` klient może wysłać `{"type":"get_map"}`. Serwer
ustala klucz mapy na podstawie wybranej postaci i odpowiada nagłówkiem
`{"type":"map_data","map_key":"map_expanded","size":274917}`. `size`
to liczba bajtów w pliku TMJ, od 1 do 1 MiB. Bezpośrednio po nagłówku
przychodzą ramki z kolejnymi bajtami pliku. Każda zawiera 4096 bajtów lub
tyle, ile pozostało do końca pliku. Wszystkie ramki mają ten sam
czterobajtowy nagłówek długości. Po zebraniu `size` bajtów połączenie wraca
do zwykłych wiadomości JSON. Klient składa mapę z pobranego TMJ i lokalnego
atlasu `Assets/Graphics/TileSet_001.png`.

## Ruch i obecność w świecie

Po załadowaniu TMJ klient wysyła `{"type":"enter_world"}`. `world_joined`
zawiera obiekt `character` o tym samym formacie co `character_selected`,
z pozycją startową ograniczoną do granic mapy. Jedna postać może mieć
jedno aktywne wejście. Serwer przyjmuje do 256 postaci na jednej mapie.

```json
{"type":"move_input","seq":1,"dx":1,"dy":0}
{"type":"world_state","map_key":"map_expanded","tick":124,"players":[{"id":12,"name":"Rycerz","position_x":1352.6666666667,"position_y":1320.0,"facing":2,"moving":true,"vx":160.0,"vy":0.0,"input_ack":1}]}
{"type":"move_input","seq":2,"dx":0,"dy":0}
{"type":"leave_world"}
{"type":"world_left"}
```

`dx` i `dy` przyjmują liczby całkowite od -1 do 1. Osie rosną w prawo
i w dół. `seq` rośnie od 1 po każdym wejściu do świata. Każde wejście
reprezentuje jeden krok ruchu o długości 1/60 s. Serwer wiąże je
z postacią połączenia, normalizuje ruch po skosie i stosuje prędkość
160 jednostek na sekundę. W każdym kroku świata wykonuje do dwóch
oczekujących wejść na postać. Pusta kolejka zatrzymuje postać.

`world_state` przychodzi 30 razy na sekundę niezależnie od odpowiedzi na żądania.
Zawiera numer kroku `tick` i listę postaci na bieżącej mapie. `input_ack`
to ostatni numer wejścia uwzględniony w pozycji danej postaci. Klient
przewiduje ruch własnej postaci od razu, a po odebraniu stanu odtwarza
wejścia z numerem większym od `input_ack`. Dla pozostałych postaci
interpoluje stany według `tick` z docelowym opóźnieniem 45 ms.
Przy braku następnego stanu może wykorzystać `vx` i `vy` przez maksymalnie
jeden krok świata. Krótka przerwa w ruchu podtrzymuje animację chodu przez
maksymalnie dwa kroki świata. Obie prędkości podano w jednostkach na sekundę.

Odrzucone wejście w aktywnej sesji daje zdarzenie `move_error` z polem
`code`. Przy wyjściu, wylogowaniu, wygaśnięciu sesji lub zamknięciu
połączenia serwer zapisuje ostatnią pozycję z symulacji do `position_x`
i `position_y` w PostgreSQL. `world_left` i `logout_ok` są wysyłane
po udanym zapisie. Jeśli zapis się nie powiedzie, klient może ponowić
żądanie. Po zerwaniu połączenia lub wygaśnięciu sesji serwer ponawia
zapis przy chwilowej niedostępności bazy. Przy następnym wejściu do
świata serwer odczytuje zapisaną pozycję.

## Czat

```json
{"type":"chat_send","text":"Cześć!"}
{"type":"chat_message","character_id":12,"name":"Rycerz","text":"Cześć!"}
```

Serwer pobiera `character_id` i `name` z aktywnej postaci nadawcy. Każda
przyjęta wiadomość trafia jako zdarzenie `chat_message` do wszystkich graczy
w świecie, w tym do nadawcy oraz do graczy na innych mapach. Klient dodaje
zdarzenie do panelu czatu i pokazuje dymek, gdy postać nadawcy jest w polu
widzenia kamery.

`text` musi zawierać od 1 do 256 punktów kodowych Unicode. Serwer usuwa
białe znaki z początku i końca tekstu. Odrzuca puste wiadomości oraz tekst
ze znakami sterującymi ASCII, w tym znakami nowej linii. Odrzucone żądanie
zalogowanego klienta daje zdarzenie `chat_error` z polem `code`
(`invalid_message` lub `world_required`).

## Błędy

```json
{"type":"error","code":"invalid_credentials"}
```

| Kod | Znaczenie |
|---|---|
| `invalid_json` | Niepoprawny JSON lub UTF-8 |
| `invalid_message` | Niepoprawna struktura wiadomości lub identyfikator |
| `unknown_message_type` | Nieobsługiwany typ, również `join` |
| `invalid_registration` | Niepoprawny login lub hasło przy rejestracji |
| `registration_failed` | Konto nie mogło zostać utworzone, np. zajęty login |
| `invalid_credentials` | Błędny login lub hasło; wspólny kod również dla braku konta |
| `authentication_required` | Operacja wymaga logowania |
| `already_authenticated` | Zalogowany klient próbuje ponownie logować lub rejestrować |
| `character_not_found` | Postać nie istnieje albo należy do innego konta |
| `character_required` | Przed pobraniem mapy trzeba wybrać postać |
| `map_not_found` | Serwer nie obsługuje klucza mapy zapisanej dla postaci |
| `map_unavailable` | Serwer nie może odczytać pliku TMJ lub plik ma nieprawidłowy rozmiar |
| `map_required` | Przed wejściem do świata trzeba pobrać mapę wybranej postaci |
| `world_required` | Ruch i czat wymagają wejścia do świata |
| `already_in_world` | Połączenie jest już w świecie gry |
| `character_in_world` | Wybrana postać jest aktywna w innym połączeniu |
| `world_full` | Mapa osiągnęła limit aktywnych postaci |
| `position_save_failed` | Zapis pozycji się nie powiódł. Można ponowić wyjście lub wylogowanie |
| `session_expired` | Sesja wygasła; konto i wybór postaci zostały usunięte z sesji |
| `rate_limited` | Przekroczony limit prób; zaczekaj na koniec okna limitu |
| `server_busy` | Brak wolnego miejsca na operację uwierzytelnienia; spróbuj później |
| `service_unavailable` | Niedostępna baza lub usługa uwierzytelniania |

Domyślnie: 5 prób logowania/min/konto, 30/min/IP i 5 rejestracji/h/IP.
Limity obejmują udane próby, obowiązują między połączeniami i są liczone
osobno dla logowania oraz rejestracji. Konfiguracja jest w `server/config/sys.config`.
Po błędzie aplikacyjnym połączenie pozostaje otwarte, z uwzględnieniem timerów.

Do sprawdzenia TLS i ramki służy `server/tools/ping.py`, a kont i sesji
`server/tools/auth.py`. Ten port nie obsługuje HTTP POST z Postmana.
