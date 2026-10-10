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
        '-o', 'build/frame_timing.exe', 'Tests/FrameTiming.cpp',
        'Engine/Core/App.cpp', 'Engine/Audio/AudioManager.cpp',
        'Engine/Renderer/TextureManager.cpp', 'Engine/Scenes/SceneManager.cpp',
        '-Wl,--wrap=SDL_CreateRenderer',
        '-lmingw32', '-lSDL2', '-lSDL2_image', '-lSDL2_mixer', '-lSDL2_ttf',
        '-lsetupapi', '-lversion', '-limm32', '-lole32', '-loleaut32', '-lwinmm',
        '-lgdi32', '-luser32', '-lshell32', '-ladvapi32', '-lRpcrt4'
    )
    & g++ @compilerArgs
    if($LASTEXITCODE -ne 0) { throw 'Frame timing check compilation failed.' }
    Push-Location build
    try {
        & ./frame_timing.exe
        if($LASTEXITCODE -ne 0) { throw 'Frame timing check failed.' }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
}
