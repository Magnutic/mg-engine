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

#include <mg/resources/mg_mesh_resource.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <mg/core/mg_log.h>
#include <mg/core/mg_resource_cache.h>
#include <mg/utils/mg_gsl.h>

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

void MeshResource::load_resource(const ResourceDataLoader& data_loader)
{
    std::vector<std::byte> bytestream(data_loader.file_size());
    data_loader.load_file(bytestream);

    MeshHeader header;

    // Cast here is redundant but GCC-8 warns otherwise (probably a bug?)
    auto data = reinterpret_cast<const uint8_t*>(bytestream.data()); // NOLINT

    std::memcpy(&header, data, sizeof(header));

    if (header.four_cc != mesh_4cc) {
        throw std::runtime_error{ format_string("Mesh '%s': invalid data (4CC mismatch).",
                                                resource_id()) };
    }

    // Check file format version
    if (header.version > k_mesh_format_version) {
        g_log.write_warning(
            "Mesh '%s': Unknown mesh format: version %d. "
            "This version of Mg Engine expects version %d",
            resource_id(),
            header.version,
            k_mesh_format_version);
    }

    m_sub_meshes = data_loader.allocator().alloc<SubMesh[]>(header.n_sub_meshes);
    m_vertices   = data_loader.allocator().alloc<Vertex[]>(header.n_vertices);
    m_indices    = data_loader.allocator().alloc<uint_vertex_index[]>(header.n_indices);

    auto offset = sizeof(MeshHeader);

    // Load binary data into span
    auto load_to = [&](auto span, size_t stride, size_t bytes_to_read = 0) {
        using data_type = typename decltype(span)::value_type;
        static_assert(std::is_trivially_copyable_v<data_type>);
        const auto size = bytes_to_read == 0 ? sizeof(data_type) : bytes_to_read;

        for (size_t i = 0; i < span.size(); ++i) {
            const void* ptr = &(bytestream[offset]);
            std::memcpy(&span[i], ptr, size);
            offset += size + stride;
        }
    };

    load_to(span{ m_sub_meshes }, 4, 8); // Skip unused 'material_index' from submesh header
    load_to(span{ m_vertices }, 0);
    load_to(span{ m_indices }, 0);

    m_centre = header.centre;
    m_radius = header.radius;

    MG_ASSERT(offset <= bytestream.size());
    MG_ASSERT(validate());
}


bool MeshResource::validate() const
{
    auto mesh_error = [&](std::string_view what) {
        g_log.write_warning("Mesh::validate() for %p: %s", this, what);
    };

    // Check data
    const auto n_sub_meshes = sub_meshes().size();
    const auto n_vertices   = vertices().size();
    const auto n_indices    = narrow<size_t>(indices().size());

    // Check submeshes
    if (n_sub_meshes == 0) {
        mesh_error("No submeshes present.");
    }

    for (size_t i = 0; i < n_sub_meshes; ++i) {
        const SubMesh& sm = sub_meshes()[i];
        if (sm.begin >= n_indices || sm.begin + sm.amount > n_indices) {
            mesh_error(format_string("Invalid submesh at index %d", i));
            return false;
        }
    }

    // Check indices
    for (size_t i = 0; i < n_indices; ++i) {
        const uint_vertex_index& vi = indices()[i];
        if (vi >= n_vertices) {
            mesh_error(format_string("Index data out of bounds at index %d, was %d.", i, vi));
            return false;
        }
    }

    // Check vertices
    if (n_vertices > k_max_vertices_per_mesh) {
        mesh_error(format_string(
            "Too many vertices. Max is '%s', was '%s'", k_max_vertices_per_mesh, n_vertices));

        return false;
    }

    // TODO: evaluate joints when functionality is present
    return true;
}

void MeshResource::calculate_bounds()
{
    // Calculate centre position
    m_centre = {};

    for (Vertex& v : m_vertices) {
        m_centre += v.position;
    }

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
