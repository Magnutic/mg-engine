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
#include "mg/gfx/mg_mesh.h"
#include "mg/utils/mg_stl_helpers.h"

#include <fmt/core.h>

#include <cstring>

namespace Mg::gfx {

namespace {

const Material* material_for_submesh(std::span<const MaterialBinding> bindings,
                                     const mesh_data::Submesh& submesh)
{
    auto it = find_if(bindings, [&](const MaterialBinding& mb) {
        return mb.material_binding_id == submesh.material_binding_id;
    });

    if (it != bindings.end()) {
        return it->material;
    }

    return nullptr;
}

// Key used for sorting render commands.
struct SortKey {
    uint32_t depth;
    uint32_t fingerprint;
    uint32_t index;
};

} // namespace

// Private data for RenderCommandProducer.
struct RenderCommandProducer::Impl {
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

void RenderCommandProducer::add_mesh(const Mesh& mesh,
                                     const glm::mat4& transform,
                                     std::span<const MaterialBinding> material_bindings)
{
    MG_ASSERT(m_impl->m_transforms_unsorted.size() == m_impl->render_commands_unsorted.size());

    for (const auto& submesh : mesh.submeshes) {
        const auto* material = material_for_submesh(material_bindings, submesh);

        if (material == nullptr) {
            log.warning_once(10.0f,
                             "No material specified for mesh '{}', material binding {}. Skipping.",
                             mesh.name.str_view(),
                             submesh.material_binding_id.str_view());
            continue;
        }

        // Write render command to command list
        {
            m_impl->m_transforms_unsorted.emplace_back(transform);
            RenderCommand& command = m_impl->render_commands_unsorted.emplace_back();

            command.vertex_array = mesh.vertex_array;
            command.bounding_sphere = mesh.bounding_sphere;
            command.begin = submesh.index_range.begin;
            command.amount = submesh.index_range.amount;
            command.material = material;
        }
    }
}

void RenderCommandProducer::add_skinned_mesh(const Mesh& mesh,
                                             const glm::mat4& transform,
                                             std::span<const MaterialBinding> material_bindings,
                                             const SkinningMatrixPalette& skinning_matrix_palette)
{
    const size_t num_commands_before = m_impl->render_commands_unsorted.size();
    add_mesh(mesh, transform, material_bindings);

    // Keep track of where the skinning matrix palette's data resides.
    for (size_t i = num_commands_before; i < m_impl->render_commands_unsorted.size(); ++i) {
        RenderCommand& command = m_impl->render_commands_unsorted[i];
        command.skinning_matrices_begin = as<uint16_t>(skinning_matrix_palette.m_start_index);
        command.num_skinning_matrices =
            as<uint16_t>(skinning_matrix_palette.m_skinning_matrices.size());
    }
}

void RenderCommandProducer::add_skinned_mesh(const Mesh& mesh,
                                             const glm::mat4& transform,
                                             const Skeleton& skeleton,
                                             const SkeletonPose& pose,
                                             std::span<const MaterialBinding> material_bindings)
{
    auto palette = allocate_skinning_matrix_palette(skeleton);
    calculate_skinning_matrices(transform, skeleton, pose, palette.skinning_matrices());
    add_skinned_mesh(mesh, transform, material_bindings, palette);
}

SkinningMatrixPalette
RenderCommandProducer::allocate_skinning_matrix_palette(const Skeleton& skeleton)
{
    const auto num_joints = narrow<uint16_t>(skeleton.joints().size());

    const size_t skinning_matrices_begin = m_impl->commands.m_skinning_matrices.size();
    m_impl->commands.m_skinning_matrices.resize(skinning_matrices_begin + num_joints);

    return SkinningMatrixPalette{ std::span(m_impl->commands.m_skinning_matrices)
                                      .subspan(skinning_matrices_begin, num_joints),
                                  as<uint16_t>(skinning_matrices_begin) };
}

void RenderCommandProducer::clear() noexcept
{
    m_impl->keys.clear();

    m_impl->commands.m_render_commands.clear();
    m_impl->commands.m_m_transforms.clear();
    m_impl->commands.m_vp_transforms.clear();
    m_impl->commands.m_skinning_matrices.clear();

    m_impl->render_commands_unsorted.clear();
    m_impl->m_transforms_unsorted.clear();
}

namespace { // Helpers for RenderCommandList::finalize

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

SortKey create_sort_key(const RenderCommandProducer::Impl& data,
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
    return { depth, command_fingerprint, as<uint32_t>(unsorted_command_index) };
}

} // namespace

const RenderCommandList& RenderCommandProducer::finalize(const ICamera& camera,
                                                         SortingMode sorting_mode)
{
    // Create sort key sequence
    m_impl->keys.clear();
    for (size_t i = 0; i < size(); ++i) {
        m_impl->keys.push_back(create_sort_key(*m_impl, camera, i));
    }

    MG_ASSERT(m_impl->keys.size() == size());

    // Sort sort-key sequence
    if (sorting_mode != SortingMode::unsorted) {
        auto cmp = (sorting_mode == SortingMode::far_to_near) ? cmp_draw_call<true>
                                                              : cmp_draw_call<false>;
        sort(m_impl->keys, [&](const SortKey& lhs, const SortKey& rhs) { return cmp(lhs, rhs); });
    }

    // Write out sorted render commands to m_impl->commands.
    m_impl->commands.m_render_commands.reserve(size());
    m_impl->commands.m_m_transforms.reserve(size());
    m_impl->commands.m_vp_transforms.reserve(size());

    const auto VP = camera.view_proj_matrix();

    for (const SortKey& key : m_impl->keys) {
        const RenderCommand& render_command = m_impl->render_commands_unsorted[key.index];
        const glm::mat4& M = m_impl->m_transforms_unsorted[key.index];

        if (!in_view(VP * M, render_command.bounding_sphere)) {
            continue;
        }

        m_impl->commands.m_render_commands.emplace_back(render_command);
        m_impl->commands.m_m_transforms.emplace_back(M);
        m_impl->commands.m_vp_transforms.emplace_back(VP);
    }

    return m_impl->commands;
}

size_t RenderCommandProducer::size() const noexcept
{
    return m_impl->render_commands_unsorted.size();
}

} // namespace Mg::gfx
