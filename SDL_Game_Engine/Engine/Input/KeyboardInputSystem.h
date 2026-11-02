#pragma once

namespace ecs { class Registry; }

void updateKeyboardInput(ecs::Registry& world, bool enabled = true);