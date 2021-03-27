//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resources/mg_mesh_resource.h"

#include "mg/core/mg_log.h"
#include "mg/resource_cache/mg_resource_loading_input.h"

#include <cstdlib>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <fmt/core.h>

#include <cstring>
#include <numeric>
#include <type_traits>

namespace Mg {

namespace {

constexpr uint32_t mesh_4cc = 0x444D474Du; // = MGMD

struct HeaderCommon {
    uint32_t four_cc = 0; // Filetype identifier
    uint32_t version = 0; // Mesh format version
};

enum class Stride : size_t;
enum class ElemSize : size_t;

struct Range {
    uint32_t begin;
    uint32_t end;
};

// Load bytestream data into arrays. Array size controls number of elements to read.
// Returns number of bytes read, if successful, otherwise 0.
// Advances bytestream.
template<typename T>
size_t load_to_array(span<const std::byte> bytestream,
                     T& array_out,
                     Opt<ElemSize> elem_size = {},
                     Opt<Stride> stride = {})
{
    using TargetT = std::decay_t<decltype(array_out[0])>;
    const auto size = elem_size ? size_t(*elem_size) : sizeof(TargetT);
    const auto stride_value = stride ? size_t(*stride) : size;

    // Sanity check
    static_assert(std::is_trivially_copyable_v<TargetT>);
    MG_ASSERT(size <= sizeof(TargetT));

    size_t read_offset = 0;
    for (TargetT& elem : array_out) {
        if (read_offset + size > bytestream.size()) {
            return 0;
        }

        std::memcpy(&elem, &bytestream[read_offset], size);
        read_offset += stride_value;
    }

    return read_offset;
}

template<typename T> size_t load_to_struct(span<const std::byte> bytestream, T& out)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (bytestream.size() < sizeof(T)) {
        return 0;
    }

    std::memcpy(&out, bytestream.data(), sizeof(T));
    return sizeof(T);
}

struct LoadResult {
    Opt<MeshResource::Data> data;
    std::string error_reason;
};

template<typename T> Array<T> read_range(span<const std::byte> bytestream, Range range)
{
    const size_t elem_size = sizeof(T);
    const size_t range_length_bytes = range.end - range.begin;

    if (range.end <= range.begin || range_length_bytes % elem_size != 0 ||
        range.end > bytestream.size()) {
        return {};
    }

    const size_t num_elems = range_length_bytes / elem_size;
    auto result = Array<T>::make(num_elems);

    size_t read_offset = range.begin;
    for (T& elem : result) {
        read_offset += load_to_struct(bytestream.subspan(read_offset), elem);
    }

    MG_ASSERT(read_offset = num_elems * elem_size);

    return result;
}

std::string_view read_string(span<const std::byte> bytestream, Range range)
{
    const size_t range_length_bytes = range.end - range.begin;
    if (range.end <= range.begin || range.end > bytestream.size()) {
        return {};
    }

    return std::string_view(reinterpret_cast<const char*>(&bytestream[range.begin]),
                            range_length_bytes);
}

LoadResult load_version_1(ResourceLoadingInput& input)
{
    struct Header {
        HeaderCommon common;
        uint32_t n_vertices = 0;   // Number of unique vertices
        uint32_t n_indices = 0;    // Number of vertex indices
        uint32_t n_sub_meshes = 0; // Number of submeshes
        glm::vec3 centre = {};     // Mesh centre coordinate
        float radius = 0.0f;       // Mesh radius
        glm::vec3 abb_min = {};    // Axis-aligned bounding box
        glm::vec3 abb_max = {};    // Axis-aligned bounding box
    };

    const span<const std::byte> bytestream = input.resource_data();
    size_t pos = 0;

    Header header;
    pos += load_to_struct(bytestream, header);

    LoadResult result = {};
    result.data.emplace();
    result.data->centre = header.centre;
    result.data->radius = header.radius;

    // Allocate memory for mesh data
    result.data->sub_meshes = Array<gfx::SubMesh>::make(header.n_sub_meshes);
    result.data->vertices = Array<gfx::Vertex>::make(header.n_vertices);
    result.data->indices = Array<gfx::uint_vertex_index>::make(header.n_indices);

    pos += load_to_array(bytestream.subspan(pos),
                         result.data->sub_meshes,
                         ElemSize{ 8 },
                         Stride{ 12 });

    pos += load_to_array(bytestream.subspan(pos), result.data->vertices);

    const bool success = load_to_array(bytestream.subspan(pos), result.data->indices) > 0;

    if (!success) {
        result.data.reset();
        result.error_reason = "Missing data.";
    }

    return result;
}

LoadResult load_version_2(ResourceLoadingInput& input, std::string_view meshname)
{
    struct String {
        uint32_t begin;
        uint32_t length;
    };

    struct SubmeshRecord {
        String name;
        String material;
        uint32_t begin;
        uint32_t numIndices;
    };

    struct Header {
        HeaderCommon common;
        glm::vec3 centre;
        float radius;
        glm::vec3 abb_min;
        glm::vec3 abb_max;
        Range vertices;
        Range indices;
        Range submeshes;
        Range strings;
    };

    const span<const std::byte> bytestream = input.resource_data();

    Header header = {};
    load_to_struct(bytestream, header);

    const std::string_view strings = read_string(bytestream, header.strings);

    auto get_string = [&strings](String string) -> std::string_view {
        if (string.begin > strings.size()) {
            return {};
        }
        return strings.substr(string.begin, string.length);
    };

    LoadResult result;
    result.data.emplace();
    result.data->vertices = read_range<gfx::Vertex>(bytestream, header.vertices);
    result.data->indices = read_range<gfx::uint_vertex_index>(bytestream, header.indices);
    result.data->centre = header.centre;
    result.data->radius = header.radius;

    auto submesh_records = read_range<SubmeshRecord>(bytestream, header.submeshes);
    result.data->sub_meshes = Array<gfx::SubMesh>::make(submesh_records.size());

    for (size_t i = 0; i < submesh_records.size(); ++i) {
        const SubmeshRecord& record = submesh_records[i];
        gfx::SubMesh& submesh = result.data->sub_meshes[i];

        submesh.index_range.begin = record.begin;
        submesh.index_range.amount = record.numIndices;
        submesh.name = Identifier::from_runtime_string(get_string(record.name));
        MG_LOG_DEBUG("Loading mesh '{}': found sub-mesh '{}'", meshname, submesh.name.str_view());
        // TODO record.material
    }

    return result;
}

} // namespace

