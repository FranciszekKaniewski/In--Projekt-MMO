cmake_minimum_required(VERSION 3.20)
project(SDL_Game_Engine)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
        "Engine/*.cpp" "Engine/*.h"
        "Game/*.cpp"   "Game/*.h"
        "main.cpp"
        )

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

if(WIN32)
    target_link_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/SDL2/lib")
    target_link_libraries(${PROJECT_NAME} PRIVATE mingw32 SDL2main SDL2 SDL2_image SDL2_mixer SDL2_ttf
        setupapi version imm32 ole32 oleaut32 winmm gdi32 user32 shell32 advapi32 Rpcrt4
        ws2_32 secur32 crypt32)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
        Vendor/SDL2/include/SDL2
        Vendor
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
