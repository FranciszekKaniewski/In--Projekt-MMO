#pragma once

namespace ecs { class Registry; }

// Advances animations in seconds and synchronizes their sprite source rectangles.
void updateAnimations(ecs::Registry& world, double deltaTime);
