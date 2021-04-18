//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_animatinos.h
 * Structures defining animations for animated meshes.
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/core/mg_identifier.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace Mg::gfx::Mesh {

// TODO store animation length, tick rate (or normalise timings on conversion)

struct PositionKey {
    double time;
    glm::vec3 value;
};

struct RotationKey {
    double time;
    glm::quat value;
};

struct ScaleKey {
    double time;
    float value;
};

// TODO there are a lot of arrays here. It would be better to allocate them all at once with the old
// 'joint allocation' code I wrote a long time ago, if I can find that.

struct AnimationChannel {
    Array<PositionKey> position_keys;
    Array<RotationKey> rotation_keys;
    Array<ScaleKey> scale_keys;
};

struct AnimationClip {
    Identifier name;
    Array<AnimationChannel> channels;
    float duration_seconds = 0.0;
};

} // namespace Mg::gfx::Mesh
