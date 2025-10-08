//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_animation_component.h
 * .
 */

#pragma once

#include "mg/core/ecs/mg_base_component.h"
#include "mg/core/gfx/mg_skeleton.h"
#include "mg/utils/mg_optional.h"

namespace Mg {

struct AnimationComponent : ecs::BaseComponent<AnimationComponent> {
    Opt<uint32_t> current_clip;
    float time_in_clip = 0.0f;
    float animation_speed = 1.0f;
    gfx::SkeletonPose pose;
};


} // namespace Mg
