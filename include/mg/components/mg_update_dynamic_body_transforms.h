//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_update_dynamic_body_transforms.h
 * .
 */

#pragma once

#include "mg/components/mg_dynamic_body_component.h"
#include "mg/components/mg_transform_component.h"
#include "mg/core/ecs/mg_entity.h"

namespace Mg {

inline void update_dynamic_body_transforms(ecs::EntityCollection& collection)
{
    for (auto [entity, dynamic_body, transform] :
         collection.get_with<DynamicBodyComponent, TransformComponent>()) {
        transform.previous_transform = transform.transform;
        transform.transform = dynamic_body.physics_body.get_transform();
    }
}


} // namespace Mg
