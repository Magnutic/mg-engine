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

/** @file mg_render_command_list.h
 * Data structure of render commands (draw calls)
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/gfx/mg_mesh_handle.h"
#include "mg/gfx/mg_render_command_data_handle.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_opaque_handle.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

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
    // May be expanded with different types of commands, flags?
    glm::vec3 centre{};
    float     radius{};

    OpaqueHandle::Value gfx_api_mesh_object_id{};

    uint32_t begin{};
    uint32_t amount{};

    const Material* material{};
};

/** Tells which material to use for a given submesh (by numeric index). */
struct MaterialBinding {
    size_t    sub_mesh_index = 0;
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

private:
    std::vector<RenderCommand> m_render_commands;
    std::vector<glm::mat4>     m_m_transform_matrices;
    std::vector<glm::mat4>     m_mvp_transform_matrices;
};

/** Interface for producing RenderCommandList. */
class RenderCommandProducer {
public:
    void add_mesh(MeshHandle                  mesh,
                  const Transform&            transform,
                  span<const MaterialBinding> material_bindings);

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
    std::vector<glm::mat4>     m_m_transform_matrices_unsorted;
};

} // namespace Mg::gfx
