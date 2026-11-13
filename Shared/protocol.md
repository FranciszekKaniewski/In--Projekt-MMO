# Protokół gry

Transport: TLS 1.2/1.3 nad TCP, domyślnie `127.0.0.1:4000`.
Klient weryfikuje certyfikat serwera i nazwę hosta. Lokalny certyfikat CA
znajduje się w `server/priv/tls/ca.crt`; przygotowanie opisuje README serwera.
Każde nowe połączenie rozpoczyna się bez zalogowanego konta.

## Ramka

Wewnątrz TLS każda wiadomość zawiera 4 bajty długości treści (unsigned,
big-endian), a następnie obiekt JSON w UTF-8, maksymalnie 4096 bajtów.
Dane mapy opisane poniżej są przesyłane jako osobne ramki z bajtami pliku.
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

Wybór postaci sprawdza w bazie jej właściciela i zapamiętuje ID w sesji.
Nie uruchamia jeszcze symulacji świata ani nie zapisuje pozycji.

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

## Błędy

```json
{"type":"error","code":"invalid_credentials"}
```

| Kod | Znaczenie |
|---|---|
| `invalid_json` | Niepoprawny JSON lub UTF-8 |
| `invalid_message` | Niepoprawna struktura wiadomości lub identyfikator |
| `unknown_message_type` | Nieobsługiwany typ, również `join` i `chat_send` |
| `invalid_registration` | Niepoprawny login lub hasło przy rejestracji |
| `registration_failed` | Konto nie mogło zostać utworzone, np. zajęty login |
| `invalid_credentials` | Błędny login lub hasło; wspólny kod również dla braku konta |
| `authentication_required` | Operacja wymaga logowania |
| `already_authenticated` | Zalogowany klient próbuje ponownie logować lub rejestrować |
| `character_not_found` | Postać nie istnieje albo należy do innego konta |
| `character_required` | Przed pobraniem mapy trzeba wybrać postać |
| `map_not_found` | Serwer nie obsługuje klucza mapy zapisanej dla postaci |
| `map_unavailable` | Serwer nie może odczytać pliku TMJ lub plik ma nieprawidłowy rozmiar |
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
