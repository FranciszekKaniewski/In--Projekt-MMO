#pragma once

class Scene;
class Map;
struct CharacterState;

Scene* createScene1();
Scene* createCharactersScene();
Scene* createNetworkGameScene(Map map, CharacterState character);
Scene* createSettingsScene();
