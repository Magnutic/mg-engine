//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_base_component.h
 * Base class for components to be used in an EntityCollection.
 */

#pragma once

#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_is_instantiation_of.h"

#include <concepts>
#include <cstdlib>

namespace Mg::ecs {

/** Maximum number of component types that may be used in one EntityCollection. */
inline constexpr size_t k_max_component_types = 64;
inline constexpr size_t k_unset_component_tag = size_t(-1);

namespace detail {
void set_component_id(size_t& component_type_id);
} // namespace detail

/** Component base class.
 * All component types must publicly derive from this class.
 */
template<typename Derived> class BaseComponent {
public:
    static size_t component_type_id()
    {
        MG_ASSERT(s_component_type_id != k_unset_component_tag &&
                  "Component type id has not been initialized. "
                  "Before use in an EntityCollection, you must call EntityCollection::init() "
                  "with all the component types you intend to use with the collection.");
        return s_component_type_id;
    }

private:
    friend class EntityCollection;

    static void init_component_type_id() { detail::set_component_id(s_component_type_id); }

    static size_t s_component_type_id;
};

template<typename Derived>
inline size_t BaseComponent<Derived>::s_component_type_id = k_unset_component_tag;

/** Mg Engine component concept. */
template<typename T>
concept Component = std::derived_from<T, BaseComponent<T>>;

/** Tag type used to indicate when we want entities containing a particular component to _not_ be
 * included.
 */
template<Component C> struct Not {
    using component_type = C;
};

/** Tag type used to indicate when we want particular component to be included when iterating over
 * an entity if it is present, without disqualifying the entity if the component is not included.
 */
template<Component C> struct Maybe {
    using component_type = C;
};

/** Tag-type used to designate which component types we want to include when iterating over
 * entities. Those which we want to include are designated by the component type itself, and those
 * we want to exclude are designated by wrapping the component type in Mg::ecs::Not<> or
 * Mg::ecs::Maybe<>.
 */
template<typename T>
concept ComponentTypeDesignator = Component<T> || InstantiationOf<T, Not> ||
                                  InstantiationOf<T, Maybe>;


} // namespace Mg::ecs
