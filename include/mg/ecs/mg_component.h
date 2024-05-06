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

#include "mg/containers/mg_slot_map.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"

namespace Mg::ecs {

/** Maximum number of component types that may be used in one EntityCollection. */
constexpr size_t k_max_component_types = 64;

/** ComponentMask is a bit mask representing the presence of a set of component types within an
 * Entity.
 */
using ComponentMask = uint64_t;

namespace detail {

// Identifies component types.
struct ComponentTag {};

// Identifies types that are instantiations of Mg::ecs::Not<>.
struct NotTag {};

} // namespace detail

/** Component base class.
 * All component types must publicly derive from this class, and set the _component_type_id to
 * a value unique for this component type (among all the component types that will be used with the
 * same EntityCollection).
 */
template<size_t _component_type_id> struct BaseComponent : detail::ComponentTag {
    static_assert(_component_type_id < k_max_component_types);
    static constexpr size_t component_type_id = _component_type_id;
};

/** Mg Engine component concept. */
template<typename T>
concept Component = std::derived_from<T, detail::ComponentTag>;

/** Tag type used to indicate when we want entities containing a particular component to _not_ be
 * included.
 */
template<Component C> struct Not : detail::NotTag {
    using component_type = C;
};

/** Tag-type used to designate which component types we want to include when iterating over
 * entities. Those which we want to include are designated by the component type itself, and those
 * we want to exclude are designated by wrapping the component type in Mg::ecs::Not<>.
 */
template<typename T>
concept ComponentTypeDesignator = Component<T> || std::derived_from<T, detail::NotTag>;

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
        return (ComponentMask{ 1u } << C::component_type_id) | tail_mask;
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
        return (ComponentMask{ 1u } << C::component_type::component_type_id) | tail_mask;
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
