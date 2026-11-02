# Czat i dymki

`Scene2` włącza czat przez `enableChat(player)`. Scena nadal tylko składa świat;
`GameScene` przekazuje zdarzenia do nakładki, blokuje lokalne sterowanie podczas pisania,
aktualizuje czas dymków i rysuje je po postaciach, przed panelem czatu i pozostałym HUD-em.

- Enter otwiera pole; kolejny Enter wysyła wiadomość i zamyka pole.
- Kliknięcie pola również rozpoczyna pisanie. Escape czyści szkic i zamyka pole;
  następny Escape może obsłużyć scena, np. wrócić do menu.
- Utrata fokusu okna zamyka edycję. Podczas edycji WASD i strzałki nie poruszają lokalnym graczem.
- Ctrl+V wkleja tekst. Kółko myszy nad panelem przewija historię.
- Poza edycją widać tylko wiadomości, z delikatnym cieniem ułatwiającym czytanie na mapie.
  Podczas pisania pojawia się tło o przezroczystości 50%, nagłówek, licznik i pole edycji.
  Tekst czatu, dymków i pola jest rasteryzowany w dwukrotnej rozdzielczości i wygładzany
  przy wyświetlaniu, z zachowaniem rozmiaru w jednostkach UI.
- Limit wynosi 256 punktów kodowych Unicode, nie bajtów UTF-8. Polskie litery liczą się jako jeden
  znak; złożone emoji lub litery z osobnym znakiem łączącym mogą obejmować kilka punktów kodowych.
  Limit obejmuje wpisywanie, wklejanie, programowe ustawienie pola, wysyłanie i odbiór.
- Puste wiadomości nie są dodawane. Tekst jest jednoliniowy, dłuższe wiadomości są zawijane
  przy rysowaniu. Historia przechowuje ostatnie 100 wiadomości.

## Warstwy

`ChatSession` w `Chat.h/.cpp` przechowuje historię i obsługuje wysyłanie/odbiór bez SDL,
fontów i renderera. `ChatOverlay` odpowiada za panel w lewym dolnym rogu, edycję, przewijanie,
buforowanie tekstur tekstu i projekcję dymków względem kamery.

`ChatSpeakerComponent` dodaje nazwę postaci, oznaczenie NPC i offset dymka względem pozycji.
Fabryka gracza dodaje go automatycznie z nazwą `Gracz`. Tak samo można oznaczyć dowolnego NPC:

```cpp
auto& speaker = world.addComponent<ChatSpeakerComponent>(npc);
speaker.name = u8"Strażnik";
speaker.npc = true;
speaker.bubbleOffset = {0.0f, -64.0f};
```

Odebrana wypowiedź takiej encji dodaje lub odświeża `SpeechBubbleComponent`.
Nowa wypowiedź zastępuje poprzedni dymek. Domyślnie trwa on 8 sekund, a ostatnią sekundę zanika.
Renderer pokazuje go, gdy mówiąca postać jest nie dalej niż 240 jednostek świata od lokalnego
gracza. Dotyczy to również własnej postaci. Zasięg jest sprawdzany podczas rysowania, więc
podejście lub odejście zmienia widoczność. Wiadomości serwera pozostają wyłącznie w panelu.

Parametry można ustawić przez `chatSession().bubbleRange` i `chatSession().bubbleLifetime`.
Granice mapy nie wpływają na położenie panelu, natomiast dymki poruszają się wraz z postaciami
i kamerą. Długie wypowiedzi mają szerszy dymek, aby zmieścić więcej tekstu nad głową.

## Odbiór i przyszły transport

Gra nie ma jeszcze połączenia sieciowego. Bez transportu lokalne wysłanie natychmiast trafia
do wspólnego mechanizmu odbioru, historii i dymków. Komunikat powitalny `Serwer` pochodzi
z lokalnego systemu gry.

Publiczne API `GameScene` pozwala dostarczać wiadomości z logiki gry lub przyszłego klienta:

```cpp
gameScene.receiveServerMessage(u8"Restart świata za 5 minut.");
gameScene.receiveChatMessage({ChatSource::Player, remoteEntity, "Nick", u8"Cześć!"});
gameScene.receiveChatMessage({ChatSource::NPC, npcEntity, "", u8"Witaj podróżniku."});
```

Jeżeli encja ma `ChatSpeakerComponent`, nazwa i rodzaj nadawcy są pobierane z niej.
Wiadomość od już usuniętej lub nieznanej encji może nadal trafić do historii, ale nie tworzy dymka.

`chatSession().setSendHandler(callback)` przekierowuje wysłane wiadomości do transportu.
Wtedy historia i dymek pojawiają się po dostarczeniu potwierdzonej wiadomości przez
`receiveChatMessage()`, co unika podwójnego lokalnego echa. Transport musi zmapować sieciowy
identyfikator nadawcy na lokalne `ecs::Entity` i wywołać odbiór na wątku symulacji.
Numery lokalnych encji nie są identyfikatorami sieciowymi.

Testy: `make test-chat` obejmuje model i UI; `make test-unit` zawiera dodatkowo walidację UTF-8.
Podglądy są zapisywane w `build/tests/integration/chat-scene-preview.bmp`
i `chat-long-message-preview.bmp`.
Stan podczas pisania: `build/tests/integration/chat-editing-preview.bmp`.
