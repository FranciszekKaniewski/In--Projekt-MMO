# PostgreSQL w Dockerze

PostgreSQL działa w Dockerze, a serwer Erlanga uruchamiamy lokalnie przez
`rebar3 shell`. Compose zawiera wyłącznie usługę bazy danych.

## Uruchomienie

W katalogu `server` powinien znajdować się lokalny plik `.env`.
Przy pierwszym przygotowaniu tego środowiska został utworzony z losowym hasłem.
Na nowej maszynie skopiuj `.env.example` do `.env` i ustaw własne hasło.
Plik `.env` jest pomijany przez Git.

Przy uruchomionym Docker Desktop, w katalogu `server`:

```powershell
docker compose up -d --wait db
docker compose ps
```

Domyślnie uruchamia się tylko baza (`docker compose up -d --wait` daje ten sam
efekt). Serwer Erlanga uruchamiaj w katalogu `server`:

```powershell
rebar3 shell
```

Serwer korzysta z `epgsql` do rejestracji, logowania i odczytu własnych postaci.
Czyta dane połączenia z `.env` (zmienne środowiska procesu mają pierwszeństwo).
Przy pierwszym uruchomieniu przygotuj lokalny certyfikat TLS według
[README serwera](../server/README.md).

## Dane połączenia z Windowsa

| Pole | Wartość domyślna |
|---|---|
| Host | `127.0.0.1` |
| Port | `5432` (zmienna `POSTGRES_PORT` w `.env`) |
| Baza | `sdl_game` |
| Użytkownik | `game_dev` |
| Hasło | `POSTGRES_PASSWORD` z lokalnego `.env` |

`game_dev` to konto administracyjne lokalnej bazy utworzone przez oficjalny
obraz. Konto aplikacji z uprawnieniami do jej tabel przygotujemy przy
podłączaniu serwera. Port jest udostępniony na lokalnym interfejsie Windowsa.

Z lokalnego Erlanga używamy `127.0.0.1` i portu hosta.

## Konsola SQL i logi

```powershell
docker compose exec db psql -U game_dev -d sdl_game
```

W konsoli SQL:

```sql
SELECT current_database(), current_user, version();
```

`\q` zamyka konsolę SQL. Podgląd logów:

```powershell
docker compose logs -f db
```

## Dane i zatrzymanie

```powershell
docker compose down
```

Polecenie zatrzymuje bazę i usuwa jej kontener, zachowując dane w nazwanym
wolumenie `sdl-game-engine_postgres_data`. Następne `up` wykorzysta te dane.
W PostgreSQL 18 wolumen jest montowany w `/var/lib/postgresql`; właściwy
katalog danych znajduje się w jego podkatalogu `18/docker`.

Zmienne `POSTGRES_DB`, `POSTGRES_USER` i `POSTGRES_PASSWORD` inicjalizują pusty
wolumen przy pierwszym starcie. Późniejsza edycja `.env` nie zmienia hasła,
użytkownika ani nazwy już istniejącej bazy. Takie zmiany wykonuje się w SQL.

## Testy serwera

W katalogu `server` uruchom lokalnie:

```powershell
rebar3 eunit
```

Testy podstawowe korzystają z lokalnych połączeń TLS i nie wymagają bazy.
`./tools/Test-Database.ps1` uruchamia dodatkowo testy rejestracji, logowania
i własności postaci w osobnej bazie `sdl_auth_test_*`, usuwanej po testach.

## Konta użytkowników

Pierwszy schemat i instrukcja jego zastosowania znajdują się w
[server/db/README.md](../server/db/README.md). Migracja `001_create_users.sql`
tworzy tabelę `public.users` z loginem, hashem hasła i datą utworzenia konta.
Migracja `002_create_characters.sql` dodaje postacie właścicieli.
