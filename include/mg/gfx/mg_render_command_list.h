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
#include "mg/utils/mg_simple_pimpl.h"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <vector>

namespace Mg {
class Transform;
}

namespace Mg::gfx {

class Material;
class Skeleton;
class SkeletonPose;

/** Function for sorting draw calls */
enum class SortingMode { near_to_far, far_to_near };

/** Description of an individual draw call. */
struct RenderCommand {
    /** Bounding sphere for quick frustum culling. */
    BoundingSphere bounding_sphere;

    /** Vertex array handle points out the GPU data buffers for the draw call. */
    VertexArrayHandle vertex_array{};

    /** Begin and end indices, into in the vertex-index buffer. */
    uint32_t begin{};
    uint32_t amount{};

    /** Material to use for this draw call. */
    const Material* material{};

    /** If rendering a skinned mesh, this will point out which skinning matrices to upload to GPU.
     * If not, `num_skinning_matrices` will be 0.
     */
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
    /** Sequence of render commands, containing references to the data that is reachable by the
     * other member functions on this class.
     */
    span<const RenderCommand> render_commands() const noexcept { return m_render_commands; }

    /** The model-to-world space transformation matrices for each render command. This array is
     * sorted in the same order as `render_commands`, i.e. the matrix for `render_commands()[i]` is
     * `m_transforms()[i]`
     */
    span<const glm::mat4> m_transforms() const noexcept { return m_m_transforms; }

    /** The model-view-perspective space transformation matrices for each render command. This array
     * is sorted in the same order as `render_commands`, i.e. the matrix for `render_commands()[i]`
     * is `m_transforms()[i]`
     */
    span<const glm::mat4> mvp_transforms() const noexcept { return m_mvp_transforms; }

    /** Skinning matrix palette for skinned (animated) meshes.
     * `RenderCommand::skinning_matrices_begin` and `RenderCommand::num_skinning_matrices` refer to
     * this array.
     */
    span<const glm::mat4> skinning_matrices() const noexcept { return m_skinning_matrices; }

private:
    std::vector<RenderCommand> m_render_commands;
    std::vector<glm::mat4> m_m_transforms;
    std::vector<glm::mat4> m_mvp_transforms;
    std::vector<glm::mat4> m_skinning_matrices;
};

/** Matrix palette for skinned meshes. This is a view into data owned by `RenderCommandList`.
 * Construct using `RenderCommandList::allocate_skinning_matrix_palette.`
 * @note Objects of this class will be invalidated when the originating `RenderCommandProducer` is
 * cleared (`RenderCommandProducer::clear`) or destroyed.
 */
class SkinningMatrixPalette {
public:
    /** Access to the skinning_matrices. Write the appropriate data to this before rendering.
     * @see Mg::gfx::calculate_skinning_matrices
     */
    span<glm::mat4> skinning_matrices() const noexcept { return m_skinning_matrices; }

private:
    friend class RenderCommandProducer;

    SkinningMatrixPalette(const span<glm::mat4> skinning_matrices, const uint16_t start_index)
        : m_skinning_matrices(skinning_matrices), m_start_index(start_index)
    {}

    span<glm::mat4> m_skinning_matrices;
    const uint16_t m_start_index;
};

struct RenderCommandProducerData;

/** Interface for producing RenderCommandList. */
class RenderCommandProducer : PImplMixin<RenderCommandProducerData> {
public:
    explicit RenderCommandProducer();
    ~RenderCommandProducer();

    MG_MAKE_NON_COPYABLE(RenderCommandProducer);
    MG_MAKE_NON_MOVABLE(RenderCommandProducer);

    /** Add a non-animated mesh to be rendered, with the given transformation and material
     * assignments.
     */
    void add_mesh(MeshHandle mesh,
                  const Transform& transform,
                  span<const MaterialBinding> material_bindings);

    /** Add a skinned (animated) mesh to be rendered, with the given transformation and material
     * assignments.
     */
    void add_skinned_mesh(MeshHandle mesh,
                          const Transform& transform,
                          span<const MaterialBinding> material_bindings,
                          const SkinningMatrixPalette& skinning_matrix_palette);

    /** Allocate space for a skinning matrix palette, for use with `add_skinned_mesh`. The matrix
     * palette must be filled with the appropriate skinning matrix data.
     *
     * @see Mg::gfx::calculate_skinning_matrices.
     *
     * @note The reason for this interface -- letting the caller calculate the matrices, instead of
     * just calculating them automatically within this class -- is that the caller can then decide
     * how and when to perform the calculations. For example, the jobs could then be divided to
     * multiple threads.
     */
    [[nodiscard]] SkinningMatrixPalette allocate_skinning_matrix_palette(uint16_t num_joints);

    /** Removes all added render commands, resetting the state of the RenderCommandProducer.
     * N.B. it is better to re-use the same RenderCommandProducer and clear each frame than to
     * create a new one, since the former approach allows the same heap-memory buffers to be
     * re-used.
     */
    void clear() noexcept;

    /** Sorts and frustum culls draw list and makes render commands available as
     * RenderCommandList.
     * @param camera The camera to consider for sorting and frustum culling.
     * @param sorting_mode Sorting order for the command sequence.
     * @return Reference to sorted command sequence along with associated transformation matrices.
     */
    const RenderCommandList& finalise(const ICamera& camera, SortingMode sorting_mode);

    /** Number of enqueued RenderCommand instances. */
    size_t size() const noexcept;
};

} // namespace Mg::gfx
