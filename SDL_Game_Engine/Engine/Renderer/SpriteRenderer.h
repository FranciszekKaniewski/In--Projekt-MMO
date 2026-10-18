#pragma once

namespace ecs { class Registry; }
struct SDL_Renderer;

void renderSprites(ecs::Registry& world, SDL_Renderer* renderer);