LoadResourceResult MeshResource::load_resource_impl(ResourceLoadingInput& input)
{
    span<const std::byte> bytestream = input.resource_data();
    HeaderCommon header_common;

    if (load_to_struct(bytestream, header_common) < sizeof(HeaderCommon)) {
        return LoadResourceResult::data_error("No header in file.");
    }

    if (header_common.four_cc != mesh_4cc) {
        return LoadResourceResult::data_error("Invalid data (4CC mismatch).");
    }

    LoadResult load_result;

    // Check file format version and invoke appropriate function.
    switch (header_common.version) {
    case 1:
        load_result = load_version_1(input);
        break;
    case 2:
        load_result = load_version_2(input, resource_id().str_view());
        break;
    default:
        return LoadResourceResult::data_error(
            fmt::format("Unsupported mesh version: {:d}.", header_common.version));
    }

    if (load_result.data.has_value()) {
        m_data = load_result.data.take().value();
    }
    else {
        return LoadResourceResult::data_error(load_result.error_reason);
    }

    if (!validate()) {
        return LoadResourceResult::data_error("Mesh validation failed.");
    }

    calculate_bounds();
    return LoadResourceResult::success();
}

bool MeshResource::validate() const
{
    bool status = true;
    const auto mesh_error = [&](std::string_view what) {
        log.warning(fmt::format("Mesh::validate() for {}: {}", resource_id().str_view(), what));
        status = false;
    };

    // Check data
    const auto n_sub_meshes = sub_meshes().size();
    const auto n_vertices = vertices().size();
    const auto n_indices = indices().size();

    // Check triangle-list validity
    if (n_indices % 3 != 0) {
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
        }
    }

    // Check indices
    for (size_t i = 0; i < n_indices; ++i) {
        const gfx::uint_vertex_index& vi = indices()[i];
        if (vi >= n_vertices) {
            mesh_error(fmt::format("Index data out of bounds at index {}, was {}.", i, vi));
        }
    }

    // Check vertices
    if (n_vertices > gfx::k_max_vertices_per_mesh) {
        mesh_error(fmt::format("Too many vertices. Max is '{}', was '{}'",
                               gfx::k_max_vertices_per_mesh,
                               n_vertices));
    }

    // TODO: evaluate joints when functionality is present
    return status;
}

void MeshResource::calculate_bounds() noexcept
{
    // Calculate centre position
    m_data.centre = {};

    for (const gfx::Vertex& v : m_data.vertices) {
        m_data.centre += v.position;
    }

    m_data.centre /= m_data.vertices.size();

    // Calculate mesh radius (distance from centre to most distance vertex)
    float radius_sqr = 0.0f;

    for (const gfx::Vertex& v : m_data.vertices) {
        const auto diff = m_data.centre - v.position;
        const float new_radius_sqr = glm::dot(diff, diff);
        radius_sqr = glm::max(radius_sqr, new_radius_sqr);
    }

    m_data.radius = glm::sqrt(radius_sqr);
}

} // namespace Mg
