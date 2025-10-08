//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_transform_component.h
 * .
 */

#pragma once

#include "mg/core/ecs/mg_base_component.h"

#include <glm/mat4x4.hpp>

namespace Mg {

struct TransformComponent : ecs::BaseComponent<TransformComponent> {
    glm::mat4 previous_transform = glm::mat4(1.0f);
    glm::mat4 transform = glm::mat4(1.0f);
};


} // namespace Mg
