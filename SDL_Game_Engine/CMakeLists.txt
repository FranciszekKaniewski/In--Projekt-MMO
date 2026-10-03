cmake_minimum_required(VERSION 3.20)
project(SDL_Game_Engine)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE ALL_SOURCES
        "Engine/*.cpp" "Engine/*.h"
        "Game/*.cpp"   "Game/*.h"
        "main.cpp"
        )

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
        Engine/Vendor/SDL2/include/SDL2
        Engine/Vendor
        Engine/Core
        Engine/ECS
        Engine/Audio
        Engine/Input
        Engine/Physics
        Engine/Renderer
        Game
        Game/Scenes
        Game/Scripts
        Game/UI
        )