.PHONY: build run bar all clean test test-unit test-integration test-e2e test-vector2 test-velocity test-collider test-memory test-frame-timing
.NOTPARALLEL:

CXX := g++
CXXFLAGS := -std=c++17 -g -static -static-libstdc++ -I Vendor -I Vendor/SDL2/include/SDL2 -isystem Vendor/Catch2
ENGINE := $(wildcard Engine/*.cpp Engine/*/*.cpp)
GAME := $(wildcard Game/*/*.cpp)
HEADERS := $(wildcard Engine/*/*.h Engine/*/*/*.h Engine/*/*/*/*.h Tests/support/*.h Vendor/nlohmann/*.hpp Vendor/SDL2/include/SDL2/*.h)
SDL_LIBS := -L Vendor/SDL2/lib -lmingw32 -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf \
	-lsetupapi -lversion -limm32 -lole32 -loleaut32 -lwinmm -lgdi32 -luser32 -lshell32 -ladvapi32 -lRpcrt4
TEST_COMMON := build/tests/catch.o build/tests/report.o
MEMORY_HOOKS := -Wl,--wrap=SDL_Init,--wrap=SDL_CreateWindow,--wrap=SDL_CreateRenderer \
	-Wl,--wrap=TTF_Init,--wrap=IMG_Init,--wrap=Mix_Init,--wrap=Mix_OpenAudio

build:
	@if not exist build mkdir build
	@xcopy /E /I /H /Y "Game\Assets" "build\Assets" >nul
	@if not exist "build\Config" mkdir "build\Config"
	@if not exist "build\Config\settings.json" copy /Y "Game\Config\settings.json" "build\Config\settings.json" >nul
	$(CXX) $(CXXFLAGS) -o build/SDL_Game_Engine.exe main.cpp $(ENGINE) $(GAME) -lmingw32 -L Vendor/SDL2/lib -lSDL2main $(SDL_LIBS)

run:
	cd build && SDL_Game_Engine.exe

bar: build run

all: build build/tests/unit/tests.exe build/tests/integration/tests.exe build/tests/e2e/tests.exe

clean:
	@for %%F in (build\SDL_Game_Engine.exe build\tests\catch.o build\tests\report.o build\tests\unit\tests.exe build\tests\integration\tests.exe build\tests\e2e\tests.exe) do @if exist "%%F" del /Q "%%F"

build/tests/catch.o: Vendor/Catch2/catch_amalgamated.cpp Vendor/Catch2/catch_amalgamated.hpp
	@if not exist "build\tests" mkdir "build\tests"
	$(CXX) $(CXXFLAGS) -DCATCH_AMALGAMATED_CUSTOM_MAIN -c $< -o $@

build/tests/report.o: Tests/support/Report.cpp Tests/support/Measurements.h Vendor/Catch2/catch_amalgamated.hpp Vendor/nlohmann/json.hpp
	@if not exist "build\tests" mkdir "build\tests"
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/tests/unit/tests.exe: $(TEST_COMMON) $(wildcard Tests/unit/*.cpp) Engine/ECS/ECS.cpp $(HEADERS)
	@if not exist "build\tests\unit" mkdir "build\tests\unit"
	$(CXX) $(CXXFLAGS) -Wall -Wextra -Wpedantic -Werror -o $@ $(wildcard Tests/unit/*.cpp) Engine/ECS/ECS.cpp $(TEST_COMMON)

build/tests/integration/tests.exe: $(TEST_COMMON) $(wildcard Tests/integration/*.cpp) $(ENGINE) $(GAME) $(HEADERS)
	@if not exist "build\tests\integration" mkdir "build\tests\integration"
	$(CXX) $(CXXFLAGS) -o $@ $(wildcard Tests/integration/*.cpp) $(ENGINE) $(GAME) $(TEST_COMMON) $(MEMORY_HOOKS) -L Vendor/SDL2/lib -lSDL2_test $(SDL_LIBS)

build/tests/e2e/tests.exe: $(TEST_COMMON) $(wildcard Tests/e2e/*.cpp) main.cpp $(ENGINE) $(HEADERS)
	@if not exist "build\tests\e2e" mkdir "build\tests\e2e"
	$(CXX) $(CXXFLAGS) -o $@ $(wildcard Tests/e2e/*.cpp) $(ENGINE) $(TEST_COMMON) -Wl,--wrap=SDL_CreateRenderer $(SDL_LIBS)

test:
	@for %%D in (unit integration e2e .) do @if exist "build\tests\%%D\out.json" del /Q "build\tests\%%D\out.json"
	@$(MAKE) -k test-unit test-integration test-e2e & build\tests\unit\tests.exe --summarize build/tests

test-unit: build/tests/unit/tests.exe
	cd build\tests\unit && tests.exe $(FILTER) --colour-mode none

test-integration: build/tests/integration/tests.exe
	@xcopy /E /I /H /Y "Game\Assets" "build\tests\integration\Assets" >nul
	@xcopy /E /I /H /Y "Game\Config" "build\tests\integration\Config" >nul
	cd build\tests\integration && tests.exe $(FILTER) --colour-mode none

test-e2e: build/tests/e2e/tests.exe
	@xcopy /E /I /H /Y "Game\Assets" "build\tests\e2e\Assets" >nul
	@xcopy /E /I /H /Y "Game\Config" "build\tests\e2e\Config" >nul
	cd build\tests\e2e && tests.exe $(FILTER) --colour-mode none

test-vector2: FILTER := [vector2]
test-velocity: FILTER := [velocity]
test-collider: FILTER := [collider]
test-memory: FILTER := [memory]
test-vector2 test-velocity: test-unit
test-collider test-memory: test-integration
test-frame-timing: test-e2e
