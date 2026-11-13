#pragma once

#include "../../Engine/Scenes/SceneList.h"
#include "../../Engine/World/Map.h"
#include "../../Engine/Network/GameClient.h"
#include <catch_amalgamated.hpp>
#include <utility>

inline constexpr char serverMaps[] = "../../../../server/priv/maps";

inline Scene* createGameSceneFixture() {
    Map map;
    REQUIRE(map.load(std::filesystem::path(serverMaps) / "Map_expanded.tmj", "Assets/Graphics/TileSet_001.png"));
    CharacterState character;
    character.name = "Gracz";
    character.x = 1350;
    character.y = 1320;
    return createNetworkGameScene(std::move(map), std::move(character));
}
