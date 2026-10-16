# Testy silnika

Projekt ma testy wektora `math::Vector2` oraz dwa testy integracyjne/regresyjne:
bilansu pamięci i działania pętli głównej. Błędy są zgłaszane komunikatem `FAIL` i niezerowym kodem wyjścia.
Testy pamięci i timingu uruchamiają SDL z driverami `dummy` i rendererem programowym,
więc nie wymagają otwierania okna ani fizycznego urządzenia audio. Test Vector2 nie wymaga SDL.

## Uruchamianie

Wymagania: Windows, GNU Make 4.4 lub nowszy oraz `g++` z obsługą C++17 w `PATH`.
Testy pamięci i timingu wymagają też bibliotek dostarczonych w `Engine/Vendor`.
Z katalogu projektu uruchom cały zestaw:

```shell
make test
```

Pojedynczy zestaw testów uruchomisz przez `make test-vector2`, `make test-memory`
lub `make test-frame-timing`. Każdy cel kompiluje własny plik `.exe` w `build/tests`,
a następnie go uruchamia. Testy pamięci i timingu dodatkowo kopiują tam zasoby oraz konfigurację.

`make test` wykonuje zestawy w kolejności: Vector2, pamięć, timing. Zachowuje tę kolejność
także przy `make -j test`. Błąd kompilacji lub testu kończy polecenie niezerowym kodem.
Aby kontynuować pozostałe zestawy po błędzie, użyj `make -k test`.
Podczas testu timingu unikaj osobnego równoległego budowania projektu i innych intensywnych zadań.

| Test | Źródło | Polecenie | Katalog roboczy |
|---|---|---|---|
| Wektor i obliczenia ruchu | [Vector2.cpp](Vector2.cpp) | `make test-vector2` | `build/tests/vector2` |
| Pamięć i własność zasobów | [MemoryLeaks.cpp](MemoryLeaks.cpp) | `make test-memory` | `build/tests/memory` |
| FPS i aktualizacje logiki | [FrameTiming.cpp](FrameTiming.cpp) | `make test-frame-timing` | `build/tests/frame-timing` |

## Vector2 — działania matematyczne

Uruchom `make test-vector2`. Cel kompiluje test w C++17 z ostrzeżeniami
traktowanymi jako błędy. Nie wymaga SDL, ECS ani zewnętrznego frameworka testowego.

Dziewięć nazwanych testów sprawdza konstrukcję, operatory bez modyfikacji argumentów,
operatory przypisania, długość i iloczyn skalarny, normalizację zwykłego i zerowego wektora,
normalizację bardzo dużych i małych wartości, odrzucanie dzielenia przez zero oraz ruch z `deltaTime`.
Wyniki `float` są porównywane z tolerancją. Każdy test wypisuje `PASS` albo `FAIL`;
pozostałe testy wykonują się także po błędzie. Końcowy kod wyjścia to `0` przy powodzeniu i `1` przy błędzie.

## 1. MemoryLeaks — pamięć i cykl życia

Test porównuje liczbę aktywnych alokacji z zapisaną wartością bazową:

- SDL: `SDL_GetNumAllocations()` wraz z `SDLTest_TrackAllocations()`;
- C++: licznik zwiększany przez `new` i zmniejszany przez `delete`.

Najpierw rozgrzewa biblioteki i zasoby, a potem sprawdza powrót do odpowiedniej bazy po sprzątaniu.
Aktywna aplikacja może mieć wiele poprawnie posiadanych alokacji; ważne, żeby zakończony scenariusz nie zmienił bilansu.

| Obszar | Co jest sprawdzane |
|---|---|
| Sceny | 100 serii po trzy sceny, czyli 300 przejść, z kontrolą bilansu po każdej serii |
| Odroczone przejścia | Zmiana i zamknięcie sceny z obsługi zdarzeń, `update`, `render`, `onEnter` i `onExit`; także przejście oczekujące bez aktywnej sceny |
| UI i zasoby | Zmiany tekstów, wartości Input/Select, przeładowanie obrazów i fontów, własność UI oraz wyświetlanie licznika FPS |
| Audio | WAV/OGG, błędne ścieżki i identyfikatory, cache, odtwarzanie, pause/resume, volume/mute, wyczerpanie kanałów i sprzątanie |
| Aplikacja | Destruktory, powtórne `init()`, wielokrotne `clean()` i brak pozostawionych podsystemów SDL |
| Błędy startu | Brak konfiguracji, niewłaściwy driver audio i siedem wymuszonych błędów inicjalizacji |

Siedem wymuszonych błędów obejmuje `SDL_Init`, tworzenie okna i renderera,
`TTF_Init`, `IMG_Init`, `Mix_Init` oraz `Mix_OpenAudio`. Wymuszanie odbywa się przez wrappery linkera.

