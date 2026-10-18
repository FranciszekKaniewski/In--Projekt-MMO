# Komponenty świata

`TransformComponent` przechowuje pozycję obiektu w jednostkach świata.
`VelocityComponent` przechowuje nazwane wektory prędkości w jednostkach świata na sekundę.
`total()` zwraca ich sumę; pusty komponent oznacza prędkość `(0, 0)`.
`ColliderComponent` opisuje hitbox niezależnie od SDL i wyglądu obiektu.
Na tym etapie dostępne są obrysy; wykrywanie i rozwiązywanie zderzeń nie jest jeszcze zaimplementowane.

```cpp
#include "Engine/ECS/Components/TransformComponent.h"
#include "Engine/ECS/Components/ColliderComponent.h"

// Przykład wewnątrz GameScene::onEnter().
const auto entity = world.createEntity();
world.addComponent<TransformComponent>(entity, math::Vector2{200.0f, 300.0f});
world.addComponent<ColliderComponent>(entity, ColliderComponent::square(64.0f));
```

- `square(side, offset)` tworzy kwadrat o zadanej długości boku.
- `circle(radius, offset)` tworzy koło o zadanym promieniu.
- `polygon(vertices, offset)` tworzy zamknięty obrys z co najmniej trzech punktów.
- `box({width, height}, offset)` pozwala także ustawić różną szerokość i wysokość.
- Środek kwadratu, prostokąta i koła to `transform.position + collider.offset`.
  Kwadrat o boku 64 i środku `(200, 300)` ma lewy górny narożnik `(168, 268)`.
- Wierzchołki wielokąta są lokalne: punkt świata to `transform.position + collider.offset + vertex`.
  Punkty są łączone w podanej kolejności, a ostatni jest automatycznie łączony z pierwszym.
  Nie trzeba powtarzać pierwszego punktu na końcu. Nie ma stałego limitu liczby punktów.
  Obsługiwane są obrysy wypukłe i wklęsłe; punkty nie są sortowane ani automatycznie centrowane.
  Walidacja nie sprawdza pola figury ani samoprzecięć.
- `enabled = false` wyłącza collider z podglądu.
- Fabryki odrzucają zerowe, ujemne i nieskończone rozmiary oraz wartości NaN.
  Offset musi być skończony. Niepoprawne dane zmienione bezpośrednio w polach są pomijane podczas rysowania.
- Kształty nie mają jeszcze obrotu ani skalowania. Pozycja nie jest zaokrąglana do liczb całkowitych.

Przykładowy trójkąt (większa liczba punktów korzysta z tego samego API):

```cpp
world.addComponent<ColliderComponent>(entity, ColliderComponent::polygon({
    {-32.0f, 24.0f}, {0.0f, -32.0f}, {32.0f, 24.0f}
}));
```

Przykład zakłada encję z `TransformComponent`, bez dotychczasowego `ColliderComponent`.

## Podgląd developerski

`GameScene::render()` czyści tło, wywołuje `renderWorld(app)`, rysuje sprite'y, hitboxy i na końcu HUD.
W scenach pochodnych rysuj świat przez nadpisanie `renderWorld(App&)`, aby zachować tę kolejność.
Jeżeli nadpisujesz całe `render()`, musisz sam wywołać `renderColliderDebug(world, app.renderer, app.devMode)`.

Obrysy są zielone, widoczne tylko przy `app.devMode == true`. Klawisz F1 przełącza ten tryb.
`Scene2` zawiera przykładowy kwadrat 128 × 128, koło o promieniu 64 oraz pięciokąt.
Renderer korzysta z logicznych współrzędnych SDL. Gdy dodasz kamerę, trzeba będzie uwzględnić jej
przesunięcie również podczas rysowania hitboxów.

Świat i UI korzystają ze stałej bazy `App::logicalSize` (1280 × 720).
Zmiana rozdzielczości okna skaluje sprite'y i obrysy colliderów razem, zachowując ich proporcje.
Przy 1920 × 1080 obiekt o boku 128 jednostek zajmuje 192 piksele, a przy 2560 × 1440 — 256 pikseli.
Inne proporcje ekranu dodają pasy, zamiast rozciągać koła w elipsy.
Obrysy zachowują grubość jednego fizycznego piksela, również przy pomniejszaniu obrazu.
Pozycje, prędkości, rozmiary colliderów i rozmiary klatek źródłowych pozostają w swoich jednostkach;
nie mnoż ich ponownie przez skalę okna. Rozmiar przestrzeni logicznej zwraca `app.getUISize()`,
a fizyczny rozmiar okna `app.getWindowSize()`. Testy projekcji: `make test-resolution`.

## Prędkość

Komponent dodaj tylko encjom, które mają się poruszać. Przykład dla istniejącej encji `circle`:

```cpp
#include "Engine/ECS/Components/VelocityComponent.h"

auto& velocity = world.addComponent<VelocityComponent>(circle);
velocity.vectors["movement"] = {120.0f, 0.0f};
velocity.vectors["wind"] = {0.0f, 30.0f};
velocity.vectors["knockback"] = {-40.0f, 0.0f};
// Suma prędkości: (80, 30) jednostek na sekundę.

velocity.vectors["movement"] = {60.0f, 0.0f}; // Zastępuje poprzedni wektor ruchu.
velocity.vectors.erase("knockback");          // Usuwa tylko odrzut.
```

