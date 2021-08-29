//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_entity.h
 * Entity-Component-System implementation.
 * This ECS implementation was originally inspired by EntityX by Alec Thomas:
 * http://github.com/alecthomas/entityx
 *
 * However, the implementation has quite diverged from EntityX's approach since.
 */

#pragma once


#include "mg/utils/mg_gsl.h"

#include "mg/containers/mg_slot_map.h"
#include "mg/ecs/mg_component.h"

#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>

/** Entity-Component-System. */
namespace Mg::ecs {

/** In the Entity-Component-System pattern, an Entity is a handle to a set of associated components.
 */
class Entity {
public:
    Entity() = default;

private:
    friend class EntityCollection;

    Slot_map_handle& handle() noexcept { return m_handle; }
    Entity(Slot_map_handle handle) noexcept : m_handle{ handle } {}

    Slot_map_handle m_handle;
};

/** EntityCollection owns entities and their components. */
class EntityCollection {
public:
    template<typename... Cs> class iterator;
    template<typename... Cs> friend class iterator;
    template<typename... Cs> class UnpackingView;

    /** Construct a new EntityCollection.
     * @param entity_capacity Max simultaneous entities. The required memory for entities and
     * components is allocated immediately, so keep this value reasonably low.
     */
    explicit EntityCollection(uint32_t entity_capacity)
        : m_entity_data{ entity_capacity }, m_component_lists{ entity_capacity }
    {}

    /** Resets EntityCollection, destroying all entities and components. */
    void reset() noexcept;

    /** Creates a new entity with no components. */
    Entity create_entity();

    /** Delete entity and its components. */
    void delete_entity(Entity entity);

    /** Add component to entity, constructed with args. */
    template<typename C, typename... Ts> C& add_component(Entity entity, Ts&&... args);

    /** Remove component from entity. Requires that the component exists. */
    template<typename C> void remove_component(Entity entity);

    /** Get whether the entity has a component of a given type. */
    template<typename C> bool has_component(Entity entity) const
    {
        assert_is_component<C>();
        return has_component(entity, C::ComponentTypeId);
    }

    /** Get reference to component. Requires that the component exists. */
    template<typename C> C& get_component(Entity entity)
    {
        assert_is_component<C>();
        return get_component<C>(component_handle_ref(entity, C::ComponentTypeId));
    }

    /** Iterate over entities which have the requested set of components, e.g.:
     * @code
     * for (auto cs : entity_collection.get_with_components<Position, Velocity>() {
     *     auto entity   = std::get<Mg::ecs::Entity>(cs);
     *     auto position = std::get<Position*>(cs);
     *     auto velocity = std::get<Velocity*>(cs);
     *     ...
     * }
     * @endcode
     *
     * Or, with C++17 structured bindings:
     * @code
     * for (auto[entity, position, velocity]
     *     : entity_collection.get_with_components<Position, Velocity>()) { ... }
     * @endcode
     *
     * @tparam Cs List of required components
     * @return Iterable view over entities with the required components
     * whose iterator dereferences to a tuple (Entity, Cs*...)
     */
    template<typename... Cs> UnpackingView<Cs...> get_with_components() { return { *this }; }

    ComponentMask component_mask(Entity entity) const { return data(entity).mask; }

    /** Get the number of currently existing entities. */
    size_t num_entities() const noexcept { return m_entity_data.size(); }

private:
    // Array of handles to all components associated with an entity
    using ComponentList = std::array<Slot_map_handle, k_max_component_types>;

    // Array of owning pointers to component collections.
    using ComponentCollectionList =
        std::array<std::unique_ptr<IComponentCollection>, k_max_component_types>;

    // Meta-data associated with each entity
    struct EntityData {
        // Bitmask representing what components the entity holds. It is
        // technically redundant, but this compact representation allows very
        // fast iteration when searching for all entities with a certain set of
        // components.
        ComponentMask mask;

        // Handle to the ComponentList for this entity in m_component_lists
        Slot_map_handle component_list_handle;
    };

    const EntityData& data(Entity entity) const { return m_entity_data[entity.handle()]; }

    EntityData& data(Entity entity) { return m_entity_data[entity.handle()]; }

    template<typename C> ComponentCollection<C>& component_collection();

    bool has_component(Entity entity, size_t component_type_id) const
    {
        return component_mask(entity).test(component_type_id);
    }

    Slot_map_handle& component_handle_ref(Entity entity, size_t component_type_id);

