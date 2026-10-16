#include "ECS.h"

#include <atomic>
#include <limits>

namespace ecs {
namespace {

std::uint64_t nextEntityID() {
    static std::atomic<std::uint64_t> next{1};
    auto id = next.load(std::memory_order_relaxed);
    do {
        if (id == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error("ECS entity identifiers exhausted.");
        }
    } while (!next.compare_exchange_weak(id, id + 1, std::memory_order_relaxed));
    return id;
}

} // namespace

Registry::~Registry() = default;

Entity Registry::createEntity() {
    const Entity entity(nextEntityID());
    entities.try_emplace(entity.value);
    ++aliveCount;
    return entity;
}

bool Registry::isAlive(Entity entity) const noexcept {
    const auto record = entities.find(entity.value);
    return record != entities.end() && record->second.alive;
}

bool Registry::destroyEntity(Entity entity) noexcept {
    const auto record = entities.find(entity.value);
    if (record == entities.end() || !record->second.alive) return false;

    record->second.alive = false;
    --aliveCount;
    if (iterationDepth == 0) entities.erase(record);
    return true;
}

void Registry::clear() noexcept {
    aliveCount = 0;
    if (iterationDepth == 0) {
        entities.clear();
    } else {
        for (auto& entry : entities) entry.second.alive = false;
    }
}

Registry::EntityRecord& Registry::requireEntity(Entity entity) {
    const auto record = entities.find(entity.value);
    if (record == entities.end() || !record->second.alive) {
        throw std::out_of_range("Entity does not belong to this registry or is no longer alive.");
    }
    return record->second;
}

Registry::IterationGuard::IterationGuard(Registry& registry) noexcept : registry(registry) {
    ++registry.iterationDepth;
}

Registry::IterationGuard::~IterationGuard() noexcept {
    if (--registry.iterationDepth == 0) registry.collectGarbage();
}

void Registry::collectGarbage() noexcept {
    for (auto record = entities.begin(); record != entities.end();) {
        if (!record->second.alive) {
            record = entities.erase(record);
            continue;
        }
        auto& components = record->second.components;
        for (auto component = components.begin(); component != components.end();) {
            if (!component->second->active) component = components.erase(component);
            else ++component;
        }
        ++record;
    }
}

} // namespace ecs
