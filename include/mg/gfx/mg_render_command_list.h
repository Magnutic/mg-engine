//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_render_command_list.h
 * Data structure of render commands (draw calls)
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_mesh_handle.h"
#include "mg/mg_bounding_volumes.h"
#include "mg/utils/mg_gsl.h"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <vector>

namespace Mg {
class Transform;
}

namespace Mg::gfx {

class Material;

/** Function for sorting draw calls */
enum class SortFunc { NEAR_TO_FAR, FAR_TO_NEAR };

/** Description of an individual draw call. */
struct RenderCommand {
    // Bounding sphere for quick frustum culling.
    BoundingSphere bounding_sphere;

    // Vertex array handle points out the GPU data buffers for the draw call.
    VertexArrayHandle vertex_array{};

    // Begin and end indices, into in the vertex-index buffer.
    uint32_t begin{};
    uint32_t amount{};

    // Material to use for this draw call.
    const Material* material{};

    // If rendering a skinned mesh, this will point out which skinning matrices to upload to GPU.
    // If not, num_skinning_matrices will be 0.
    uint16_t skinning_matrices_begin{};
    uint16_t num_skinning_matrices{};
};

/** Tells which material to use for a given submesh (by numeric index). */
struct MaterialBinding {
    size_t sub_mesh_index = 0;
    Material* material{};
};

class ICamera;

/** List of draw calls to be rendered. */
class RenderCommandList {
    friend class RenderCommandProducer;

public:
    span<const RenderCommand> render_commands() const noexcept { return m_render_commands; }
    span<const glm::mat4> m_transform_matrices() const noexcept { return m_m_transform_matrices; }
    span<const glm::mat4> mvp_transform_matrices() const noexcept
    {
        return m_mvp_transform_matrices;
    }
    span<const glm::mat4> skinning_matrices() const noexcept { return m_skinning_matrices; }

private:
    std::vector<RenderCommand> m_render_commands;
    std::vector<glm::mat4> m_m_transform_matrices;
    std::vector<glm::mat4> m_mvp_transform_matrices;
    std::vector<glm::mat4> m_skinning_matrices;
};

/** Interface for producing RenderCommandList. */
class RenderCommandProducer {
public:
    void add_mesh(MeshHandle mesh,
                  const Transform& transform,
                  span<const MaterialBinding> material_bindings);

    void add_skinned_mesh(MeshHandle mesh,
                          const Transform& transform,
                          span<const MaterialBinding> material_bindings,
                          span<const glm::mat4> skinning_matrices);

    /** Removes all added render commands, resetting the state of the RenderCommandProducer.
     * N.B. it is better to re-use the same RenderCommandProducer and clear each frame than to
     * create a new one, since the former approach allows the same heap-memory buffers to be
     * re-used.
     */
    void clear() noexcept;

    /** Sorts and frustum culls draw list and makes render commands available as
     * RenderCommandList.
     * @param camera The camera to consider for sorting and frustum culling.
     * @param sort_func Sorting order for the command sequence.
     * @return Reference to sorted command sequence along with associated transformation matrices.
     */
    const RenderCommandList& finalise(const ICamera& camera, SortFunc sort_func);

    size_t size() const noexcept { return m_render_commands_unsorted.size(); }

    struct SortKey {
        uint32_t depth;
        uint32_t fingerprint;
        uint32_t index;
    };

private:
    RenderCommandList m_commands;

    std::vector<SortKey> m_keys;

    std::vector<RenderCommand> m_render_commands_unsorted;
    std::vector<glm::mat4> m_m_transform_matrices_unsorted;
    std::vector<glm::mat4> m_skinning_matrices;
};

} // namespace Mg::gfx
