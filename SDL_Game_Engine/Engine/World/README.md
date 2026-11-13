# Mapa świata

`Map` wczytuje tilemapy Tiled (`.tmj`) oraz przestrzenny eksport taki jak
`Map_example.json` z odpowiadającym mu obrazem.

```cpp
#include "Engine/World/Map.h"

// Pole sceny:
Map map;

// Po pobraniu pliku TMJ i utworzeniu renderera przez App:
if (!map.loadTilemap(downloadedTmj, "Assets/Graphics/TileSet_001.png")) {
    std::cerr << map.error() << '\n';
}
map.position = {0.0f, 0.0f}; // Lewy górny narożnik w świecie.
map.scale = 1.0f;           // Jeden piksel obrazu odpowiada jednostce świata.

// W renderWorld(App& app), kamera aktywnej GameScene:
map.draw(app.renderer, camera());
```

Po kliknięciu `Play` klient pobiera TMJ z serwera, składa mapę przez
`loadTilemap()` z lokalnym `Assets/Graphics/TileSet_001.png` i przekazuje ją
do `Scene2` razem z danymi postaci. Pobieranie korzysta z sesji TLS.

`Map_expanded.tmj` ma 2560 × 2560 px, czyli 160 × 160 pól
po 16 × 16 px. To dwukrotnie większa szerokość i wysokość niż w przykładzie (czterokrotnie
większa powierzchnia), bez powiększania pikseli grafiki. W scenie, przy skali 1, świat ma
2560 × 2560 jednostek, a pole zajmuje 16 × 16 jednostek.
Kamera pokazuje fragment 1280 × 720 i zaczyna przy centralnej polanie.
WASD lub strzałki poruszają graczem. Kamera śledzi go i zatrzymuje się na granicach mapy.
Obiekty ECS są rysowane na mapie przez `GameScene`. Zmiana rozdzielczości okna korzysta ze wspólnej
projekcji silnika, bez zmiany rozmiaru świata ani zasięgu kamery w jednostkach świata.

## Tilemapy Tiled

`Map_expanded.tmj` i `Map_example2.tmj` znajdują się w `server/priv/maps`
i używają tego samego atlasu z zasobów klienta.
Loader obsługuje skończone mapy ortogonalne z jednym osadzonym opisem atlasu
oraz wyrównanymi warstwami kafelków o rozmiarze mapy. Tablice `data` zawierają
liczbowe GID-y bez flag odbicia. `0` oznacza puste pole. Warstwy są składane
w kolejności zapisu, z uwzględnieniem widoczności i przezroczystości.
Loader składa je raz do jednej tekstury rysowanej przez kamerę.

## Eksport obrazowy

JSON zawiera tablicę `frames`, gdzie `frame.x/y/w/h` opisują pola gotowego obrazu,
oraz `meta.size.w/h`, określające rozmiar całej mapy.
Wszystkie pola muszą mieć jednakowe dodatnie wymiary i tworzyć pełną siatkę bez duplikatów.
Obsługiwane są pola bez obrotu. `duration`, `filename`, `sourceSize`, `spriteSourceSize`,
`trimmed` i nazwy warstw nie sterują animacją ani położeniem mapy.

Przekazany eksport nie zawiera identyfikatorów kafelków odwołujących się do `TileSet_001.png`.
Sam JSON nie wystarcza do odtworzenia grafiki. `Map_example.png` został wyeksportowany z pliku
`Map_example.aseprite` i umieszczony razem z JSON-em w `Tests/fixtures/maps`.
W obrazie są już złożone wszystkie warstwy. Mapa jest rysowana jednym wywołaniem SDL z prostokątem
źródłowym obejmującym widok kamery, bez tysięcy encji i bez szczelin między kafelkami.
Poza widokiem nie wysyła polecenia rysowania. Przy ułamkowym przesunięciu wycinek jest poszerzany
do całych pikseli źródłowych (maksymalnie o jeden na krawędź), a nadmiar przycina viewport SDL.
Cała tekstura jest wczytywana raz; ta wersja nie strumieniuje fragmentów mapy z dysku.
Wywołanie `draw(renderer)` bez kamery nadal rysuje całą mapę pod podaną pozycją i skalą.

Grafika jest wybierana w następującej kolejności:

1. Drugi argument `load(jsonPath, imageOverride)`, jeżeli został podany.
2. `meta.image`, względem katalogu JSON-a.
3. Nazwa JSON-a z rozszerzeniem `.png`, np. `Map_example.json` → `Map_example.png`.

Jawny `imageOverride` jest ścieżką względem katalogu uruchomienia lub ścieżką absolutną.
Wymiary obrazu muszą zgadzać się z `meta.size`. Tekstura korzysta z filtrowania nearest.
`make build` kopiuje zasoby z `Game/Assets` do `build/Assets`.

## Własność i błędy

- `load()` zwraca `false` i zapisuje przyczynę w `error()` po błędzie pliku, JSON-a lub grafiki.
  Nieudane przeładowanie zachowuje dotychczasową mapę.
- `clear()` zwalnia mapę. Tekstura jest również zwalniana automatycznie przy zniszczeniu sceny.
  Mapa musi zostać zniszczona przed rendererem, tak samo jak sprite'y.
- `worldSize()` zwraca rozmiar po zastosowaniu `scale`; `width()/height()` zwracają wymiary obrazu.
- `columns()/rows()`, `tileWidth()/tileHeight()` i `tileCount()` opisują siatkę eksportu.
- Dodatnia skończona `scale` i skończona `position` są wymagane do rysowania.

`make test-map` sprawdza piksele, pozycję, skalowanie, składanie warstw,
zachowanie poprzedniej mapy po błędzie oraz wczytywanie obu plików TMJ.
