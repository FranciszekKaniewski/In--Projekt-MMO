# Klient gry

Budowanie i uruchamianie z tego katalogu, na Windowsie 10 (1809+) lub 11,
z MinGW g++ obsługującym C++17 i GNU Make:

```powershell
make build
make run
```

Przed logowaniem uruchom serwer według [jego instrukcji](../server/README.md).
Klient weryfikuje łańcuch certyfikatów i nazwę hosta, używając Windows Schannel.

Przy uruchomieniu z repozytorium klient używa `server/.env`, jeśli obok
`SDL_Game_Engine.exe` nie ma lokalnego `.env`. Ścieżka `GAME_CAFILE` jest
względna względem użytego pliku `.env`.

Na drugim komputerze skopiuj `.env.example` do `.env` obok programu,
uzupełnij adres i port serwera oraz skopiuj publiczny certyfikat CA pod
ścieżkę `GAME_CAFILE`. Przykład dla LAN:

```dotenv
GAME_HOST=192.168.68.50
GAME_PORT=4000
GAME_CAFILE=ca.crt
GAME_TIMEOUT_MS=5000
```

Do klienta dołącz tylko jego `.env` i publiczny plik CA. Adres w
`GAME_HOST` musi być ujęty w certyfikacie TLS serwera.

Przycisk Play wysyła dane logowania w tle i pokazuje odpowiedź serwera.
Po sukcesie otwiera się lista własnych postaci z obsługą stron i odświeżania.
Wybierz postać i kliknij Play; po zatwierdzeniu przez serwer gra odtwarza
jej nazwę, pozycję i kierunek na mapie `map_expanded`.
Połączenie i sesja są wspólne dla tych scen. Escape w grze wraca do listy,
a Wyloguj zamyka sesję. Utrata połączenia lub wygaśnięcie sesji wraca
do logowania z komunikatem.

Konto bez postaci otrzyma pustą listę; tworzenie postaci nie ma jeszcze
osobnej operacji w protokole serwera. Testy opisuje [Tests/README.md](Tests/README.md).
