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
#include "mg/gfx/mg_mesh_data.h"
#include "mg/utils/mg_opaque_handle.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <cstring>

namespace Mg::gfx::internal {

using SubMeshRanges = small_vector<SubMeshRange, 4>;

/** Internal mesh structure. @see MeshRepository */
struct GpuMesh {
    SubMeshRanges submeshes;
    glm::vec3 centre{};
    float radius{};
    Identifier mesh_id{ "" };

    // Identifier for the mesh buffers in the graphics API.
    OpaqueHandle::Value vertex_array_id{};
    OpaqueHandle::Value vertex_buffer_id{};
    OpaqueHandle::Value index_buffer_id{};
};

inline MeshHandle make_mesh_handle(const GpuMesh* gpu_mesh) noexcept
{
    MeshHandle handle{};
    static_assert(sizeof(handle) >= sizeof(gpu_mesh));
    std::memcpy(&handle, &gpu_mesh, sizeof(gpu_mesh));
    return handle;
}

/** Dereference mesh handle. */
inline GpuMesh& get_gpu_mesh(MeshHandle handle) noexcept
{
    GpuMesh* p = nullptr;
    std::memcpy(&p, &handle, sizeof(handle));
    MG_ASSERT(p != nullptr);
    return *p;
}


} // namespace Mg::gfx::internal
