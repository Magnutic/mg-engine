//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_light.h
 * Types representing light sources.
 */

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#pragma once

namespace Mg::gfx {

struct Light {
    glm::vec4 vector;
    glm::vec3 colour;
    float     range_sqr;
};

inline Light make_point_light(glm::vec3 position, glm::vec3 colour, float range)
{
    Light l;
    l.vector    = glm::vec4{ position, 1.0f };
    l.colour    = colour;
    l.range_sqr = range * range;
    return l;
}

} // namespace Mg::gfx
