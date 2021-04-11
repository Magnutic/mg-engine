//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_render_command_list.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_transform.h"
#include "mg/gfx/mg_camera.h"
#include "mg/gfx/mg_frustum.h"
#include "mg/utils/mg_stl_helpers.h"

#include "mg_mesh_internal.h"

#include <fmt/core.h>

#include <cstring>

namespace Mg::gfx {

namespace {

Material* material_for_submesh(span<const MaterialBinding> material_bindings, size_t sub_mesh_index)
{
    auto it = find_if(material_bindings, [&](const MaterialBinding& mb) {
        return mb.sub_mesh_index == sub_mesh_index;
    });

    if (it != material_bindings.end()) {
        return it->material;
    }

    if (sub_mesh_index == 0) {
        return nullptr;
    }

    return material_for_submesh(material_bindings, 0);
}

// Key used for sorting render commands.
struct SortKey {
    uint32_t depth;
    uint32_t fingerprint;
    uint32_t index;
};

} // namespace

// Private data for RenderCommandProducer.
struct RenderCommandProducerData {
    RenderCommandList commands;
    std::vector<SortKey> keys;
    std::vector<RenderCommand> render_commands_unsorted;
    std::vector<glm::mat4> m_transforms_unsorted;
};

//--------------------------------------------------------------------------------------------------
// RenderCommandProducer implementation
//--------------------------------------------------------------------------------------------------

RenderCommandProducer::RenderCommandProducer() = default;

RenderCommandProducer::~RenderCommandProducer() = default;

void RenderCommandProducer::add_mesh(MeshHandle mesh_handle,
                                     const Transform& transform,
                                     span<const MaterialBinding> material_bindings)
{
    const MeshInternal& mesh = get_mesh(mesh_handle);

    for (size_t i = 0; i < mesh.submeshes.size(); ++i) {
        const auto* material = material_for_submesh(material_bindings, i);

        if (material == nullptr) {
            log.warning("No material specified for mesh '{}', submesh {}. Skipping.",
                        mesh.name.str_view(),
                        i);
            continue;
        }

        // Write render command to command list
        {
            impl().m_transforms_unsorted.emplace_back(transform.matrix());
            RenderCommand& command = impl().render_commands_unsorted.emplace_back();

            command.vertex_array = mesh.vertex_array;
            command.bounding_sphere = mesh.bounding_sphere;
            command.begin = mesh.submeshes[i].begin;
            command.amount = mesh.submeshes[i].amount;
            command.material = material;
        }
    }
}

void RenderCommandProducer::add_skinned_mesh(MeshHandle mesh,
                                             const Transform& transform,
                                             span<const MaterialBinding> material_bindings,
                                             const SkinningMatrixPalette& skinning_matrix_palette)
{
    add_mesh(mesh, transform, material_bindings);
    RenderCommand& command = impl().render_commands_unsorted.back();

    // Keep track of where the skinning matrix palette's data resides.
    command.skinning_matrices_begin = narrow<uint16_t>(skinning_matrix_palette.m_start_index);
    command.num_skinning_matrices =
        narrow<uint16_t>(skinning_matrix_palette.m_skinning_matrices.size());
}

SkinningMatrixPalette
RenderCommandProducer::allocate_skinning_matrix_palette(const uint16_t num_joints)
{
    const size_t skinning_matrices_begin = impl().commands.m_skinning_matrices.size();
    impl().commands.m_skinning_matrices.resize(skinning_matrices_begin + num_joints);
    return { span{ impl().commands.m_skinning_matrices }.subspan(skinning_matrices_begin,
                                                                 num_joints),
             narrow<uint16_t>(skinning_matrices_begin) };
}

void RenderCommandProducer::clear() noexcept
{
    RenderCommandProducerData& data = impl();

    data.keys.clear();

    data.commands.m_render_commands.clear();
    data.commands.m_m_transforms.clear();
    data.commands.m_mvp_transforms.clear();
    data.commands.m_skinning_matrices.clear();

    data.render_commands_unsorted.clear();
    data.m_transforms_unsorted.clear();
}

namespace { // Helpers for RenderCommandList::finalise

template<bool invert> bool cmp_draw_call(SortKey lhs, SortKey rhs) noexcept
{
    uint64_t lhs_int;
    uint64_t rhs_int;

    // Comparing first 8 bytes of sort keys, i.e. depth and fingerprint
    std::memcpy(&lhs_int, &lhs, sizeof(uint64_t));
    std::memcpy(&rhs_int, &rhs, sizeof(uint64_t));

    return invert ? (lhs_int > rhs_int) : (rhs_int < lhs_int);
}

bool in_view(const glm::mat4& MVP, const BoundingSphere& bounding_sphere)
{
    return !frustum_cull(MVP, bounding_sphere.centre, bounding_sphere.radius);
}

uint32_t view_depth_in_cm(const ICamera& camera, glm::vec3 pos) noexcept
{
    const float depth_f = camera.depth_at_point(pos) * 100.0f;
    return narrow_cast<uint32_t>(glm::max(0.0f, depth_f));
}

uint32_t render_command_fingerpint(const RenderCommand& command) noexcept
{
    // TODO: This fingerprint could be better.
    const uint32_t mesh_fingerprint = static_cast<uint8_t>(command.vertex_array.get());
    uintptr_t mat_ptr_as_int{};
    std::memcpy(&mat_ptr_as_int, &command.material, sizeof(mat_ptr_as_int));
    const auto material_fingerprint = narrow_cast<uint32_t>(mat_ptr_as_int);
    return (material_fingerprint << 8) | mesh_fingerprint;
}

SortKey create_sort_key(const RenderCommandProducerData& data,
                        const ICamera& camera,
                        const size_t unsorted_command_index)
{
    const RenderCommand& command = data.render_commands_unsorted[unsorted_command_index];
    const glm::mat4& m_transform = data.m_transforms_unsorted[unsorted_command_index];

    // Find distance to camera for sorting.
    // Store depth in centimetres to get better precision as uint32_t.
    const glm::vec3 translation = m_transform[3];
    const auto depth = view_depth_in_cm(camera, translation);
    const uint32_t command_fingerprint = render_command_fingerpint(command);
    return { depth, command_fingerprint, narrow<uint32_t>(unsorted_command_index) };
}

} // namespace

const RenderCommandList& RenderCommandProducer::finalise(const ICamera& camera,
                                                         SortingMode sorting_mode)
{
    RenderCommandProducerData& data = impl();

    // Create sort key sequence
    data.keys.clear();
    for (size_t i = 0; i < size(); ++i) {
        data.keys.push_back(create_sort_key(data, camera, i));
    }

    MG_ASSERT(data.keys.size() == size());

    // Sort sort-key sequence
    auto cmp = (sorting_mode == SortingMode::far_to_near) ? cmp_draw_call<true>
                                                          : cmp_draw_call<false>;
    sort(data.keys, [&](const SortKey& lhs, const SortKey& rhs) { return cmp(lhs, rhs); });

    // Write out sorted render commands to data.commands.
    data.commands.m_render_commands.reserve(size());
    data.commands.m_m_transforms.reserve(size());
    data.commands.m_mvp_transforms.reserve(size());

    const glm::mat4 VP = camera.view_proj_matrix();

    for (const SortKey& key : data.keys) {
        const RenderCommand& command = data.render_commands_unsorted[key.index];
        const glm::mat4& M = data.m_transforms_unsorted[key.index];
        const glm::mat4 MVP = VP * M;

        if (in_view(MVP, command.bounding_sphere)) {
            data.commands.m_render_commands.emplace_back(data.render_commands_unsorted[key.index]);
            data.commands.m_m_transforms.emplace_back(M);
            data.commands.m_mvp_transforms.emplace_back(MVP);
        }
    }

    return data.commands;
}

size_t RenderCommandProducer::size() const noexcept
{
    return impl().render_commands_unsorted.size();
}

} // namespace Mg::gfx
