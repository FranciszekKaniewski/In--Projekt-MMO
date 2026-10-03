.PHONY: build run bar test


OUT_NAME := main

LIB_DIR := Engine/Vendor/SDL2/lib
INCLUDE_DIR := Engine/Vendor/SDL2/include/SDL2
VENDOR_H_FILES := ./Engine/Vendor/
DLL_FILES := -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_Mixer -lSDL2_ttf


GENERAL_H_FILES := ./Engine/Headers/
SPEC_H_FILES := ./Engine/Headers/*/
ECS_H_FILES := ./Engine/ECS_Components/
ECS_SPEC_H_FILES := ./Engine/ECS_Components/Headers/

GAME_H_FILES := ./Game/scenes/Headers/
GAME_SCRIPTS_FILES := ./Game/scripts/Headers/

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
	xcopy /E /I /H /Y "Game\Config\" "build\Config\"
	g++ -static -static-libstdc++ -I $(INCLUDE_DIR) -I $(VENDOR_H_FILES) -I $(GENERAL_H_FILES) -I $(GAME_H_FILES) -I $(SPEC_H_FILES) -I $(ECS_H_FILES) -I $(ECS_SPEC_H_FILES) -I $(GAME_SCRIPTS_FILES) -L $(LIB_DIR) -o ./build/$(OUT_NAME) $(GENERAL_CPP_FILES) $(SPEC_CPP_FILES) $(GAME_CPP_FILES) $(MAIN_CPP_FILES) $(DLL_FILES) \
	-lsetupapi -lversion -limm32 -lole32 -loleaut32 -lwinmm -lgdi32 -luser32 -lshell32 -ladvapi32 -lRpcrt4

run:
	@echo "Executing..."
	cd build && $(OUT_NAME).exe

bar:
	@$(MAKE) build
	@$(MAKE) run