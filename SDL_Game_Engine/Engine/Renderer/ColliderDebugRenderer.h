#pragma once

namespace ecs { class Registry; }
struct SDL_Renderer;
struct CameraComponent;

void renderColliderDebug(ecs::Registry& world, SDL_Renderer* renderer, bool devMode,
                         const CameraComponent* camera = nullptr);