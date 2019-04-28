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

/** @file mg_render_command_data.h
 * Data associated with mesh-render command list.
 */

#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "mg/gfx/mg_render_command_data_handle.h"

namespace Mg::gfx {
class Material;
}

namespace Mg::gfx::internal {

struct RenderCommandData {
    glm::mat4 M;
    glm::vec3 centre;

    uint32_t mesh_vao_id;

    uint32_t begin;
    uint32_t amount;

    const Material* material;

    float radius;
};

inline const RenderCommandData& get_command_data(RenderCommandDataHandle handle)
{
    return *reinterpret_cast<const RenderCommandData*>(handle); // NOLINT
}

inline RenderCommandDataHandle cast_to_render_command_data_handle(void* ptr)
{
    return RenderCommandDataHandle{ reinterpret_cast<uintptr_t>(ptr) }; // NOLINT
}

} // namespace Mg::gfx::internal
