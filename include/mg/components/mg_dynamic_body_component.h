//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_dynamic_body_component.h
 * .
 */

#pragma once

#include "mg/core/ecs/mg_base_component.h"
#include "mg/core/physics/mg_dynamic_body_handle.h"

namespace Mg {

struct DynamicBodyComponent : ecs::BaseComponent<DynamicBodyComponent> {
    physics::DynamicBodyHandle physics_body;
};


} // namespace Mg
