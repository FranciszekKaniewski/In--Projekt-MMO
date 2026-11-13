# Konta użytkowników i postacie

Na tym etapie używamy jawnego SQL. Migracje to numerowane pliki opisujące
kolejne zmiany struktury bazy, przechowywane razem z kodem w Git.
Plik `migrations/001_create_users.sql` tworzy tabelę `public.users` w istniejącej
bazie `sdl_game`. Uruchamia się go raz. Następne zmiany otrzymają kolejne pliki;
nie edytujemy migracji, która została już zastosowana.

Migracja `migrations/002_create_characters.sql` dodaje postacie powiązane
z kontami.
Migracje należy wykonywać po kolei: 001, potem 002.

## Dlaczego SQL

ORM może ułatwiać obsługę rozbudowanego modelu danych. Obecny schemat opisujemy
migracjami SQL; ich tabele można następnie odwzorować również w ORM.
Jawny SQL pozwala prześledzić ograniczenia i relacje kont oraz postaci.
`server_db` obsługuje połączenie przez `epgsql`. Parametryzowane zapytania
znajdują się w `server_accounts` dla kont i w `server_characters` dla postaci.

Przykład zapytania używanego przy logowaniu:

```erlang
epgsql:equery(Connection,
    "SELECT id, username, password_hash FROM public.users WHERE lower(username) = lower($1)",
    [Username]).
```

`$1` jest parametrem przekazywanym oddzielnie. Nie składamy SQL przez
doklejanie loginu lub hasła do tekstu zapytania.

## Struktura

| Kolumna | Zadanie |
|---|---|
| `id` | Trwały identyfikator konta, nadawany automatycznie |
| `username` | Login długości 3–32 punktów kodowych, bez białych i sterujących znaków |
| `password_hash` | Pełny zapis hasha hasła, z parametrami i solą |
| `created_at` | Data utworzenia konta, typ `timestamptz` |

Indeks unikalny na `lower(username)` sprawia, że `Franek` i `franek` nie mogą
być oddzielnymi kontami. Oryginalna pisownia loginu zostaje zachowana.
`id` konta jest niezależny od identyfikatora sesji i lokalnej encji ECS.

Hasła są hashowane i sprawdzane po stronie serwera Erlanga, za pomocą
biblioteki Argon2id. Do bazy zapisujemy pełny zakodowany wynik, który
zawiera sól i parametry; oddzielna kolumna na sól nie jest potrzebna.
Sama tabela nie oblicza ani nie weryfikuje hasha. Jej ograniczenie sprawdza
tylko, czy pole jest obecne i nie jest puste. Nie wpisuj do niego zwykłego hasła.

## Zastosowanie migracji

Polecenia wykonuj z katalogu `server`, gdzie znajdują się `compose.yaml` i `.env`:

```powershell
docker compose up -d --wait db
Get-Content -Raw -Encoding UTF8 db/migrations/001_create_users.sql | docker compose exec -T db psql -U game_dev -d sdl_game -v ON_ERROR_STOP=1
Get-Content -Raw -Encoding UTF8 db/migrations/002_create_characters.sql | docker compose exec -T db psql -U game_dev -d sdl_game -v ON_ERROR_STOP=1
```

Możesz też otworzyć plik migracji w HeidiSQL, połączyć się z bazą `sdl_game`
i uruchomić cały plik w zakładce zapytania SQL. `BEGIN` i `COMMIT` obejmują
zmiany jedną transakcją. Jeśli wystąpi błąd, wycofaj ją przez `ROLLBACK;`.
Istniejąca tabela `users` spowoduje błąd, zamiast zostać nadpisana lub usunięta.

Po zastosowaniu migracji odśwież listę tabel w HeidiSQL. Możesz sprawdzić:

```sql
SELECT id, username, created_at FROM public.users;
```

Tabela początkowo jest pusta. Konto z poprawnym hashem utworzysz przez
`python tools/auth.py register LOGIN`; instrukcja TLS znajduje się w
[README serwera](../README.md). Rejestracja nie tworzy postaci.
Autoryzacja sprawdza `characters.user_id` względem konta z sesji.
Sesja istnieje w pamięci procesu połączenia, więc nie wymaga nowej tabeli.

## Sprawdzenie ograniczeń

```powershell
Get-Content -Raw -Encoding UTF8 db/tests/001_users.sql | docker compose exec -T db psql -U game_dev -d sdl_game -v ON_ERROR_STOP=1
```

Test sprawdza automatyczne wartości, wyszukiwanie bez uwzględniania wielkości
liter, blokowanie duplikatów i niepoprawnych wartości. Kończy się `ROLLBACK`,
więc nie pozostawia kont testowych. Używa syntetycznego tekstu w polu hasha,
ponieważ nie testuje kryptografii ani logowania. Sekwencja identyfikatorów
może mieć luki po wycofanych transakcjach; jest to normalne.

Źródła: [epgsql](https://github.com/epgsql/epgsql),
[unikalne indeksy na wyrażeniach](https://www.postgresql.org/docs/18/indexes-expressional.html),
[przechowywanie haseł — OWASP](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html).
