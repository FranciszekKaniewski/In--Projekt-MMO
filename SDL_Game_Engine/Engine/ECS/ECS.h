#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ecs {

class Entity {
public:
    constexpr Entity() noexcept = default;
    constexpr std::uint64_t id() const noexcept { return value; }
    constexpr explicit operator bool() const noexcept { return value != 0; }
    constexpr bool operator==(Entity other) const noexcept { return value == other.value; }
    constexpr bool operator!=(Entity other) const noexcept { return !(*this == other); }

private:
    friend class Registry;
    explicit constexpr Entity(std::uint64_t id) noexcept : value(id) {}
    std::uint64_t value = 0;
};

class Registry {
public:
    Registry() = default;
    ~Registry();
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) = delete;
    Registry& operator=(Registry&&) = delete;

    Entity createEntity();
    bool isAlive(Entity entity) const noexcept;
    bool destroyEntity(Entity entity) noexcept;
    void clear() noexcept;
    std::size_t size() const noexcept { return aliveCount; }


    template<typename T, typename... Args>
    T& addComponent(Entity entity, Args&&... args) {
        const auto key = componentKey<T>();
        auto& record = requireEntity(entity);
        if (record.components.find(key) != record.components.end()) {
            throw std::logic_error("Component already exists or its removal is pending.");
        }

        auto component = std::make_unique<StoredComponent<T>>(std::forward<Args>(args)...);
        // A user-defined constructor may have changed the registry. Revalidate
        // the entity and insertion instead of returning an unowned reference.
        auto& destination = requireEntity(entity).components;
        const auto inserted = destination.emplace(key, std::move(component));
        if (!inserted.second) {
            throw std::logic_error("Component was added during its construction.");
        }
        return static_cast<StoredComponent<T>&>(*inserted.first->second).value;
    }

    template<typename T>
    bool hasComponent(Entity entity) const {
        return tryGetComponent<T>(entity) != nullptr;
    }

    template<typename T>
    const T* tryGetComponent(Entity entity) const {
        const auto key = componentKey<T>();
        const auto record = entities.find(entity.value);
        if (record == entities.end() || !record->second.alive) return nullptr;

        const auto component = record->second.components.find(key);
        if (component == record->second.components.end() || !component->second->active) {
            return nullptr;
        }
        return &static_cast<const StoredComponent<T>&>(*component->second).value;
    }

    template<typename T>
    T* tryGetComponent(Entity entity) {
        return const_cast<T*>(std::as_const(*this).tryGetComponent<T>(entity));
    }

    template<typename T>
    const T& getComponent(Entity entity) const {
        const T* component = tryGetComponent<T>(entity);
        if (!component) {
            throw std::out_of_range("Entity is not alive or does not have this component.");
        }
        return *component;
    }

    template<typename T>
    T& getComponent(Entity entity) {
        return const_cast<T&>(std::as_const(*this).getComponent<T>(entity));
    }


    template<typename T>
    bool removeComponent(Entity entity) {
        const auto key = componentKey<T>();
        const auto record = entities.find(entity.value);
        if (record == entities.end() || !record->second.alive) return false;

        auto& components = record->second.components;
        const auto component = components.find(key);
        if (component == components.end() || !component->second->active) return false;

        if (iterationDepth == 0) components.erase(component);
        else component->second->active = false;
        return true;
    }


    template<typename... Components, typename Function>
    void forEach(Function&& function) {
        const IterationGuard guard(*this);
        std::vector<Entity> snapshot;
        snapshot.reserve(aliveCount);
        for (const auto& entry : entities) {
            const Entity entity(entry.first);
            if (entry.second.alive && (hasComponent<Components>(entity) && ...)) {
                snapshot.push_back(entity);
            }
        }

        for (const Entity entity : snapshot) {
            if (isAlive(entity) && (hasComponent<Components>(entity) && ...)) {
                function(entity, getComponent<Components>(entity)...);
            }
        }
    }

private:
    struct ComponentStorage {
        bool active = true;
        virtual ~ComponentStorage() = default;
    };

    template<typename T>
    struct StoredComponent final : ComponentStorage {
        template<typename... Args>
        explicit StoredComponent(Args&&... args) : value{std::forward<Args>(args)...} {}
        T value;
    };

    struct EntityRecord {
        bool alive = true;
        std::unordered_map<std::type_index, std::unique_ptr<ComponentStorage>> components;
    };

    struct IterationGuard {
        explicit IterationGuard(Registry& registry) noexcept;
        ~IterationGuard() noexcept;
        IterationGuard(const IterationGuard&) = delete;
        IterationGuard& operator=(const IterationGuard&) = delete;
        Registry& registry;
    };

    template<typename T>
    static std::type_index componentKey() {
        static_assert(std::is_object_v<T> && std::is_same_v<T, std::decay_t<T>>,
                      "Use an unqualified component type, for example Transform.");
        static_assert(std::is_nothrow_destructible_v<T>,
                      "Component destructors must not throw.");
        return std::type_index(typeid(T));
    }

    EntityRecord& requireEntity(Entity entity);
    void collectGarbage() noexcept;

    std::unordered_map<std::uint64_t, EntityRecord> entities;
    std::size_t aliveCount = 0;
    std::size_t iterationDepth = 0;
};
}
