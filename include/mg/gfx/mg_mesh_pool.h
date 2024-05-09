//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_pool.h
 * Creates, stores, and updates meshes.
 */

#pragma once

#include "mg/gfx/mg_mesh_buffer.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"

#include <cstddef>
#include <memory>

namespace Mg {
class MeshResource;
class ResourceCache;
} // namespace Mg

namespace Mg::gfx {

class MeshBuffer;

struct MeshPoolImpl;

/** Creates, stores, and updates meshes. */
class MeshPool {
public:
    MG_MAKE_NON_COPYABLE(MeshPool);
    MG_MAKE_NON_MOVABLE(MeshPool);

    explicit MeshPool(std::shared_ptr<ResourceCache> resource_cache);

    ~MeshPool();

    /** Get or load a mesh using the given mesh resource. */
    const Mesh* get_or_load(Identifier resource_id);

    /** Create a new mesh using the given mesh data. Expects that no mesh with the same
     * identifier already exists -- if one does, use update() instead.
     */
    const Mesh* create(const mesh_data::MeshDataView& mesh_data, Identifier name);

    /** Find the mesh with the given name, if such a mesh exists. Otherwise, returns nullptr. */
    const Mesh* find(Identifier name) const;

    /** Destroy the given mesh. Unless another mesh uses the same GPU data buffers (as would be the
     * case if meshes were created using the same `Mg::gfx::MeshBuffer`), then the GPU buffers will
     * be freed.
     */
    void destroy(const Mesh* handle);

    /** Update an existing mesh using the given mesh data.
     * Returns true if a mesh was updated; false if no matching mesh existed in the repository.
     */
    bool update(const mesh_data::MeshDataView& mesh_data, Identifier name);

    /** Update the mesh that was created from resource.
     * Used for hot-reloading of mesh files.
     * Returns true if a mesh was updated; false if no matching mesh existed in the repository.
     */
    bool update(const MeshResource& mesh_res);

    /** Create a mesh buffer of given size. This buffer allows you to create meshes sharing the same
     * underlying GPU storage buffers.
     */
    MeshBuffer new_mesh_buffer(mesh_data::VertexBufferSize vertex_buffer_size,
                               mesh_data::IndexBufferSize index_buffer_size,
                               mesh_data::InfluencesBufferSize influences_buffer_size =
                                   mesh_data::InfluencesBufferSize{ 0 });

private:
    ImplPtr<MeshPoolImpl> m_impl;
};

} // namespace Mg::gfx
