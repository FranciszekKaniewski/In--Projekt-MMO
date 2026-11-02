# Konta użytkowników

Na tym etapie używamy jawnego SQL. Migracje to numerowane pliki opisujące
kolejne zmiany struktury bazy, przechowywane razem z kodem w Git.
Plik `migrations/001_create_users.sql` tworzy tabelę `public.users` w istniejącej
bazie `sdl_game`. Uruchamia się go raz. Następne zmiany otrzymają kolejne pliki;
nie edytujemy migracji, która została już zastosowana.

## Dlaczego SQL

ORM może ułatwiać obsługę rozbudowanego modelu danych. Obecnie mamy jedną
tabelę, a jawny SQL pozwoli łatwiej prześledzić rejestrację i logowanie.
Przy integracji Erlanga użyjemy sterownika takiego jak `epgsql` oraz osobnego
modułu dostępu do kont, aby zapytania nie trafiały do kodu scen czy socketów.

Przykład przyszłego zapytania (sterownik nie jest jeszcze dodany do projektu):

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

Hasła będą hashowane i sprawdzane po stronie serwera Erlanga, za pomocą
gotowej biblioteki Argon2id. Do bazy zapisujemy pełny zakodowany wynik, który
zawiera sól i parametry; oddzielna kolumna na sól nie jest potrzebna.
Sama tabela nie oblicza ani nie weryfikuje hasha. Jej ograniczenie sprawdza
tylko, czy pole jest obecne i nie jest puste. Nie wpisuj do niego zwykłego hasła.

## Zastosowanie migracji

Polecenia wykonuj z katalogu `server`, gdzie znajdują się `compose.yaml` i `.env`:

```powershell
docker compose up -d --wait db
Get-Content -Raw -Encoding UTF8 db/migrations/001_create_users.sql | docker compose exec -T db psql -U game_dev -d sdl_game -v ON_ERROR_STOP=1
```

Możesz też otworzyć plik migracji w HeidiSQL, połączyć się z bazą `sdl_game`
i uruchomić cały plik w zakładce zapytania SQL. `BEGIN` i `COMMIT` obejmują
zmiany jedną transakcją. Jeśli wystąpi błąd, wycofaj ją przez `ROLLBACK;`.
Istniejąca tabela `users` spowoduje błąd, zamiast zostać nadpisana lub usunięta.

Po zastosowaniu migracji odśwież listę tabel w HeidiSQL. Możesz sprawdzić:

```sql
SELECT id, username, created_at FROM public.users;
```

Tabela początkowo jest pusta. Dodawanie kont z poprawnym hashem i obsługa
logowania w Erlangu będą następnym krokiem.

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
