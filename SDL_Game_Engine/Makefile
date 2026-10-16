.PHONY: build run bar test test-vector2 test-memory test-frame-timing

# Run the suite sequentially, including with make -j test.
.NOTPARALLEL: test


OUT_NAME := SDL_Game_Engine

LIB_DIR := Engine/Vendor/SDL2/lib
INCLUDE_DIR := Engine/Vendor/SDL2/include/SDL2
VENDOR_H_FILES := ./Engine/Vendor/
DLL_FILES := -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_Mixer -lSDL2_ttf


GENERAL_H_FILES := ./Engine/Headers/
SPEC_H_FILES := ./Engine/Headers/*/
ECS_H_FILES := ./Engine/ECS_Components/

GAME_H_FILES := ./Game/scenes/Headers/
GAME_SCRIPTS_FILES := ./Game/scripts/Headers/
ECS_SPEC_H_FILES := ./Engine/ECS_Components/Headers/

GENERAL_CPP_FILES := $(wildcard ./Engine/*.cpp)
SPEC_CPP_FILES := $(wildcard ./Engine/*/*.cpp)

GAME_CPP_FILES := $(wildcard ./Game/*/*.cpp)
MAIN_CPP_FILES := $(wildcard ./main.cpp)

build:
	echo "Building..."
	if not exist build mkdir build
	if not exist "build\Assets" mkdir "build\Assets"
	xcopy /E /I /H /Y "Game\assets\" "build\Assets\"
	if not exist "build\Config" mkdir "build\Config"
	if not exist "build\Config\settings.json" copy /Y "Game\Config\settings.json" "build\Config\settings.json"
	g++ -static -static-libstdc++ -I $(INCLUDE_DIR) -I $(VENDOR_H_FILES) -I $(GENERAL_H_FILES) -I $(GAME_H_FILES) -I $(SPEC_H_FILES) -I $(ECS_H_FILES) -I $(ECS_SPEC_H_FILES) -I $(GAME_SCRIPTS_FILES) -L $(LIB_DIR) -o ./build/$(OUT_NAME) $(GENERAL_CPP_FILES) $(SPEC_CPP_FILES) $(GAME_CPP_FILES) $(MAIN_CPP_FILES) $(DLL_FILES) \
	-lsetupapi -lversion -limm32 -lole32 -loleaut32 -lwinmm -lgdi32 -luser32 -lshell32 -ladvapi32 -lRpcrt4

run:
	@echo "Executing..."
	cd build && $(OUT_NAME).exe

bar:
	@$(MAKE) build
	@$(MAKE) run


TEST_CXXFLAGS := -std=c++17 -g -static -static-libstdc++
TEST_SDL_FLAGS := -I $(INCLUDE_DIR) -L $(LIB_DIR)
TEST_ENGINE_SOURCES := $(GENERAL_CPP_FILES) $(SPEC_CPP_FILES)
TEST_SDL_LIBS := -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf
TEST_WINDOWS_LIBS := -lsetupapi -lversion -limm32 -lole32 -loleaut32 -lwinmm -lgdi32 -luser32 -lshell32 -ladvapi32 -lRpcrt4

test: test-vector2 test-memory test-frame-timing

test-vector2:
	@echo "Testing Vector2..."
	@if not exist "build\tests\vector2" mkdir "build\tests\vector2"
	$(CXX) $(TEST_CXXFLAGS) -Wall -Wextra -Wpedantic -Werror -o build/tests/vector2/vector2_tests.exe Tests/Vector2.cpp
	@build\tests\vector2\vector2_tests.exe

test-memory:
	@echo "Testing memory and resource ownership..."
	@if not exist "build\tests\memory" mkdir "build\tests\memory"
	@xcopy /E /I /H /Y "Game\Assets" "build\tests\memory\Assets" >nul
	@xcopy /E /I /H /Y "Game\Config" "build\tests\memory\Config" >nul
	$(CXX) $(TEST_CXXFLAGS) $(TEST_SDL_FLAGS) -o build/tests/memory/memory_leaks.exe \
		Tests/MemoryLeaks.cpp $(TEST_ENGINE_SOURCES) $(GAME_CPP_FILES) \
		-Wl,--wrap=SDL_Init,--wrap=SDL_CreateWindow,--wrap=SDL_CreateRenderer \
		-Wl,--wrap=TTF_Init,--wrap=IMG_Init,--wrap=Mix_Init,--wrap=Mix_OpenAudio \
		-lmingw32 -lSDL2_test $(TEST_SDL_LIBS) $(TEST_WINDOWS_LIBS)
	@cd build\tests\memory && memory_leaks.exe

test-frame-timing:
	@echo "Testing frame timing..."
	@if not exist "build\tests\frame-timing" mkdir "build\tests\frame-timing"
	@xcopy /E /I /H /Y "Game\Assets" "build\tests\frame-timing\Assets" >nul
	@xcopy /E /I /H /Y "Game\Config" "build\tests\frame-timing\Config" >nul
	$(CXX) $(TEST_CXXFLAGS) $(TEST_SDL_FLAGS) -o build/tests/frame-timing/frame_timing.exe \
		Tests/FrameTiming.cpp $(TEST_ENGINE_SOURCES) -Wl,--wrap=SDL_CreateRenderer \
		-lmingw32 $(TEST_SDL_LIBS) $(TEST_WINDOWS_LIBS)
	@cd build\tests\frame-timing && frame_timing.exe
