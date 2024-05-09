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

// TODO looking back at this again a few years later, it strikes me how much pointer-chasing there
// still is. Not sure it will bring the cache-friendliness promised of ECS. Notably iterating over
// entities with different sets of components will cause random access in each component list for
// each iteration.
//
// A simple case to fix is iterating with only one component type; in that case, we can iterate
// directly over the component list.
//
// Otherwise solutions get more complex. One could define each component as being part of a group,
// and allocate each group's components in an AoS. So an "SoAoS". That only helps when iterating
// over (subsets) of that particular group, though.
//
// Also, I should probably use plf::colony instead of Mg::Slot_map for this. Might even remove the
// latter as I cannot think of any situation where it is better than colony.

#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_gsl.h"

#include "mg/containers/mg_slot_map.h"
#include "mg/ecs/mg_component.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
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

template<Component... Cs> static consteval bool has_duplicate_component_ids()
{
    std::vector<size_t> component_ids;
    component_ids.reserve(sizeof...(Cs));
    (component_ids.push_back(Cs::component_type_id), ...);
    std::ranges::sort(component_ids);
    return std::ranges::adjacent_find(component_ids) != component_ids.end();
}

/** EntityCollection owns entities and their components. */
class EntityCollection {
public:
    template<ComponentTypeDesignator... Cs> class iterator;
    template<ComponentTypeDesignator... Cs> friend class iterator;
    template<ComponentTypeDesignator... Cs> class UnpackingView;

    /** Construct a new EntityCollection.
     * @param entity_capacity Max simultaneous entities. The required memory for entities and
     * components is allocated immediately, so keep this value reasonably low.
     *
     * Must call init() with the set of component types you want to use before using the
     * EntityCollection.
     */
    explicit EntityCollection(uint32_t entity_capacity)
        : m_entity_data{ entity_capacity }, m_component_lists{ entity_capacity }
    {}

    template<Component... Cs>
    void init()
        requires(!has_duplicate_component_ids<Cs...>())
    {
        (add_component_collection<Cs>(), ...);
    }

    /** Resets EntityCollection, destroying all entities and components. */
    void reset() noexcept;

    /** Creates a new entity with no components. */
    [[nodiscard]] Entity create_entity();

    /** Delete entity and its components. */
    void delete_entity(Entity entity);

    /** Add component to entity, constructed with args. */
    template<Component C, typename... Ts> C& add_component(Entity entity, Ts&&... args);

    /** Remove component from entity. Requires that the component exists. */
    template<Component C> void remove_component(Entity entity);

    /** Get whether the entity has a component of a given type. */
    template<Component C> bool has_component(Entity entity) const
    {
        return has_component(entity, C::component_type_id);
    }

    /** Get reference to component. Requires that the component exists. */
    template<Component C> C& get_component(Entity entity)
    {
        MG_ASSERT(has_component<C>(entity));
        return get_component<C>(component_handle_ref(entity, C::component_type_id));
    }

    /** Iterate over entities which have the requested set of components, e.g.:
     * @code
     * for (auto[entity, position, velocity]
     *     : entity_collection.get_with<Position, Velocity>()) { ... }
     * @endcode
     *
     * One can also use `Mg::ecs::Not` to specify components that the entities shall not have:
     *
     * @code
     * for (auto[entity, position]
     *     : entity_collection.get_with<Position, Mg::ecs::Not<Velocity>>()) {
     *         // Will ignore all entities that have a Velocity component.
     *     }
     * @endcode
     *
     * And `Mg::ecs::Maybe` to specify components that shall be included if the entity has them, but
     * if the entity does not have them, the entity will not be skipped, but the corresponding value
     * will be nullptr.
     *
     * @code
     * for (auto[entity, position, velocity]
     *     : entity_collection.get_with<Position, Mg::ecs::Maybe<Velocity>>()) {
     *         // Use entity, position, and velocity -- but check whether velocity is nullptr first.
     *     }
     * @endcode
     *
     * @tparam Cs List of required components
     * @return Iterable view over entities with the required components
     * whose iterator dereferences to a tuple (Entity, Cs*...)
     */
    template<ComponentTypeDesignator... Cs> [[nodiscard]] UnpackingView<Cs...> get_with()
    {
        return { *this };
    }

    ComponentMask component_mask(Entity entity) const { return data(entity).mask; }

    /** Get the number of currently existing entities. */
    size_t num_entities() const noexcept { return m_entity_data.size(); }

    // Array of handles to all components associated with an entity
    using ComponentList = std::array<Slot_map_handle, k_max_component_types>;

    // Array of owning pointers to component collections.
    using ComponentCollectionList =
        std::array<std::unique_ptr<IComponentCollection>, k_max_component_types>;

private:
    // Meta-data associated with each entity
    struct EntityData {
        // Bitmask representing what components the entity holds. It is
        // technically redundant, but this compact representation allows very
        // fast iteration when searching for all entities with a certain set of
        // components.
        ComponentMask mask{};

