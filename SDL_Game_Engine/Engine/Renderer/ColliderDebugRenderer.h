#pragma once

namespace ecs { class Registry; }
struct SDL_Renderer;

void renderColliderDebug(ecs::Registry& world, SDL_Renderer* renderer, bool devMode);