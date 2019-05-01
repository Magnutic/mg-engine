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

#include "mg/core/mg_log.h"
#include "mg/core/mg_transform.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_frustum.h"
#include "mg/utils/mg_stl_helpers.h"

#include "mg_mesh_info.h"
#include "mg_texture_node.h"

#include <fmt/core.h>

#include <cstring>

namespace Mg::gfx {

namespace {

Material* material_for_submesh(span<const MaterialBinding> material_bindings, size_t sub_mesh_index)
{
    auto it = find_if(material_bindings, [&](const MaterialBinding& mb) {
        return mb.sub_mesh_index == sub_mesh_index;
    });

    if (it != material_bindings.end()) { return it->material; }

    if (sub_mesh_index == 0) { return nullptr; }

    return material_for_submesh(material_bindings, 0);
}

} // namespace

void RenderCommandProducer::add_mesh(MeshHandle                  mesh,
                                     const Transform&            transform,
                                     span<const MaterialBinding> material_bindings)
{
    const internal::MeshInfo& md = internal::mesh_info(mesh);

    for (size_t i = 0; i < md.submeshes.size(); ++i) {
        auto material = material_for_submesh(material_bindings, i);

        if (material == nullptr) {
            auto msg = fmt::format("No material specified for mesh '{}', submesh {}. Skipping.",
                                   md.mesh_id.c_str(),
                                   i);
            g_log.write_warning(msg);
            continue;
        }

        // Write render command to command list
        {
            m_m_transform_matrices_unsorted.emplace_back(transform.matrix());
            RenderCommand& command = m_render_commands_unsorted.emplace_back();

            command.gfx_api_mesh_object_id = md.gfx_api_mesh_object_id;
            command.centre                 = md.centre;
            command.begin                  = md.submeshes[i].begin;
            command.amount                 = md.submeshes[i].amount;
            command.material               = material;
            command.radius                 = md.radius;
        }
    }
}

void RenderCommandProducer::clear()
{
    m_keys.clear();
    m_commands.m_render_commands.clear();
    m_commands.m_m_transform_matrices.clear();
    m_commands.m_mvp_transform_matrices.clear();
    m_render_commands_unsorted.clear();
    m_m_transform_matrices_unsorted.clear();
}

namespace {

template<bool invert>
bool cmp_draw_call(RenderCommandProducer::SortKey lhs, RenderCommandProducer::SortKey rhs) noexcept
{
    uint64_t lhs_int;
    uint64_t rhs_int;

    // Comparing first 8 bytes of sort keys, i.e. depth and fingerprint
    std::memcpy(&lhs_int, &lhs, sizeof(uint64_t));
    std::memcpy(&rhs_int, &rhs, sizeof(uint64_t));

    return invert ? (lhs_int > rhs_int) : (rhs_int < lhs_int);
}

bool in_view(const glm::mat4& M, const glm::mat4& MVP, glm::vec3 centre, float radius)
{
    const glm::vec3 scale{ M[0][0], M[1][1], M[2][2] };
    const float     scale_factor = std::max(std::max(scale.x, scale.y), scale.z);

    return !frustum_cull(MVP, centre, scale_factor * radius);
}

uint32_t view_depth_in_cm(const ICamera& camera, glm::vec3 pos)
{
    const float depth_f = camera.depth_at_point(pos) * 100.0f;
    return static_cast<uint32_t>(glm::max(0.0f, depth_f));
}

uint32_t render_command_fingerpint(const RenderCommand& command)
{
    // TODO: This fingerprint is not much good
    const uint32_t mesh_fingerprint     = static_cast<uint8_t>(command.gfx_api_mesh_object_id);
    const auto     material_fingerprint = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(command.material)); // NOLINT
    return (material_fingerprint << 8) | mesh_fingerprint;
}

} // namespace

// TODO: make sure sort is correct
const RenderCommandList& RenderCommandProducer::finalise(const ICamera& camera, SortFunc sort_func)
{
    // Create sort key sequence
    m_keys.clear();

    for (uint32_t i = 0; i < size(); ++i) {
        const RenderCommand& command = m_render_commands_unsorted[i];
        const glm::mat4&     M       = m_m_transform_matrices_unsorted[i];

        // Find distance to camera for sorting
        // Store depth in cm to get better precision as uint32_t
        const glm::vec3 translation = M[3];
        const auto      depth       = view_depth_in_cm(camera, translation);

        const uint32_t command_fingerprint = render_command_fingerpint(command);

        m_keys.push_back({ depth, command_fingerprint, i });
    }

    MG_ASSERT(m_keys.size() == size());

    // Sort sort-key sequence
    auto cmp = (sort_func == SortFunc::FAR_TO_NEAR) ? cmp_draw_call<true> : cmp_draw_call<false>;
    sort(m_keys, [&](const SortKey& lhs, const SortKey& rhs) { return cmp(lhs, rhs); });

    // Write out sorted render commands to m_commands.
    m_commands.m_render_commands.reserve(size());
    m_commands.m_m_transform_matrices.reserve(size());
    m_commands.m_mvp_transform_matrices.reserve(size());

    const glm::mat4 VP = camera.view_proj_matrix();

    for (const SortKey& key : m_keys) {
        const RenderCommand& command = m_render_commands_unsorted[key.index];
        const glm::mat4&     M       = m_m_transform_matrices_unsorted[key.index];
        const glm::mat4      MVP     = VP * M;

        if (in_view(M, MVP, command.centre, command.radius)) {
            m_commands.m_render_commands.emplace_back(m_render_commands_unsorted[key.index]);
            m_commands.m_m_transform_matrices.emplace_back(M);
            m_commands.m_mvp_transform_matrices.emplace_back(MVP);
        }
    }

    return m_commands;
}

} // namespace Mg::gfx
