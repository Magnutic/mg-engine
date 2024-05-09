//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_buffer.h
 * GPU-side buffers for mesh data that can be shared among several meshes.
 */

#pragma once

#include "mg/gfx/mg_mesh_data.h"
#include "mg/gfx/mg_mesh_handle.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_optional.h"

namespace Mg {
class MeshResource;
} // namespace Mg

namespace Mg::gfx {

struct MeshPoolImpl;

/** MeshBuffer allows creating meshes within pre-allocated buffers on the GPU. This is useful for
 * performance reasons -- keeping meshes that are often used together in the same buffer may result
 * in better performance.
 *
 * Construct using Mg::MeshPool::new_mesh_buffer()
 */
class MeshBuffer {
public:
    enum class ReturnCode {
        Success,
        Vertex_buffer_full,
        Index_buffer_full,
        Influences_buffer_full
    };

    struct CreateReturn {
        Opt<MeshHandle> opt_mesh;
        ReturnCode return_code;
    };

    /** Try to create a new mesh in this buffer using the given mesh resource. */
    CreateReturn create_in_buffer(const MeshResource& resource);

    /** Try to create a new mesh in this buffer using the given mesh data. */
    CreateReturn create_in_buffer(const mesh_data::MeshDataView& mesh_data, Identifier name);

    class Impl;

private:
    friend class MeshPool;

    MeshBuffer(MeshPoolImpl& mesh_pool,
               mesh_data::VertexBufferSize vertex_buffer_size,
               mesh_data::IndexBufferSize index_buffer_size,
               mesh_data::InfluencesBufferSize influences_buffer_size);

    ImplPtr<Impl> m_impl;
};

} // namespace Mg::gfx