![Bilans alokacji SDL i C++ po kolejnych seriach scen](docs/memory-balance.png)

Wykres pokazuje **różnicę względem bazy po sprzątnięciu każdej serii**, a nie chwilowe zapotrzebowanie podczas tworzenia scen.
Pożądany wynik to `0` dla SDL i C++. W tym przebiegu wszystkie 113 kontroli bilansu przeszły;
100 z nich dotyczyło pętli 300 przejść scen. Baza po zamknięciu aplikacji wynosiła `SDL=1`, `C++=0`.
Ta pojedyncza alokacja SDL należała już do bazy testu i nie przyrastała podczas kolejnych kontroli.

Test mierzy liczbę alokacji, nie RAM w MB ani pamięć GPU. Liczniki nie zastępują sanitizera wykrywającego błędny dostęp do pamięci.

## 2. FrameTiming — FPS i stały krok logiki

Test uruchamia rzeczywistą pętlę z `main.cpp` z własną sceną pomiarową.
Zmienia limit w trakcie działania: **1 → 5 → 10 → 30 → 60 → 120 → 144 → 240 → 5 FPS**,
a na końcu sprawdza tryb bez limitu. Powrót z 240 do 5 sprawdza również zmianę z wysokiego limitu na niski.
Każdy pomiar limitu trwa co najmniej 2,1 s.

| Warunek | Kryterium zaliczenia |
|---|---|
| Ograniczenie FPS | Zmierzony FPS mieści się między 90% a 102% wybranego limitu |
| Tempo logiki | 55–65 aktualizacji/s, niezależnie od wybranego limitu renderowania |
| Krok aktualizacji | `App::deltaTime` wynosi `1/60 s` z tolerancją `0.000001` |
| Licznik FPS | `currentFPS` różni się od wybranego limitu najwyżej o 10% |
| VSync | Renderer nie jest tworzony z `SDL_RENDERER_PRESENTVSYNC` |
| Zatrzymanie na 400 ms | Przy 60 FPS pętla nadrabia pięć aktualizacji, a następna klatka wykonuje najwyżej dwie |
| Tryb bez limitu | Zmierzony FPS i `currentFPS` przekraczają 240, a logika nadal mieści się w 55–65 aktualizacjach/s |
| Zamknięcie | `SDL_QUIT` zostaje odebrane, pętla kończy działanie i nie renderuje ponownie |

![Wybrany i zmierzony FPS oraz częstotliwość aktualizacji logiki](docs/frame-timing.png)

Górny panel porównuje limit z uzyskanym FPS; dolny pokazuje tempo logiki i dopuszczalny zakres.
Wartości pochodzą z testowej sceny na rendererze programowym. Wykres obejmuje tylko faktycznie wykonane etapy;
wyniki zależą od maszyny i warunków uruchomienia.

**W świeżym osobnym przebiegu test zakończył się `FAIL` przy 30 FPS:** uzyskał 25,49 FPS,
poniżej wymaganych 27 FPS, mimo prawidłowych 59,94 aktualizacji/s.
Wykres pokazuje cztery wykonane etapy. Dalsze limity, zatrzymanie na 400 ms i tryb bez limitu
nie zostały w tym przebiegu wykonane. Wcześniejszy pomiar równoległy także nie przeszedł progu 30 FPS;
ustalenie przyczyny powtarzającego się wyniku wymaga osobnej diagnozy.

## Jak czytać wynik

- **`PASS:` i kod `0`** — wszystkie warunki danego testu zostały spełnione.
- **`FAIL:` i kod `1`** — komunikat wskazuje pierwsze niespełnione sprawdzenie.
- W teście pamięci komunikaty o brakujących plikach lub wymuszonych błędach SDL są częścią scenariuszy negatywnych. O wyniku decyduje końcowe `PASS`/`FAIL`.
- `Outstanding allocations changed` oznacza zmianę bilansu. Log podaje wartości SDL i C++ przed scenariuszem oraz po nim.
- `Measured FPS does not match the selected limit` może wystąpić przy przeciążeniu komputera. Powtórz pomiar osobno; jeśli błąd się powtarza, sprawdź timing pętli.

## Dane wykresów

Obrazy PNG są zapisanym przykładem z **13 października 2026**, a nie automatycznie aktualizowanym raportem.
Dane źródłowe: [bilans pamięci](docs/memory-measurements.csv) i [timing](docs/frame-timing-measurements.csv).
Do wykresu pamięci zebrano liczniki z tymczasowej kopii testu, która dopisuje pomiary do logu i zachowuje jego asercje.
Wykres timingu pochodzi z osobnego przebiegu zakończonego `FAIL` przy 30 FPS.

Testy obejmują scenariusze opisane powyżej. Renderowanie na GPU, wygląd UI na monitorze i rzeczywiste przełączanie fullscreen wymagają oddzielnego sprawdzenia.