Sam komponent przechowuje dane. Aby uzyskać ruch, w `updateWorld()` sceny lub w systemie ruchu
aktualizuj pozycje encji mających oba komponenty:

```cpp
world.forEach<TransformComponent, VelocityComponent>(
    [dt = App::deltaTime](ecs::Entity, TransformComponent& transform, const VelocityComponent& velocity) {
        transform.position += velocity.total() * dt;
    });
```

Collider korzysta z pozycji w `TransformComponent`, więc jego obrys podąża za poruszającą się encją.
Wektory pozostają aktywne do ich zmiany lub usunięcia. `velocity.vectors.clear()` usuwa wszystkie źródła.
Nie dodawaj nowego wpisu co klatkę dla tego samego źródła — aktualizuj jego wartość pod tą samą nazwą.
Są to prędkości, a nie jednorazowe przesunięcia; system mnoży ich sumę przez `deltaTime`.

Testy: `make test-collider`. Sprawdzają rzeczywiste piksele renderera programowego bez otwierania okna.
Sumowanie prędkości i ruch encji: `make test-velocity`.

## Sprite i animacja

`SpriteComponent` przechowuje współdzieloną teksturę, prostokąt źródłowy w pikselach obrazu,
rozmiar wyświetlania w jednostkach świata, offset, odbicie SDL i widoczność.
Środek sprite'a to `transform.position + sprite.offset`, tak jak w colliderach typu box i circle.
`source` domyślnie wybiera klatkę 64 × 64, a `size` ustawia jej rozmiar w świecie.
Rozmiar grafiki nie zmienia collidera. Sprite statyczny nie potrzebuje `AnimationComponent`.

Przykład wewnątrz `onEnter()` sceny, po dodaniu własnego spritesheetu do `Game/Assets/Graphics/`:

```cpp
#include "Engine/ECS/Components/SpriteComponent.h"
#include "Engine/ECS/Components/AnimationComponent.h"
#include "Engine/Renderer/TextureManager.h"

auto texture = TextureManager::LoadSharedTexture("Assets/Graphics/player.png");
if (!texture) return; // Loader wypisuje przyczynę błędu.

const auto player = world.createEntity();
world.addComponent<TransformComponent>(player, math::Vector2{200.0f, 300.0f});
auto& sprite = world.addComponent<SpriteComponent>(player);
sprite.texture = texture;
sprite.source = {0, 0, 32, 48};
sprite.size = {64.0f, 96.0f};
sprite.flip = SDL_FLIP_NONE; // Także SDL_FLIP_HORIZONTAL lub SDL_FLIP_VERTICAL.

auto& animation = world.addComponent<AnimationComponent>(player);
animation.row = 1;            // Wiersze liczone od zera.
animation.frameCount = 4;     // Kolejne klatki od pierwszej kolumny.
animation.frameDuration = 0.2; // Sekundy na klatkę.
```

Przykład zakłada równe, przylegające klatki bez marginesów; obraz musi zawierać cały wskazany wiersz.
Renderer pomija brakujące tekstury, niepoprawne rozmiary i wycinki poza granicami obrazu.
Współrzędne rysowania zachowują część ułamkową. Nie ma jeszcze kamery ani sortowania warstw.

`LoadSharedTexture()` używa `SDL_DestroyTexture` po zwolnieniu ostatniego uchwytu.
Przypisuj tę samą wartość `texture` kolejnym sprite'om korzystającym z tej grafiki.
Loader nie ma cache — każde wywołanie wczytuje nową teksturę. Nie twórz niezależnych
`shared_ptr` z tego samego surowego wskaźnika. Wszystkie uchwyty muszą zostać zwolnione przed
zniszczeniem renderera; komponenty świata są niszczone razem ze sceną przed `App::clean()`
zwalniającym renderer.

`GameScene::update()` wywołuje `updateWorld(app)`, a następnie `updateAnimations(world, App::deltaTime)`.
W scenach pochodnych nadpisuj `updateWorld(App&)`, aby animacje były aktualizowane automatycznie.
`GameScene::render()` automatycznie wywołuje `renderSprites(world, app.renderer)` po `renderWorld()`.
Przy nadpisywaniu całych `update()` lub `render()` trzeba samodzielnie zapewnić te wywołania.
Własne sceny mogą też korzystać bezpośrednio z `Engine/ECS/AnimationSystem.h`
oraz `Engine/Renderer/SpriteRenderer.h`.

- `animation.playing = false` wstrzymuje animację, zachowując klatkę i pozostały czas.
- `animation.loop = false` odtwarza ją raz i zatrzymuje na ostatniej klatce.
- `animation.restart()` ustawia pierwszą klatkę, zeruje zegar i wznawia odtwarzanie.
  Prostokąt źródłowy zostaje zsynchronizowany przy następnej aktualizacji, także z `deltaTime == 0`.
- Przy zmianie animacji ustaw `row`, `frameCount`, `frameDuration` i wywołaj `restart()`.
  Nie restartuj tej samej animacji w każdej aktualizacji.
- `sprite.visible = false` wyłącza rysowanie, ale nie zatrzymuje animacji.
- Aktualizacja zachowuje nadmiar czasu i obsługuje wiele klatek w stałym czasie.
  Ujemny lub nieskończony `deltaTime`, NaN i niepoprawne dane komponentu są pomijane.

Testy: `make test-sprite` obejmuje piksele renderera programowego, animacje i współdzielenie tekstur.
`make test-animation` uruchamia tylko przypadki dotyczące animacji.
