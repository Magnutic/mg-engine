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

#include <cstdint>
#include <memory>
#include <vector>

#include "mg/core/mg_transform.h"
#include "mg/gfx/mg_mesh_handle.h"
#include "mg/gfx/mg_render_command_data_handle.h"
#include "mg/utils/mg_gsl.h"

#include "mg/mg_defs.h"

namespace Mg::gfx {

class Material;

/** Function for sorting draw calls */
enum class SortFunc { NEAR_TO_FAR, FAR_TO_NEAR };

/** Description of an individual draw call (one submesh, with transform, material, and metadata). */
struct RenderCommand {
    // May be expanded with different types of commands, flags?
    RenderCommandDataHandle data;
    bool                    culled;
};

/** Tells which material to use for a given submesh (by numeric index). */
struct MaterialBinding {
    size_t    sub_mesh_index = 0;
    Material* material;
};

class ICamera;

/** List of draw calls to be rendered. */
class RenderCommandList {
public:
    explicit RenderCommandList();

    void
    add_mesh(MeshHandle mesh, Transform transform, span<const MaterialBinding> material_bindings);

    void clear()
    {
        m_keys.clear();
        m_render_commands.clear();
        m_command_data_offset = 0;
    }

    void frustum_cull_draw_list(const ICamera& camera);

    void sort_draw_list(const ICamera& camera, SortFunc sf);

    const RenderCommand& operator[](size_t i) const { return m_render_commands[m_keys[i].index]; }

    size_t size() const { return m_render_commands.size(); }

    span<const uint8_t> command_buffer_data() const noexcept;

    struct SortKey {
        uint32_t depth;
        uint32_t fingerprint;
        uint32_t index;
    };

private:
    std::vector<SortKey>       m_keys;
    std::vector<RenderCommand> m_render_commands;
    std::unique_ptr<uint8_t[]> m_command_data;
    uint32_t                   m_command_data_offset = 0;
};

} // namespace Mg::gfx
