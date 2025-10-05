//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_component_mask.h
 * ComponentMask is a bit mask representing the presence of a set of component types within an
 * Entity.
 */

#pragma once

#include "mg/core/ecs/mg_base_component.h"

namespace Mg::ecs {

/** ComponentMask is a bit mask representing the presence of a set of component types within an
 * Entity.
 */
using ComponentMask = uint64_t;

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

    if constexpr (is_instantiation_of_v<C, Not>) {
        return (ComponentMask{ 1u } << C::component_type::component_type_id()) | tail_mask;
    }
    else {
        return tail_mask;
    }
}


} // namespace Mg::ecs
