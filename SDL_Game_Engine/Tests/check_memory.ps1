$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    Copy-Item -Recurse -Force Game/Assets build
    Copy-Item -Recurse -Force Game/Config build
    $compilerArgs = @(
        '-std=c++17', '-g', '-static', '-static-libstdc++',
        '-I', 'Engine/Vendor/SDL2/include/SDL2', '-L', 'Engine/Vendor/SDL2/lib',
        '-o', 'build/memory_leaks.exe', 'Tests/MemoryLeaks.cpp',
        'Engine/Core/App.cpp', 'Engine/Audio/AudioManager.cpp',
        'Engine/Renderer/TextureManager.cpp',
        'Engine/Scenes/SceneManager.cpp', 'Game/Scenes/Scene1.cpp',
        'Game/Scenes/Scene2.cpp', 'Game/Scenes/SettingsScene.cpp',
        '-Wl,--wrap=SDL_Init,--wrap=SDL_CreateWindow,--wrap=SDL_CreateRenderer',
        '-Wl,--wrap=TTF_Init,--wrap=IMG_Init,--wrap=Mix_Init,--wrap=Mix_OpenAudio',
        '-lmingw32', '-lSDL2_test', '-lSDL2', '-lSDL2_image', '-lSDL2_mixer', '-lSDL2_ttf',
        '-lsetupapi', '-lversion', '-limm32', '-lole32', '-loleaut32', '-lwinmm',
        '-lgdi32', '-luser32', '-lshell32', '-ladvapi32', '-lRpcrt4'
    )
    & g++ @compilerArgs
    if($LASTEXITCODE -ne 0) { throw 'Memory check compilation failed.' }
    Push-Location build
    try {
        & ./memory_leaks.exe
        if($LASTEXITCODE -ne 0) { throw 'Memory check failed.' }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
}
