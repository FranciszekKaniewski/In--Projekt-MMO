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

`GameScene::render()` czyści tło, wywołuje `renderWorld(app)`, rysuje hitboxy i na końcu HUD.
W scenach pochodnych rysuj świat przez nadpisanie `renderWorld(App&)`, aby zachować tę kolejność.
Jeżeli nadpisujesz całe `render()`, musisz sam wywołać `renderColliderDebug(world, app.renderer, app.devMode)`.

Obrysy są zielone, widoczne tylko przy `app.devMode == true`. Klawisz F1 przełącza ten tryb.
`Scene2` zawiera przykładowy kwadrat 128 × 128, koło o promieniu 64 oraz pięciokąt.
Renderer korzysta z logicznych współrzędnych SDL. Gdy dodasz kamerę, trzeba będzie uwzględnić jej
przesunięcie również podczas rysowania hitboxów.

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

Sam komponent przechowuje dane. Aby uzyskać ruch, w `update()` sceny lub w systemie ruchu
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
