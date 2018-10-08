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

#include <cstddef>
#include <memory>
#include <optional>

#include <mg/gfx/mg_mesh_handle.h>
#include <mg/utils/mg_macros.h>

namespace Mg {
class MeshResource;
}

namespace Mg::gfx {

/** MeshBuffer allows creating meshes within pre-allocated buffers on the GPU. This is useful for
 * performance reasons -- keeping meshes that are often used together in the same buffer may result
 * in better performance.
 *
 * Construct using Mg::MeshRepository::new_mesh_buffer()
 */
class MeshBuffer {
public:
    MG_MAKE_NON_COPYABLE(MeshBuffer);
    MG_MAKE_DEFAULT_MOVABLE(MeshBuffer);
    ~MeshBuffer();

    enum class ReturnCode { Success, Vertex_buffer_full, Index_buffer_full };

    struct CreateReturn {
        std::optional<MeshHandle> opt_mesh;
        ReturnCode                return_code;
    };

    CreateReturn create(const MeshResource& resource);

private:
    friend class MeshRepository;
    class Impl;

    std::unique_ptr<Impl> m_impl;
    MeshBuffer(std::unique_ptr<Impl> impl);
};

//--------------------------------------------------------------------------------------------------

enum class VertexBufferSize : size_t;
enum class IndexBufferSize : size_t;

/** Creates, stores, and updates meshes. */
class MeshRepository {
public:
    MeshRepository();
    MG_MAKE_NON_COPYABLE(MeshRepository);
    MG_MAKE_DEFAULT_MOVABLE(MeshRepository);
    ~MeshRepository();

    /** Create a new mesh using the given mesh resource. */
    MeshHandle create(const MeshResource& mesh_res);

    /** Destroy the given mesh. Unless another mesh uses the same GPU data buffers (as would be the
     * case if meshes were created using the same `Mg::gfx::MeshBuffer`), then the GPU buffers will
     * be freed.
     */
    void destroy(MeshHandle handle);

    /** Create a mesh buffer of given size. This buffer allows you to create meshes sharing the same
     * underlying GPU storage buffers.
     */
    MeshBuffer new_mesh_buffer(VertexBufferSize vertex_buffer_size,
                               IndexBufferSize  index_buffer_size);

private:
    friend class MeshBuffer;

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Mg::gfx
