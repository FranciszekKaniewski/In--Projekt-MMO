#pragma once

namespace ecs { class Registry; }
struct SDL_Renderer;
struct CameraComponent;

void renderSprites(ecs::Registry& world, SDL_Renderer* renderer, const CameraComponent* camera = nullptr);
