//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_static_body_component.h
 * .
 */

#pragma once

#include "mg/core/ecs/mg_base_component.h"
#include "mg/core/physics/mg_static_body_handle.h"

namespace Mg {

struct StaticBodyComponent : ecs::BaseComponent<StaticBodyComponent> {
    physics::StaticBodyHandle physics_body;
};


} // namespace Mg
