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

#include "mg/gfx/mg_render_command_list.h"

#include <cstring>

#include <fmt/core.h>

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_frustum.h"
#include "mg/utils/mg_stl_helpers.h"

#include "mg_mesh_info.h"
#include "mg_render_command_data.h"
#include "mg_texture_node.h"

namespace Mg::gfx {

inline Material* material_for_submesh(span<const MaterialBinding> material_bindings,
                                      size_t                      sub_mesh_index)
{
    auto it = find_if(material_bindings, [&](const MaterialBinding& mb) {
        return mb.sub_mesh_index == sub_mesh_index;
    });

    if (it != material_bindings.end()) { return it->material; }

    if (sub_mesh_index == 0) { return nullptr; }

    return material_for_submesh(material_bindings, 0);
}

RenderCommandList::RenderCommandList()
    : m_command_data(std::make_unique<uint8_t[]>(defs::k_render_command_data_buffer_size))
{}

void RenderCommandList::add_mesh(MeshHandle                  mesh,
                                 Transform                   transform,
                                 span<const MaterialBinding> material_bindings)
{
    const internal::MeshInfo& md = internal::mesh_info(mesh);

    for (size_t i = 0; i < md.submeshes.size(); ++i) {
        auto material = material_for_submesh(material_bindings, i);

        if (material == nullptr) {
            g_log.write_warning(
                fmt::format("No material specified for mesh '{}', submesh {}. Skipping.",
                            md.mesh_id.c_str(),
                            i));
            continue;
        }

        void* p_command_data = nullptr;

        // Write data to command data buffer
        {
            // TODO: Consider whether this can be optimised to write directly to m_command_data
            internal::RenderCommandData command_data{};

            command_data.M           = transform.matrix();
            command_data.mesh_vao_id = md.vao_id;
            command_data.centre      = md.centre;
            command_data.begin       = md.submeshes[i].begin;
            command_data.amount      = md.submeshes[i].amount;
            command_data.material    = material;
            command_data.radius      = md.radius;

            // Write to final location in buffer
            p_command_data = &m_command_data[m_command_data_offset];
            std::memcpy(p_command_data, &command_data, sizeof(command_data));

            m_command_data_offset += sizeof(command_data);
        }

        // Write render command to command list
        m_render_commands.push_back(
            { internal::cast_to_render_command_data_handle(p_command_data), false });

        // Write dummy sort-key (Overwritten with proper values upon call to sort_draw_list())
        m_keys.push_back(SortKey{ 0, 0, narrow<uint32_t>(m_render_commands.size() - 1) });
    }
}

void RenderCommandList::frustum_cull_draw_list(const ICamera& camera)
{
    const glm::mat4 VP = camera.view_proj_matrix();

    for (RenderCommand& command : m_render_commands) {
        auto& command_data = internal::get_command_data(command.data);

        const glm::mat4& M      = command_data.M;
        float            radius = command_data.radius;

        const glm::vec3 scale{ M[0][0], M[1][1], M[2][2] };
        const glm::mat4 MVP          = VP * M;
        const float     scale_factor = std::max(std::max(scale.x, scale.y), scale.z);

        command.culled = frustum_cull(MVP, command_data.centre, scale_factor * radius);
    }
}

template<bool invert>
inline bool cmp_draw_call(RenderCommandList::SortKey lhs, RenderCommandList::SortKey rhs) noexcept
{
    uint64_t lhs_int;
    uint64_t rhs_int;

    // Comparing first 8 bytes of sort keys, i.e. depth and fingerprint
    std::memcpy(&lhs_int, &lhs, sizeof(uint64_t));
    std::memcpy(&rhs_int, &rhs, sizeof(uint64_t));

    return invert ? (lhs_int > rhs_int) : (rhs_int < lhs_int);
}

// TODO: make sure sort is correct
void RenderCommandList::sort_draw_list(const ICamera& camera, SortFunc sf)
{
    for (uint32_t i = 0; i < m_render_commands.size(); ++i) {
        const RenderCommand& command      = m_render_commands[i];
        auto&                command_data = internal::get_command_data(command.data);

        // Find distance to camera for sorting
        const glm::vec3 translation = command_data.M[3];

        // Store depth in cm to get better precision as uint32_t
        const float depth_f = camera.depth_at_point(translation) * 100.0f;
        const auto  depth   = uint32_t(glm::max(0.0f, depth_f));

        // TODO: This fingerprint is not much good
        const uint32_t mesh_fingerprint = command_data.mesh_vao_id & 0x0F;
        const auto     material_fingerprint =
            uint32_t(reinterpret_cast<uintptr_t>(command_data.material)); // NOLINT
        const uint32_t draw_call_fingerprint = (material_fingerprint << 8) | mesh_fingerprint;

        m_keys[i] = SortKey{ depth, draw_call_fingerprint, i };
    }

    auto cmp = sf == SortFunc::FAR_TO_NEAR ? cmp_draw_call<true> : cmp_draw_call<false>;
    sort(m_keys, [&](SortKey lhs, SortKey rhs) { return cmp(lhs, rhs); });
}

span<const uint8_t> RenderCommandList::command_buffer_data() const noexcept
{
    return { m_command_data.get(), defs::k_render_command_data_buffer_size };
}

} // namespace Mg::gfx
