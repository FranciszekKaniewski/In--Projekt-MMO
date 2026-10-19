#include "CameraSystem.h"
#include "ECS.h"
#include "Components/CameraComponent.h"
#include "Components/TransformComponent.h"

void updateCameras(ecs::Registry& world) {
    world.forEach<CameraComponent>([&world](ecs::Entity, CameraComponent& camera) {
        if (!camera.isValid()) return;
        if (camera.following) {
            const auto* transform = world.tryGetComponent<TransformComponent>(camera.target);
            if (!transform) camera.following = false;
            else if (std::isfinite(transform->position.x) && std::isfinite(transform->position.y)) {
                camera.position = transform->position - camera.size * 0.5f;
            }
        }
        camera.clampToBounds();
    });
}
