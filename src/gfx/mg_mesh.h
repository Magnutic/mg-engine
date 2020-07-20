//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh.h
 * Internal mesh structure. @see MeshRepository
 */

#pragma once

#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/gfx/mg_mesh_handle.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <cstring>

namespace Mg::gfx {

// Vertex and index buffers may be shared between multiple meshes.
// This structure lets us keep track of how many meshes are using a given buffer, so that we can
// know when it is safe to destroy.
struct SharedBuffer {
    BufferHandle handle;
    int32_t num_users = 0;
};

using SubMeshRanges = small_vector<SubMeshRange, 4>;

/** Internal mesh structure. @see MeshRepository */
struct Mesh {
    SubMeshRanges submeshes;
    glm::vec3 centre{};
    float radius{};
    Identifier name{ "" };

    // Identifier for the mesh buffers in the graphics API.
    VertexArrayHandle vertex_array;
    SharedBuffer* vertex_buffer = nullptr;
    SharedBuffer* index_buffer = nullptr;
};

inline MeshHandle make_mesh_handle(const Mesh* mesh) noexcept
{
    MeshHandle handle{};
    static_assert(sizeof(handle) >= sizeof(mesh));
    std::memcpy(&handle, &mesh, sizeof(mesh));
    return handle;
}

/** Dereference mesh handle. */
inline Mesh& get_mesh(MeshHandle handle) noexcept
{
    Mesh* p = nullptr;
    std::memcpy(&p, &handle, sizeof(handle));
    MG_ASSERT(p != nullptr);
    return *p;
}


} // namespace Mg::gfx
