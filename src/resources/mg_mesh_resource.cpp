//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resources/mg_mesh_resource.h"

#include "mg/core/mg_log.h"
#include "mg/resource_cache/mg_resource_loading_input.h"
#include "mg/utils/mg_gsl.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <fmt/core.h>

#include <cstring>
#include <type_traits>

namespace Mg {

struct MeshHeader {
    uint32_t four_cc = 0;      // Filetype identifier
    uint32_t version = 0;      // Mesh format version
    uint32_t n_vertices = 0;   // Number of unique vertices
    uint32_t n_indices = 0;    // Number of vertex indices
    uint32_t n_sub_meshes = 0; // Number of submeshes
    glm::vec3 centre = {};     // Mesh centre coordinate
    float radius = 0.0f;       // Mesh radius
    glm::vec3 abb_min = {};    // Axis-aligned bounding box
    glm::vec3 abb_max = {};    // Axis-aligned bounding box
};

static_assert(std::is_trivially_copyable_v<MeshHeader>);

constexpr uint32_t mesh_4cc = 0x444D474Du; // = MGMD
constexpr uint32_t k_mesh_format_version = 1;

LoadResourceResult MeshResource::load_resource_impl(const ResourceLoadingInput& input)
{
    const std::string_view fname = resource_id().str_view();
    span<const std::byte> bytestream = input.resource_data();

    MeshHeader header;
    {
        MG_ASSERT(bytestream.size() >= sizeof(header));
        std::memcpy(&header, bytestream.data(), sizeof(header));
        bytestream = bytestream.subspan(sizeof(header));
    }

    m_centre = header.centre;
    m_radius = header.radius;

    if (header.four_cc != mesh_4cc) {
        return LoadResourceResult::data_error(fmt::format("Invalid data (4CC mismatch).", fname));
    }

    // Check file format version
    if (header.version > k_mesh_format_version) {
        g_log.write_warning(
            fmt::format("Mesh '{}': Unknown mesh format: version {:d}. Expected version {:d}",
                        fname,
                        header.version,
                        k_mesh_format_version));
    }

    // Allocate memory for mesh data
    m_sub_meshes = Array<gfx::SubMesh>::make(header.n_sub_meshes);
    m_vertices = Array<gfx::Vertex>::make(header.n_vertices);
    m_indices = Array<gfx::uint_vertex_index>::make(header.n_indices);

    enum class Stride : size_t;
    enum class ElemSize : size_t;
    // Load bytestream data into arrays. Array size controls number of elements to read.
    auto load_to_array =
        [&bytestream](auto& array_out, Opt<ElemSize> elem_size = {}, Opt<Stride> stride = {}) {
            using TargetT = std::decay_t<decltype(array_out[0])>;
            const auto size = elem_size ? size_t(*elem_size) : sizeof(TargetT);
            const auto stride_value = stride ? size_t(*stride) : size;

            // Sanity check
            static_assert(std::is_trivially_copyable_v<TargetT>);
            MG_ASSERT(size <= sizeof(TargetT));

            for (size_t i = 0; i < array_out.size(); ++i) {
                if (size > bytestream.size()) {
                    return false;
                }
                std::memcpy(&array_out[i], bytestream.data(), size);
                bytestream = bytestream.subspan(stride_value);
            }

            return true;
        };

    bool success = true;
    // Skip unused 'material_index' from submesh header
    success = success && load_to_array(m_sub_meshes, ElemSize{ 8 }, Stride{ 12 });
    success = success && load_to_array(m_vertices);
    success = success && load_to_array(m_indices);

    if (!success) {
        return LoadResourceResult::data_error("Missing mesh data.");
    }
    if (!validate()) {
        return LoadResourceResult::data_error("Mesh validation failed.");
    }

    return LoadResourceResult::success();
}


bool MeshResource::validate() const
{
    const auto mesh_error = [&](std::string_view what) {
        g_log.write_warning(
            fmt::format("Mesh::validate() for {}: {}", static_cast<const void*>(this), what));
    };

    // Check data
    const auto n_sub_meshes = sub_meshes().size();
    const auto n_vertices = vertices().size();
    const auto n_indices = narrow<size_t>(indices().size());

    // Check triangle-list validity
    if (n_sub_meshes % 3 != 0) {
        mesh_error("Mesh is not a triangle-list (number of indices not divisible by three).");
    }

    // Check submeshes
    if (n_sub_meshes == 0) {
        mesh_error("No submeshes present.");
    }

    for (size_t i = 0; i < n_sub_meshes; ++i) {
        const gfx::SubMesh& sm = sub_meshes()[i];
        const auto [index_begin, index_amount] = sm.index_range;
        if (index_begin >= n_indices || index_begin + index_amount > n_indices) {
            mesh_error(fmt::format("Invalid submesh at index {}", i));
            return false;
        }
    }

    // Check indices
    for (size_t i = 0; i < n_indices; ++i) {
        const gfx::uint_vertex_index& vi = indices()[i];
        if (vi >= n_vertices) {
            mesh_error(fmt::format("Index data out of bounds at index {}, was {}.", i, vi));
            return false;
        }
    }

    // Check vertices
    if (n_vertices > gfx::k_max_vertices_per_mesh) {
        mesh_error(fmt::format("Too many vertices. Max is '{}', was '{}'",
                               gfx::k_max_vertices_per_mesh,
                               n_vertices));

        return false;
    }

    // TODO: evaluate joints when functionality is present
    return true;
}

void MeshResource::calculate_bounds() noexcept
{
    // Calculate centre position
    m_centre = {};

    for (const gfx::Vertex& v : m_vertices) {
        m_centre += v.position;
    }

    m_centre /= m_vertices.size();

    // Calculate mesh radius (distance from centre to most distance vertex)
    float radius_sqr = 0.0f;

    for (const gfx::Vertex& v : m_vertices) {
        const auto diff = m_centre - v.position;
        const float new_radius_sqr = glm::dot(diff, diff);
        radius_sqr = glm::max(radius_sqr, new_radius_sqr);
    }

    m_radius = glm::sqrt(radius_sqr);
}

} // namespace Mg
