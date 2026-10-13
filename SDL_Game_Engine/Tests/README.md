# Testy silnika

Projekt ma dwa testy integracyjne/regresyjne: bilansu pamięci i działania pętli głównej.
Używają kodu silnika oraz bibliotek SDL, a błędy zgłaszają przez asercje `require()` i niezerowy kod wyjścia.
Oba uruchamiają SDL z driverami `dummy` i rendererem programowym, więc nie wymagają otwierania okna ani fizycznego urządzenia audio.

## Uruchamianie

Wymagania: Windows, PowerShell, `g++` z obsługą C++17 w `PATH` i biblioteki dostarczone w `Engine/Vendor`.
Z katalogu projektu uruchom kolejno:

```powershell
.\Tests\check_memory.ps1
.\Tests\check_frame_timing.ps1
```

Każdy skrypt kompiluje własny plik `.exe`, kopiuje zasoby i konfigurację do `build/tests`,
a następnie uruchamia test w przygotowanym katalogu. Przy błędzie kompilacji lub testu skrypt zgłasza wyjątek.
Test timingu uruchamiaj osobno, bez równoległego budowania projektu i innych intensywnych zadań.

| Test | Źródło | Skrypt | Katalog roboczy |
|---|---|---|---|
| Pamięć i własność zasobów | [MemoryLeaks.cpp](MemoryLeaks.cpp) | [check_memory.ps1](check_memory.ps1) | `build/tests/memory` |
| FPS i aktualizacje logiki | [FrameTiming.cpp](FrameTiming.cpp) | [check_frame_timing.ps1](check_frame_timing.ps1) | `build/tests/frame-timing` |

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