        // Handle to the ComponentList for this entity in m_component_lists
        Slot_map_handle component_list_handle;
    };

    const EntityData& data(Entity entity) const { return m_entity_data[entity.handle()]; }

    EntityData& data(Entity entity) { return m_entity_data[entity.handle()]; }

    template<Component C> void add_component_collection();

    template<Component C> ComponentCollection<C>& component_collection();

    bool has_component(Entity entity, size_t component_type_id) const
    {
        return (component_mask(entity) & (ComponentMask{ 1u } << component_type_id)) != 0u;
    }

    Slot_map_handle& component_handle_ref(Entity entity, size_t component_type_id);

    template<Component C> C& get_component(Slot_map_handle component_handle)
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
// See UnpackingView and EntityCollection::get_with
template<ComponentTypeDesignator... Cs> class EntityCollection::iterator {
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

    // Dereference into a tuple of (Entity, Components&...)
    auto operator*()
    {
        Entity entity = m_collection.m_entity_data.make_handle(m_it);
        ComponentList& component_list = m_collection.m_component_lists[m_it->component_list_handle];

        return std::tuple_cat(std::tuple{ entity },
                              get_tuple_with_reference_to_component<Cs>(component_list)...);
    }

    friend bool operator!=(iterator l, iterator r) { return l.m_it != r.m_it; }

private:
    template<std::derived_from<detail::NotTag> C>
    std::tuple<> get_tuple_with_reference_to_component(ComponentList&)
    {
        return {};
    }

    template<std::derived_from<detail::MaybeTag> C>
    std::tuple<typename C::component_type*>
    get_tuple_with_reference_to_component(ComponentList& component_list)
    {
        constexpr auto id = C::component_type::component_type_id;
        return component_list[id]
                   ? &m_collection.get_component<typename C::component_type>(component_list[id])
                   : nullptr;
    }

    template<Component C>
    std::tuple<C&> get_tuple_with_reference_to_component(ComponentList& component_list)
    {
        return m_collection.get_component<C>(component_list[C::component_type_id]);
    }

    // The byte mask indicating which components must be included.
    static consteval ComponentMask mask()
    {
        if constexpr (sizeof...(Cs) > 0) {
            return create_mask<Cs...>();
        }
        return 0;
    }

    // The byte mask indicating which components must be not included.
    static consteval ComponentMask not_mask()
    {
        if constexpr (sizeof...(Cs) > 0) {
            return create_not_mask<Cs...>();
        }
        return 0;
    }

    // Check if the iterator points to an entity which has the sought set of components.
    bool match() { return (m_it->mask & mask()) == mask() && (m_it->mask & not_mask()) == 0u; }

    void find_match()
    {
        while (m_it != m_collection.m_entity_data.end() && !match()) {
            ++m_it;
        }
    }

    EntityCollection& m_collection;
    Slot_map<EntityData>::iterator m_it;
};


/** UnpackingView allows iteration over entities with certain components.
 * @see EntityCollection::get_with()
 */
template<ComponentTypeDesignator... Cs> class EntityCollection::UnpackingView {
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
template<Component C, typename... Ts>
C& EntityCollection::add_component(Entity entity, Ts&&... args)
{
    // Make sure component does not already exist
    MG_ASSERT(!has_component(entity, C::component_type_id));

    auto& components = component_collection<C>();
    auto handle = components.emplace(std::forward<Ts>(args)...);

    component_handle_ref(entity, C::component_type_id) = handle;
    component_mask_ref(entity) |= (ComponentMask{ 1 } << C::component_type_id);

    return components.get_component(handle);
}

// Remove component from entity
template<Component C> void EntityCollection::remove_component(Entity entity)
{
    MG_ASSERT(has_component<C>(entity) && "Removing non-existent component.");

    auto& collection = component_collection<C>();
    auto& handle = component_handle_ref(entity, C::component_type_id);
    collection.erase(handle);

    // Reset component handle (not strictly necessary but safer)
    handle = {};

    // Clear component flag.
    component_mask_ref(entity) &= ~(ComponentMask{ 1 } << C::component_type_id);
}

template<Component C> void EntityCollection::add_component_collection()
{
    auto& p_collection = m_component_collections[C::component_type_id];
    MG_ASSERT(p_collection == nullptr);
    p_collection = std::make_unique<ComponentCollection<C>>(m_entity_data.capacity());
}

// Retrieve component collection for component type C
template<Component C> ComponentCollection<C>& EntityCollection::component_collection()
{
    auto& p_collection = m_component_collections[C::component_type_id];
    MG_ASSERT(p_collection != nullptr &&
              "EntityCollection does not contain a ComponentCollection for this component type.");

    // Cast to actual type and return
    auto& collection = *(p_collection.get());
    return static_cast<ComponentCollection<C>&>(collection);
}

} // namespace Mg::ecs
