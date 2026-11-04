# Klient gry

Budowanie i uruchamianie z tego katalogu, na Windowsie 10 (1809+) lub 11,
z MinGW g++ obsługującym C++17 i GNU Make:

```powershell
make build
make run
```

Przed logowaniem uruchom serwer według [jego instrukcji](../server/README.md).
`make build` kopiuje lokalny publiczny certyfikat CA z
`../server/priv/tls/ca.crt` do `build/Config/server-ca.crt`.
Klient weryfikuje łańcuch certyfikatów i nazwę hosta, używając Windows Schannel.
Nie instaluje CA w systemowym magazynie certyfikatów.

Połączenie konfiguruje sekcja `server` w `build/Config/settings.json`:

```json
"server": {
  "host": "localhost",
  "port": 4000,
  "caFile": "server-ca.crt",
  "timeoutMs": 5000
}
```

Ścieżka `caFile` jest względna względem pliku ustawień. Login i hasło
nie są zapisywane w konfiguracji ani wypisywane do konsoli.

Przycisk Play wysyła dane logowania w tle i pokazuje odpowiedź serwera.
Po sukcesie otwiera się lista własnych postaci z obsługą stron i odświeżania.
Wybierz postać i kliknij Play; po zatwierdzeniu przez serwer gra odtwarza
jej nazwę, pozycję i kierunek na mapie `map_expanded`.
Połączenie i sesja są wspólne dla tych scen. Escape w grze wraca do listy,
a Wyloguj zamyka sesję. Utrata połączenia lub wygaśnięcie sesji wraca
do logowania z komunikatem.

Konto bez postaci otrzyma pustą listę; tworzenie postaci nie ma jeszcze
osobnej operacji w protokole serwera. Testy opisuje [Tests/README.md](Tests/README.md).
