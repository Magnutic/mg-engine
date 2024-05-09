//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_internal.h
 * Internal mesh structure. @see MeshPool
 */

#pragma once

#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_identifier.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/gfx/mg_mesh_handle.h"

#include <bit>
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

/** Internal mesh structure. @see MeshPool */
struct MeshInternal {
    /** Submeshes, defined as ranges in the index_buffer. */
    small_vector<Mesh::SubmeshRange, 8> submeshes;

    /** Bounding sphere used for frustum culling. */
    BoundingSphere bounding_sphere;

    /** Bounding box covering all vertices in the mesh. */
    AxisAlignedBoundingBox aabb;

    /** Mesh identifier, for debugging purposes. */
    Identifier name{ "" };

    /** Identifier for the mesh buffers in the graphics API. */
    VertexArrayHandle vertex_array;

    /** Vertex data buffer. */
    SharedBuffer* vertex_buffer = nullptr;

    /** Index buffer, triangle list of indexes into vertex_buffer. */
    SharedBuffer* index_buffer = nullptr;

    /** Buffer for per-vertex joint influences, for skeletal animation. May be nullptr. */
    SharedBuffer* influences_buffer = nullptr;
};

/** Convert pointer to public opaque handle. */
inline MeshHandle make_mesh_handle(const MeshInternal* p) noexcept
{
    return std::bit_cast<MeshHandle>(p);
}

/** Dereference mesh handle. */
inline MeshInternal& get_mesh(MeshHandle handle) noexcept
{
    MG_ASSERT(handle != MeshHandle{}); // Not null
    return *std::bit_cast<MeshInternal*>(handle);
}


} // namespace Mg::gfx
