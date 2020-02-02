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

/** @file mg_mesh_repository.h
 * Creates, stores, and updates meshes.
 */

#pragma once

#include "mg/gfx/mg_mesh_handle.h"
#include "mg/gfx/mg_submesh.h"
#include "mg/gfx/mg_vertex.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <cstddef>

namespace Mg {
class MeshResource;
}

namespace Mg::gfx {

class MeshBufferImpl;

/** MeshBuffer allows creating meshes within pre-allocated buffers on the GPU. This is useful for
 * performance reasons -- keeping meshes that are often used together in the same buffer may result
 * in better performance.
 *
 * Construct using Mg::MeshRepository::new_mesh_buffer()
 */
class MeshBuffer : PImplMixin<MeshBufferImpl> {
public:
    MG_MAKE_NON_COPYABLE(MeshBuffer);
    MG_MAKE_DEFAULT_MOVABLE(MeshBuffer);
    ~MeshBuffer();

    enum class ReturnCode { Success, Vertex_buffer_full, Index_buffer_full };

    struct CreateReturn {
        Opt<MeshHandle> opt_mesh;
        ReturnCode      return_code;
    };

    CreateReturn create(const MeshResource& resource);

private:
    friend class MeshRepository;
    using PImplMixin::PImplMixin; // Constructor forwarding to pImpl class constructor.
};

//--------------------------------------------------------------------------------------------------

/** Strongly typed size type for vertex buffers, specified in number of bytes.
 * Can be casted to and from size_t using static_cast.
 * Can also be constructed directly, for example: `VertexBufferSize{ 512 }`
 */
enum class VertexBufferSize : size_t;

/** Strongly typed size type for vertex index buffers, specified in number of bytes.
 * Can be casted to and from size_t using static_cast.
 * Can also be constructed directly, for example: `IndexBufferSize{ 512 }`
 */
enum class IndexBufferSize : size_t;

class MeshRepositoryImpl;

struct MeshDataView {
    span<const Vertex>            vertices;
    span<const uint_vertex_index> indices;
    span<const SubMesh>           sub_meshes;

    struct BoundingInfo {
        glm::vec3 centre = {};
        float     radius = 0.0f;
    };
    Opt<BoundingInfo> bounding_info;
};

/** Creates, stores, and updates meshes. */
class MeshRepository : PImplMixin<MeshRepositoryImpl> {
public:
    MeshRepository();
    MG_MAKE_NON_COPYABLE(MeshRepository);
    MG_MAKE_DEFAULT_MOVABLE(MeshRepository);
    ~MeshRepository();

    /** Create a new mesh using the given mesh resource. */
    MeshHandle create(const MeshResource& mesh_res);

    MeshHandle create(const MeshDataView& mesh_data, Identifier mesh_id);

    Opt<MeshHandle> get(Identifier mesh_id) const;

    /** Destroy the given mesh. Unless another mesh uses the same GPU data buffers (as would be the
     * case if meshes were created using the same `Mg::gfx::MeshBuffer`), then the GPU buffers will
     * be freed.
     */
    void destroy(MeshHandle handle);

    /** Update the mesh that was created from resource.
     * Used for hot-reloading of mesh files.
     * Returns true if a mesh was updated; false if no matching mesh existed in the repository.
     */
    bool update(const MeshResource& mesh_res);

    /** Create a mesh buffer of given size. This buffer allows you to create meshes sharing the same
     * underlying GPU storage buffers.
     */
    MeshBuffer new_mesh_buffer(VertexBufferSize vertex_buffer_size,
                               IndexBufferSize  index_buffer_size);
};

} // namespace Mg::gfx
