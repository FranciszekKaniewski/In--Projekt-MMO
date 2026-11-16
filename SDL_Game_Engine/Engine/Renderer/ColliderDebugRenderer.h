#pragma once

namespace ecs { class Registry; }
class Map;
struct SDL_Renderer;
struct CameraComponent;

void renderColliderDebug(ecs::Registry& world, SDL_Renderer* renderer, bool devMode,
                         const CameraComponent* camera = nullptr);
void renderMapColliderDebug(const Map& map, SDL_Renderer* renderer, bool devMode,
                            const CameraComponent* camera = nullptr);
