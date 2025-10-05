//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_collision_group.h
 * .
 */

#pragma once

#include "mg/utils/mg_macros.h"

#include <cstdint>

namespace Mg::physics {

/** Pre-defined set of collision filter groups. Note: these correspond to the Bullet library's
 * pre-defined filter groups. Using the same ones in this API reduces risk of collisions.
 */
enum CollisionGroup : uint32_t {
    None = 0u,
    Default = 1u,
    Static = 2u,
    Kinematic = 4u,
    Debris = 8u,
    Sensor = 16u,
    Character = 32u,
    All = ~0u
};
MG_DEFINE_BITMASK_OPERATORS(CollisionGroup);

} // namespace Mg::physics
