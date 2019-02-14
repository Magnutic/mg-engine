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

#include "mg/resources/mg_mesh_resource.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <type_traits>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <fmt/core.h>

#include "mg/core/mg_log.h"
#include "mg/core/mg_resource_cache.h"
#include "mg/utils/mg_gsl.h"

namespace Mg {

struct MeshHeader {
    uint32_t  four_cc      = 0;    // Filetype identifier
    uint32_t  version      = 0;    // Mesh format version
    uint32_t  n_vertices   = 0;    // Number of unique vertices
    uint32_t  n_indices    = 0;    // Number of vertex indices
    uint32_t  n_sub_meshes = 0;    // Number of submeshes
    glm::vec3 centre       = {};   // Mesh centre coordinate
    float     radius       = 0.0f; // Mesh radius
    glm::vec3 abb_min      = {};   // Axis-aligned bounding box
    glm::vec3 abb_max      = {};   // Axis-aligned bounding box
};

static_assert(std::is_trivially_copyable_v<MeshHeader>);

constexpr uint32_t mesh_4cc              = 0x444D474Du; // = MGMD
constexpr uint32_t k_mesh_format_version = 1;

LoadResourceResult MeshResource::load_resource_impl(const LoadResourceParams& load_params)
{
    const auto                  fname      = resource_id().str_view();
    const span<const std::byte> bytestream = load_params.resource_data();

    MeshHeader header;
    {
        MG_ASSERT(bytestream.size() >= sizeof(header));
        // Cast here is redundant but GCC-8 warns otherwise (probably a bug?)
        auto data = reinterpret_cast<const uint8_t*>(bytestream.data()); // NOLINT
        std::memcpy(&header, data, sizeof(header));
    }

    if (header.four_cc != mesh_4cc) {
        throw std::runtime_error{ fmt::format("Mesh '{}': invalid data (4CC mismatch).", fname) };
    }

    // Check file format version
    if (header.version > k_mesh_format_version) {
        g_log.write_warning(
            fmt::format("Mesh '{}': Unknown mesh format: version {:d}. "
                        "This version of Mg Engine expects version {:d}",
                        fname,
                        header.version,
                        k_mesh_format_version));
    }

    // Allocate memory for mesh data
    m_sub_meshes = Array<SubMesh>::make(header.n_sub_meshes);
    m_vertices   = Array<Vertex>::make(header.n_vertices);
    m_indices    = Array<uint_vertex_index>::make(header.n_indices);

    auto offset = sizeof(MeshHeader);

    // Load binary data into span
    auto load_to = [&](auto span, size_t stride, size_t bytes_to_read = 0) {
        using data_type = typename decltype(span)::value_type;
        static_assert(std::is_trivially_copyable_v<data_type>);
        const auto size = bytes_to_read == 0 ? sizeof(data_type) : bytes_to_read;

        for (size_t i = 0; i < span.size(); ++i) {
            if (offset + size > bytestream.size()) { return false; }

            const void* ptr = &(bytestream[offset]);
            std::memcpy(&span[i], ptr, size);
            offset += size + stride;
        }

        return true;
    };

    bool success = true;
    // Skip unused 'material_index' from submesh header
    success = success && load_to(span{ m_sub_meshes }, 4, 8);
    success = success && load_to(span{ m_vertices }, 0);
    success = success && load_to(span{ m_indices }, 0);

    if (!success) { return LoadResourceResult::data_error("Missing mesh data."); }

    m_centre = header.centre;
    m_radius = header.radius;

    MG_ASSERT(offset <= bytestream.size());

    if (!validate()) { return LoadResourceResult::data_error("Mesh validation failed."); }

    return LoadResourceResult::success();
}


bool MeshResource::validate() const
{
    auto mesh_error = [&](std::string_view what) {
        g_log.write_warning(
            fmt::format("Mesh::validate() for {}: {}", static_cast<const void*>(this), what));
    };

    // Check data
    const auto n_sub_meshes = sub_meshes().size();
    const auto n_vertices   = vertices().size();
    const auto n_indices    = narrow<size_t>(indices().size());

    // Check submeshes
    if (n_sub_meshes == 0) { mesh_error("No submeshes present."); }

    for (size_t i = 0; i < n_sub_meshes; ++i) {
        const SubMesh& sm = sub_meshes()[i];
        if (sm.begin >= n_indices || sm.begin + sm.amount > n_indices) {
            mesh_error(fmt::format("Invalid submesh at index {}", i));
            return false;
        }
    }

    // Check indices
    for (size_t i = 0; i < n_indices; ++i) {
        const uint_vertex_index& vi = indices()[i];
        if (vi >= n_vertices) {
            mesh_error(fmt::format("Index data out of bounds at index {}, was {}.", i, vi));
            return false;
        }
    }

    // Check vertices
    if (n_vertices > k_max_vertices_per_mesh) {
        mesh_error(fmt::format(
            "Too many vertices. Max is '{}', was '{}'", k_max_vertices_per_mesh, n_vertices));

        return false;
    }

    // TODO: evaluate joints when functionality is present
    return true;
}

void MeshResource::calculate_bounds()
{
    // Calculate centre position
    m_centre = {};

    for (Vertex& v : m_vertices) { m_centre += v.position; }

    m_centre /= m_vertices.size();

    // Calculate mesh radius (distance from centre to most distance vertex)
    float radius_sqr = 0.0f;

    for (Vertex& v : m_vertices) {
        auto diff  = m_centre - v.position;
        radius_sqr = std::max(radius_sqr, glm::dot(diff, diff));
    }

    m_radius = glm::sqrt(radius_sqr);
}

} // namespace Mg
