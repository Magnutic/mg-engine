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

#include "mg/containers/mg_slot_map.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"

/** Used to define new component types, e.g.
 *    MG_DEFINE_COMPONENT(Position) { float x, y; };
 */
#define MG_DEFINE_COMPONENT(COMPONENT_TYPE_NAME) \
    struct COMPONENT_TYPE_NAME : ::Mg::ecs::BaseComponent<COMPONENT_TYPE_NAME>

namespace Mg {
namespace ecs {

/** Maximum number of component types that may be declared.
 * Larger value means larger internal storage per entity, so keep reasonably
 * low.
 */
constexpr size_t k_max_component_types = 64;

struct ComponentTag {
protected:
    static size_t new_family_id() noexcept
    {
        MG_ASSERT(n_component_types < k_max_component_types - 1);
        return n_component_types++;
    }

private:
    static size_t n_component_types; // Defined in mg_entity.cpp
};

/** Component base class. Using the Curiously Recurring Template Pattern (CRTP)
 * in order to create unique ComponentTypeId for each derived type.
 */
template<typename Derived> struct BaseComponent : ComponentTag {
    static const size_t ComponentTypeId;
};

template<typename Derived> const size_t BaseComponent<Derived>::ComponentTypeId = new_family_id();

/** Mg Engine component type trait. */
template<typename T> using IsComponent = std::is_base_of<ComponentTag, T>;

/** Quick way to assert that a type T is an Mg Engine component type. */
template<typename T> constexpr void assert_is_component()
{
    static_assert(IsComponent<T>::value, "Supplied type must be a Mg Engine ECS component type.");
}

/** ComponentMask is a bit mask representing the presence of a set of
 * component types within an Entity.
 */
using ComponentMask = std::bitset<k_max_component_types>;

/** Creates ComponentMask from a set of component types (given as pointers). */
constexpr ComponentMask create_mask()
{
    return 0;
}

template<typename C> constexpr ComponentMask create_mask(const C*)
{
    return size_t{ 1u } << C::ComponentTypeId;
}

template<typename C, typename... Cs>
constexpr ComponentMask create_mask(const C* dummy, const Cs*... rest)
{
    return create_mask<C>(dummy) | create_mask(rest...);
}

/** Interface for ComponentCollection for any Component type. */
class IComponentCollection {
public:
    MG_INTERFACE_BOILERPLATE(IComponentCollection);
    virtual void erase(Slot_map_handle handle) = 0;
};

/** ComponentCollection creates, stores, and destroys Components. */
template<typename C> class ComponentCollection : public IComponentCollection {
public:
    explicit ComponentCollection(uint32_t num_elems) : m_data{ num_elems } {}

    template<typename... Ts> Slot_map_handle emplace(Ts&&... args)
    {
        return { m_data.insert(C{ std::forward<Ts>(args)... }) };
    }

    void erase(Slot_map_handle handle) override { m_data.erase(handle); }

    C& get_component(Slot_map_handle handle) { return m_data[handle]; }

private:
    Slot_map<C> m_data;
};
} // namespace ecs
} // namespace Mg
