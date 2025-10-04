//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file ecs/mg_component.h
 * Component type and utilities for the ECS implementation.
 * @see mg_entity.h
 */

#pragma once

#include <bitset>
#include <concepts>
#include <mutex>

#include "mg/containers/mg_slot_map.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"

namespace Mg::ecs {

/** Maximum number of component types that may be used in one EntityCollection. */
constexpr size_t k_max_component_types = 64;
constexpr size_t k_unset_component_tag = size_t(-1);

/** ComponentMask is a bit mask representing the presence of a set of component types within an
 * Entity.
 */
using ComponentMask = uint64_t;

namespace detail {

// Identifies component types.
struct ComponentTag {};

// Identifies types that are instantiations of Mg::ecs::Not<>.
struct NotTag {};

// Identifies types that are instantiations of Mg::ecs::Maybe<>.
struct MaybeTag {};

inline size_t g_next_component_type_id{ 0 };

inline std::mutex g_component_type_id_init_mutex;

} // namespace detail

/** Component base class.
 * All component types must publicly derive from this class.
 */
template<typename Derived> class BaseComponent : public detail::ComponentTag {
public:
    static void init_component_type_id()
    {
        std::lock_guard g{ detail::g_component_type_id_init_mutex };
        if (s_component_type_id == k_unset_component_tag) {
            s_component_type_id = detail::g_next_component_type_id++;
        }
        MG_ASSERT(s_component_type_id < k_max_component_types);
    }

    static size_t component_type_id()
    {
        MG_ASSERT_DEBUG(s_component_type_id != k_unset_component_tag &&
                        "Component type id has not been initialized.");
        return s_component_type_id;
    }

private:
    static size_t s_component_type_id;
};

template<typename Derived>
inline size_t BaseComponent<Derived>::s_component_type_id = k_unset_component_tag;

/** Mg Engine component concept. */
template<typename T>
concept Component = std::derived_from<T, detail::ComponentTag>;

/** Tag type used to indicate when we want entities containing a particular component to _not_ be
 * included.
 */
template<Component C> struct Not : detail::NotTag {
    using component_type = C;
};

/** Tag type used to indicate when we want particular component to be included when iterating over
 * an entity if it is present, without disqualifying the entity if the component is not included.
 */
template<Component C> struct Maybe : detail::MaybeTag {
    using component_type = C;
};

/** Tag-type used to designate which component types we want to include when iterating over
 * entities. Those which we want to include are designated by the component type itself, and those
 * we want to exclude are designated by wrapping the component type in Mg::ecs::Not<> or
 * Mg::ecs::Maybe<>.
 */
template<typename T>
concept ComponentTypeDesignator = Component<T> || std::derived_from<T, detail::NotTag> ||
                                  std::derived_from<T, detail::MaybeTag>;

/** Creates ComponentMask from a set of component type designators, including the designators that
 * are Component types while ignoring those that are wrapped in Mg::ecs::Not<Component>.
 */
template<ComponentTypeDesignator C, ComponentTypeDesignator... Cs>
constexpr ComponentMask create_mask()
{
    ComponentMask tail_mask{};
    if constexpr (sizeof...(Cs) > 0) {
        tail_mask = create_mask<Cs...>();
    }

    if constexpr (Component<C>) {
        return (ComponentMask{ 1u } << C::component_type_id()) | tail_mask;
    }
    else {
        return tail_mask;
    }
}

/** Creates ComponentMask from a set of component type designators, ignoring the designators that
 * are Component types while including only those that are wrapped in Mg::ecs::Not<Component>.
 */
template<ComponentTypeDesignator C, ComponentTypeDesignator... Cs>
constexpr ComponentMask create_not_mask()
{
    ComponentMask tail_mask{};
    if constexpr (sizeof...(Cs) > 0) {
        tail_mask = create_not_mask<Cs...>();
    }

    if constexpr (std::derived_from<C, detail::NotTag>) {
        return (ComponentMask{ 1u } << C::component_type::component_type_id()) | tail_mask;
    }
    else {
        return tail_mask;
    }
}

/** Interface for ComponentCollection for any Component type. */
class IComponentCollection {
public:
    MG_INTERFACE_BOILERPLATE(IComponentCollection);
    virtual void erase(Slot_map_handle handle) noexcept = 0;
    virtual void clear() noexcept = 0;
};

/** ComponentCollection creates, stores, and destroys Components. */
template<typename C> class ComponentCollection : public IComponentCollection {
public:
    explicit ComponentCollection(uint32_t num_elems) : m_data{ num_elems } {}

    template<typename... Ts> Slot_map_handle emplace(Ts&&... args)
    {
        return { m_data.insert(C{ /*BaseComponent*/ {}, std::forward<Ts>(args)... }) };
    }

    void erase(Slot_map_handle handle) noexcept override { m_data.erase(handle); }

    void clear() noexcept override { m_data.clear(); }

    C& get_component(Slot_map_handle handle) { return m_data[handle]; }

private:
    Slot_map<C> m_data;
};

} // namespace Mg::ecs
