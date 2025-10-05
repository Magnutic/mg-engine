//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_light.h
 * Types representing light sources.
 */

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#pragma once

namespace Mg::gfx {

struct Light {
    glm::vec4 vector = glm::vec4(0.0f);
    glm::vec3 colour = glm::vec3(0.0f);
    float range_sqr = 0.0f;
};

inline Light make_point_light(glm::vec3 position, glm::vec3 colour, float range)
{
    Light l;
    l.vector = glm::vec4{ position, 1.0f };
    l.colour = colour;
    l.range_sqr = range * range;
    return l;
}

} // namespace Mg::gfx
