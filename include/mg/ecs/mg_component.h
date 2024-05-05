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

/** Maximum number of component types that may be declared.
 * Larger value means larger internal storage per entity, so keep reasonably
 * low.
 */
constexpr size_t k_max_component_types = 64;

namespace detail {

struct ComponentTag {};

struct NotTag {};

} // namespace detail

/** Component base class. */
template<size_t _component_type_id> struct BaseComponent : detail::ComponentTag {
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

template<typename T>
concept ComponentTypeDesignator = Component<T> || std::derived_from<T, detail::NotTag>;

/** ComponentMask is a bit mask representing the presence of a set of
 * component types within an Entity.
 */
using ComponentMask = std::bitset<k_max_component_types>;

/** Creates ComponentMask from a set of component types (given as pointers). */
constexpr ComponentMask create_mask()
{
    return 0;
}

constexpr ComponentMask create_not_mask()
{
    return 0;
}

template<Component C> constexpr ComponentMask create_mask(const C*)
{
    return size_t{ 1u } << C::component_type_id;
}

template<Component C, typename... Cs>
constexpr ComponentMask create_mask(const C* dummy, const Cs*... rest)
{
    return create_mask<C>(dummy) | create_mask(rest...);
}

template<std::derived_from<detail::NotTag> C, typename... Cs>
constexpr ComponentMask create_mask(const C*, const Cs*... rest)
{
    return create_mask(rest...);
}

template<Component C, typename... Cs>
constexpr ComponentMask create_not_mask(const C*, const Cs*... rest)
{
    return create_not_mask(rest...);
}

template<std::derived_from<detail::NotTag> C, typename... Cs>
constexpr ComponentMask create_not_mask(const C*, const Cs*... rest)
{
    return create_mask(static_cast<typename C::component_type*>(nullptr)) |
           create_not_mask(rest...);
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
