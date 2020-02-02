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

/** @file mg_mesh_info.h
 * Internal mesh structure. @see MeshRepository
 */

#pragma once

#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_mesh_handle.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_opaque_handle.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <cstring>

namespace Mg::gfx::internal {

/** Range of indices belonging to a submesh. */
struct SubMeshRange {
    uint32_t begin;
    uint32_t amount;
};

using SubMeshInfo = small_vector<SubMeshRange, 4>;

/** Internal mesh structure. @see MeshRepository */
struct MeshInfo {
    SubMeshInfo submeshes;
    glm::vec3   centre{};
    float       radius{};
    Identifier  mesh_id{ "" };

    // Identifier for the mesh object (vertex array object) in the graphics API.
    OpaqueHandle::Value gfx_api_mesh_object_id{};

    // Index of this object in data structure -- used for deletion.
    uint32_t self_index{};
};

inline MeshHandle make_mesh_handle(const MeshInfo* mesh_info) noexcept
{
    MeshHandle handle{};
    static_assert(sizeof(handle) >= sizeof(mesh_info));
    std::memcpy(&handle, &mesh_info, sizeof(mesh_info));
    return handle;
}

/** Dereference mesh handle. */
inline const MeshInfo& mesh_info(MeshHandle handle) noexcept
{
    const MeshInfo* p{};
    std::memcpy(&p, &handle, sizeof(handle));
    MG_ASSERT(p != nullptr);
    return *p;
}

} // namespace Mg::gfx::internal
