#include "KeyboardInputSystem.h"

#include "../ECS/ECS.h"
#include "../ECS/Components/KeyboardControlComponent.h"
#include "../ECS/Components/MovementInputComponent.h"

#include <SDL.h>

void updateKeyboardInput(ecs::Registry& world) {
    const auto* keys = SDL_GetKeyboardState(nullptr);
    const math::Vector2 direction{
        static_cast<float>((keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) -
                           (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])),
        static_cast<float>((keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) -
                           (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]))};

    world.forEach<KeyboardControlComponent, MovementInputComponent>(
        [direction](ecs::Entity, const KeyboardControlComponent&, MovementInputComponent& input) {
            input.direction = direction;
        });
}