    template<typename C> C& get_component(Slot_map_handle component_handle)
    {
        return component_collection<C>().get_component(component_handle);
    }

    ComponentMask& component_mask_ref(Entity entity) { return data(entity).mask; }

    Slot_map<EntityData> m_entity_data;

    // m_component_lists holds arrays of component handles, each array
    // corresponding to the components belonging to an entity.
    Slot_map<ComponentList> m_component_lists;

    // ComponentCollections owns the actual component data
    ComponentCollectionList m_component_collections;
};

//--------------------------------------------------------------------------------------------------

// Iterator over all entities with components Cs...
// See UnpackingView and EntityCollection::get_with_components
template<typename... Cs> class EntityCollection::iterator {
public:
    iterator(EntityCollection& collection, Slot_map<EntityData>::iterator it)
        : m_collection{ collection }, m_it{ it }
    {
        find_match();
    }

    // Increment until next matching entity is found (or end)
    iterator& operator++()
    {
        ++m_it;
        find_match();
        return *this;
    }

    // Dereference into a tuple of (Entity, Components*...)
    std::tuple<Entity, Cs*...> operator*();

    friend bool operator!=(iterator l, iterator r) { return l.m_it != r.m_it; }

private:
    bool match() { return (m_it->mask & m_mask) == m_mask; }

    void find_match()
    {
        while (m_it != m_collection.m_entity_data.end() && !match()) {
            ++m_it;
        }
    }

    EntityCollection& m_collection;
    Slot_map<EntityData>::iterator m_it;
    ComponentMask m_mask = create_mask(std::add_pointer_t<Cs>{ nullptr }...);
};

// Dereference iterator
template<typename... Cs> std::tuple<Entity, Cs*...> EntityCollection::iterator<Cs...>::operator*()
{
    Entity entity = m_collection.m_entity_data.make_handle(m_it);
    auto& components = m_collection.m_component_lists[m_it->component_list_handle];

    return std::tuple<Entity, Cs*...>{
        entity, &m_collection.get_component<Cs>(components[Cs::ComponentTypeId])...
    };
}

/** UnpackingView allows iteration over entities with certain components.
 * @see EntityCollection::get_with_components()
 */
template<typename... Cs> class EntityCollection::UnpackingView {
public:
    UnpackingView(EntityCollection& collection) : m_owner{ collection } {}

    iterator<Cs...> begin() { return { m_owner, m_owner.m_entity_data.begin() }; }
    iterator<Cs...> end() { return { m_owner, m_owner.m_entity_data.end() }; }

private:
    EntityCollection& m_owner;
};

//--------------------------------------------------------------------------------------------------
// EntityCollection member function template implementations
//--------------------------------------------------------------------------------------------------

// Add component to entity
template<typename C, typename... Ts> C& EntityCollection::add_component(Entity entity, Ts&&... args)
{
    assert_is_component<C>();

    static_assert(std::is_constructible<C, Ts...>::value,
                  "Component can not be constructed with the given arguments.");

    // Make sure component does not already exist
    MG_ASSERT(!has_component(entity, C::ComponentTypeId));

    auto& components = component_collection<C>();
    auto handle = components.emplace(std::forward<Ts>(args)...);

    component_handle_ref(entity, C::ComponentTypeId) = handle;
    component_mask_ref(entity).set(C::ComponentTypeId);

    return components.get_component(handle);
}

// Remove component from entity
template<typename C> void EntityCollection::remove_component(Entity entity)
{
    assert_is_component<C>();
    MG_ASSERT(has_component<C>(entity) && "Removing non-existent component.");

    auto& collection = component_collection<C>();
    auto& handle = component_handle_ref(entity, C::ComponentTypeId);
    collection.erase(handle);

    // Reset component handle (not strictly necessary but safer)
    handle = {};

    // Clear component flag.
    component_mask_ref(entity).reset(C::ComponentTypeId);
}

// Retrieve component collection for component type C
template<typename C> ComponentCollection<C>& EntityCollection::component_collection()
{
    assert_is_component<C>();

    auto& p_collection = m_component_collections[C::ComponentTypeId];

    // Lazy construction of ComponentCollections
    if (p_collection == nullptr) {
        p_collection = std::make_unique<ComponentCollection<C>>(m_entity_data.capacity());
    }

    // Cast to actual type and return
    auto& collection = *(p_collection.get());
    return static_cast<ComponentCollection<C>&>(collection);
}

} // namespace Mg::ecs
