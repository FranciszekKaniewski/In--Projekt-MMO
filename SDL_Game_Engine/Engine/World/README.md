# Mapa świata

`Map` wczytuje przestrzenny eksport taki jak dostarczony `Map_example.json` i odpowiadający mu obraz.
Przykład zawiera siatkę 80 × 80 pól po 16 × 16 px, o wymiarach 1280 × 1280 px.

```cpp
#include "Engine/World/Map.h"

// Pole sceny:
Map map;

// W onEnter(), po utworzeniu renderera przez App:
if (!map.load("Assets/Maps/Map_example.json")) {
    std::cerr << map.error() << '\n';
}
map.position = {0.0f, 0.0f}; // Lewy górny narożnik w świecie.
map.scale = 1.0f;           // Jeden piksel obrazu odpowiada jednostce świata.

// W renderWorld(App& app), kamera aktywnej GameScene:
map.draw(app.renderer, camera());
```

`Scene2` korzysta z nowej mapy `Map_expanded.json`: 2560 × 2560 px, czyli 160 × 160 pól
po 16 × 16 px. To dwukrotnie większa szerokość i wysokość niż w przykładzie (czterokrotnie
większa powierzchnia), bez powiększania pikseli grafiki. W scenie, przy skali 1, świat ma
2560 × 2560 jednostek, a pole zajmuje 16 × 16 jednostek.
Kamera pokazuje fragment 1280 × 720 i zaczyna przy centralnej polanie.
Strzałki przesuwają kamerę, która zatrzymuje się na granicach mapy.
Obiekty ECS są rysowane na mapie przez `GameScene`. Zmiana rozdzielczości okna korzysta ze wspólnej
projekcji silnika, bez zmiany rozmiaru świata ani zasięgu kamery w jednostkach świata.

## Rozbudowana wyspa

`Game/Assets/Maps/Map_expanded.png` wykorzystuje wyłącznie oryginalny `TileSet_001.png`:
wodę, ziemię, trawę i ich krawędzie w natywnym rozmiarze. `Map_example.png` jest wzorem
zarysu wyspy. Rozbudowany teren zawiera plaże, jezioro ze strumieniem, mniejszy staw,
sieć ścieżek, przejście z lądu przez strumień oraz małe wysepki. Kafelki zachowują oryginalne
kolory i przezroczystość; generator nie dorysowuje grafiki ani dekoracji.
Całość pozostaje jednym obrazem zgodnym z loaderem `Map`.
Oryginalne pliki `Map_example.*` pozostają dostępne.

Źródłem nowej mapy jest `Tools/generate_expanded_map.py`. Ponowny eksport:

```powershell
python Tools/generate_expanded_map.py
make build
```

Generator wymaga pakietu Pillow i zawsze odtwarza ten sam układ terenu.
Punkty ścieżek, kształt jeziora, strumienia i wysepek są edytowalne w skrypcie.
Podglądy całej wyspy i centralnego obszaru trafiają do `build/map-preview`.
Python i Pillow nie są potrzebne do uruchamiania gry.
Ta mapa nie dodaje kolizji ani interaktywnych obiektów.

## Format i grafika

JSON zawiera tablicę `frames`, gdzie `frame.x/y/w/h` opisują pola gotowego obrazu,
oraz `meta.size.w/h`, określające rozmiar całej mapy.
Wszystkie pola muszą mieć jednakowe dodatnie wymiary i tworzyć pełną siatkę bez duplikatów.
Obsługiwane są pola bez obrotu. `duration`, `filename`, `sourceSize`, `spriteSourceSize`,
`trimmed` i nazwy warstw nie sterują animacją ani położeniem mapy.

Przekazany eksport nie zawiera identyfikatorów kafelków odwołujących się do `TileSet_001.png`.
Sam JSON nie wystarcza do odtworzenia grafiki. `Map_example.png` został wyeksportowany z pliku
`Map_example.aseprite` i umieszczony razem z JSON-em w `Game/Assets/Maps`.
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
`make build` kopiuje oba pliki do `build/Assets/Maps`. Aseprite nie jest potrzebny do uruchomienia gry.

## Własność i błędy

- `load()` zwraca `false` i zapisuje przyczynę w `error()` po błędzie pliku, JSON-a lub grafiki.
  Nieudane przeładowanie zachowuje dotychczasową mapę.
- `clear()` zwalnia mapę. Tekstura jest również zwalniana automatycznie przy zniszczeniu sceny.
  Mapa musi zostać zniszczona przed rendererem, tak samo jak sprite'y.
- `worldSize()` zwraca rozmiar po zastosowaniu `scale`; `width()/height()` zwracają wymiary obrazu.
- `columns()/rows()`, `tileWidth()/tileHeight()` i `tileCount()` opisują siatkę eksportu.
- Dodatnia skończona `scale` i skończona `position` są wymagane do rysowania.

Ten format nie ma danych kolizji, punktów startowych ani oddzielnych warstw do rysowania przed graczem.
Ich nazwy w `meta.layers` nie zawierają układu pól. Kolizje, drzewa z HP i edycja pojedynczych kafelków
wymagają dodatkowych danych; loader nie odgaduje ich z kolorów obrazu.

Testy: `make test-map` sprawdza rzeczywiste piksele, pozycję, skalowanie, walidację,
zachowanie poprzedniej mapy po błędzie oraz dostarczony eksport wyspy.
