//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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
    glm::vec3 centre{};
    float radius{};
    Identifier mesh_id{ "" };

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
