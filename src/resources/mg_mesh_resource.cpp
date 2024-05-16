//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resources/mg_mesh_resource.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/resource_cache/mg_resource_loading_input.h"
#include "mg/resources/mg_mesh_resource_data.h"
#include "mg/utils/mg_stl_helpers.h"

#include <fmt/core.h>

#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace Mg {

using namespace gfx::mesh_data;

struct MeshResource::Data {
    Array<Vertex> vertices;
    Array<Index> indices;
    Array<Submesh> submeshes;
    Array<Influences> influences;
    Array<Joint> joints;
    Array<AnimationClip> animation_clips;

    glm::mat4 skeleton_root_transform = glm::mat4(1.0f);

    BoundingSphere bounding_sphere;
    AxisAlignedBoundingBox axis_aligned_bounding_box;
};

namespace {

struct HeaderCommon {
    uint32_t four_cc = 0; // Filetype identifier
    uint32_t version = 0; // Mesh format version
};

enum class Stride : size_t;
enum class ElemSize : size_t;

// Cast const std::byte* to const std::uint8_t*, since GCC will (incorrectly, as far as I can tell)
// warn on memcpy from std::byte*.
const uint8_t* as_uint8_t_ptr(const std::byte* byte_ptr)
{
    return reinterpret_cast<const uint8_t*>(byte_ptr); // NOLINT
}

template<typename T> size_t load_to_struct(std::span<const std::byte> bytestream, T& out)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (bytestream.size() < sizeof(T)) {
        return 0;
    }

    std::memcpy(&out, as_uint8_t_ptr(bytestream.data()), sizeof(T));
    return sizeof(T);
}

struct LoadResult {
    std::unique_ptr<MeshResource::Data> data;
    std::string error_reason;
};

template<typename T> Array<T> read_range(std::span<const std::byte> bytestream, FileDataRange range)
{
    const size_t elem_size = sizeof(T);
    const size_t range_length_bytes = range.end - range.begin;

    if (range.end <= range.begin || range_length_bytes % elem_size != 0 ||
        range.end > bytestream.size()) {
        return {};
    }

    const size_t num_elems = range_length_bytes / elem_size;
    auto result = Array<T>::make_for_overwrite(num_elems);

    size_t read_offset = range.begin;
    for (T& elem : result) {
        read_offset += load_to_struct(bytestream.subspan(read_offset), elem);
    }

    MG_ASSERT(read_offset = num_elems * elem_size);

    return result;
}

std::string_view read_string(std::span<const std::byte> bytestream, FileDataRange range)
{
    const size_t range_length_bytes = range.end - range.begin;
    if (range.end <= range.begin || range.end > bytestream.size()) {
        return {};
    }

    return { reinterpret_cast<const char*>(&bytestream[range.begin]), range_length_bytes };
}

LoadResult load_version_2(ResourceLoadingInput& input, [[maybe_unused]] std::string_view meshname)
{
    const std::span<const std::byte> bytestream = input.resource_data();

    MeshResourceData::Header header = {};
    load_to_struct(bytestream, header);

    const std::string_view strings = read_string(bytestream, header.strings);

    auto get_string = [&strings](MeshResourceData::StringRange string) -> std::string_view {
        if (string.begin > strings.size()) {
            return {};
        }
        return strings.substr(string.begin, string.length);
    };

    LoadResult result;
    result.data = std::make_unique<MeshResource::Data>();
    result.data->vertices = read_range<Vertex>(bytestream, header.vertices);
    result.data->indices = read_range<Index>(bytestream, header.indices);
    result.data->influences = read_range<Influences>(bytestream, header.influences);
    result.data->bounding_sphere.centre = header.centre;
    result.data->bounding_sphere.radius = header.radius;
    result.data->skeleton_root_transform = header.skeleton_root_transform;

    MG_LOG_DEBUG("Number of vertices: {}, indices: {}, triangles: {}",
                 result.data->vertices.size(),
                 result.data->indices.size(),
                 result.data->indices.size() / 3u);

    auto submesh_records = read_range<MeshResourceData::Submesh>(bytestream, header.submeshes);
    result.data->submeshes = Array<Submesh>::make_for_overwrite(submesh_records.size());

    for (size_t i = 0; i < submesh_records.size(); ++i) {
        const MeshResourceData::Submesh& record = submesh_records[i];
        Submesh& submesh = result.data->submeshes[i];

        submesh.index_range.begin = record.begin;
        submesh.index_range.amount = record.num_indices;
        submesh.name = Identifier::from_runtime_string(get_string(record.name));
        submesh.material_binding_id = Identifier::from_runtime_string(get_string(record.material));
        MG_LOG_DEBUG("Loading mesh '{}': found sub-mesh '{}'", meshname, submesh.name.str_view());
        MG_LOG_DEBUG("Submesh range {}:{}", record.begin, record.begin + record.num_indices);
        MG_LOG_DEBUG("Submesh material binding: {}", get_string(record.material));
        MG_ASSERT_DEBUG(record.begin + record.num_indices <= result.data->indices.size());
    }

    auto joint_records = read_range<MeshResourceData::Joint>(bytestream, header.joints);
    result.data->joints = Array<Joint>::make_for_overwrite(joint_records.size());

    for (size_t i = 0; i < joint_records.size(); ++i) {
        const MeshResourceData::Joint record = joint_records[i];
        Joint& joint = result.data->joints[i];

        joint.children = record.children;
        joint.inverse_bind_matrix = record.inverse_bind_matrix;
        joint.name = Identifier::from_runtime_string(get_string(record.name));
    }

    auto clip_records = read_range<MeshResourceData::AnimationClip>(bytestream, header.animations);
    result.data->animation_clips = Array<AnimationClip>::make_for_overwrite(clip_records.size());

    for (size_t i = 0; i < clip_records.size(); ++i) {
        const MeshResourceData::AnimationClip& clip_record = clip_records[i];
        AnimationClip& clip = result.data->animation_clips[i];
        clip.name = Identifier::from_runtime_string(get_string(clip_record.name));
        clip.duration_seconds = narrow_cast<float>(clip_record.duration);

        auto channel_records = read_range<MeshResourceData::AnimationChannel>(bytestream,
                                                                              clip_record.channels);

        clip.channels = Array<AnimationChannel>::make_for_overwrite(channel_records.size());

        for (size_t u = 0; u < channel_records.size(); ++u) {
            AnimationChannel& channel = clip.channels[u];
            MeshResourceData::AnimationChannel& channel_record = channel_records[u];
            channel.position_keys = read_range<PositionKey>(bytestream,
                                                            channel_record.position_keys);
            channel.rotation_keys = read_range<RotationKey>(bytestream,
                                                            channel_record.rotation_keys);
            channel.scale_keys = read_range<ScaleKey>(bytestream, channel_record.scale_keys);
        }
    }

    return result;
}

} // namespace

MeshResource::MeshResource(Identifier id) : BaseResource(id) {}

MeshResource::~MeshResource() = default;

MeshDataView MeshResource::data_view() const noexcept
{
    MeshDataView result{
        .vertices = vertices(),
        .indices = indices(),
        .submeshes = submeshes(),
        .animation_data = nullopt,
        .bounding_sphere = bounding_sphere(),
        .aabb = axis_aligned_bounding_box(),
    };

    if (!influences().empty() && !joints().empty()) {
        result.animation_data.emplace();
        result.animation_data->influences = influences();
        result.animation_data->joints = joints();
        result.animation_data->animation_clips = animation_clips();
        result.animation_data->skeleton_root_transform = skeleton_root_transform();
    }

    return result;
}

std::span<const Vertex> MeshResource::vertices() const noexcept
{
    return m_data ? m_data->vertices : std::span<const Vertex>{};
}
std::span<const Index> MeshResource::indices() const noexcept
{
    return m_data ? m_data->indices : std::span<const Index>{};
}
std::span<const Submesh> MeshResource::submeshes() const noexcept
{
    return m_data ? m_data->submeshes : std::span<const Submesh>{};
}
std::span<const Influences> MeshResource::influences() const noexcept
{
    return m_data ? m_data->influences : std::span<const Influences>{};
}
std::span<const Joint> MeshResource::joints() const noexcept
{
    return m_data ? m_data->joints : std::span<const Joint>{};
}
std::span<const AnimationClip> MeshResource::animation_clips() const noexcept
{
    return m_data ? m_data->animation_clips : std::span<const AnimationClip>{};
}

Opt<size_t> MeshResource::get_submesh_index(const Identifier& submesh_name) const noexcept
{
    if (!m_data) {
        return nullopt;
    }

    auto has_matching_name = [&](const Submesh& submesh) { return submesh.name == submesh_name; };
    const auto [found, index] = index_where(m_data->submeshes, has_matching_name);

    return found ? Opt<size_t>(index) : nullopt;
}

const glm::mat4& MeshResource::skeleton_root_transform() const noexcept
{
    static const auto identity = glm::mat4(1.0f);
    return m_data ? m_data->skeleton_root_transform : identity;
}

BoundingSphere MeshResource::bounding_sphere() const noexcept
{
    return m_data ? m_data->bounding_sphere : BoundingSphere{};
}
AxisAlignedBoundingBox MeshResource::axis_aligned_bounding_box() const noexcept
{
    return m_data ? m_data->axis_aligned_bounding_box : AxisAlignedBoundingBox{};
}

LoadResourceResult MeshResource::load_resource_impl(ResourceLoadingInput& input)
{
    std::span<const std::byte> bytestream = input.resource_data();
    HeaderCommon header_common;

    if (load_to_struct(bytestream, header_common) < sizeof(HeaderCommon)) {
        return LoadResourceResult::data_error("No header in file.");
    }

    if (header_common.four_cc != MeshResourceData::fourcc) {
        return LoadResourceResult::data_error("Invalid data (4CC mismatch).");
    }

    LoadResult load_result;

    // Check file format version and invoke appropriate function.
    switch (header_common.version) {
    // version 1 has been removed.
    case 2:
        load_result = load_version_2(input, resource_id().str_view());
        break;
    default:
        return LoadResourceResult::data_error(
            fmt::format("Unsupported mesh version: {:d}.", header_common.version));
    }

    if (load_result.data == nullptr) {
        return LoadResourceResult::data_error(load_result.error_reason);
    }

    m_data = std::move(load_result.data);

    // TODO: store bounding box in mesh file.
    m_data->axis_aligned_bounding_box = calculate_mesh_bounding_box(m_data->vertices);

    if (!validate()) {
        return LoadResourceResult::data_error("Mesh validation failed.");
    }

    return LoadResourceResult::success();
}

bool MeshResource::validate() const
{
    bool status = true;
    const auto mesh_error = [&]<typename... Ts>(fmt::format_string<Ts...> msg, Ts&&... args) {
        log.warning("MeshResource::validate() for {}: {}",
                    resource_id().str_view(),
                    fmt::format(msg, std::forward<decltype(args)>(args)...));
        status = false;
    };

    // Check data
    const auto n_submeshes = submeshes().size();
    const auto n_vertices = vertices().size();
    const auto n_indices = indices().size();
    const auto n_influences = influences().size();
    const auto n_joints = joints().size();

    // Check triangle-list validity
    if (n_indices % 3 != 0) {
        mesh_error("Mesh is not a triangle-list (number of indices not divisible by three).");
    }

    // Check submeshes
    if (n_submeshes == 0) {
        mesh_error("No submeshes present.");
    }

    for (size_t i = 0; i < n_submeshes; ++i) {
        const Submesh& sm = submeshes()[i];
        const auto [index_begin, index_amount] = sm.index_range;
        if (index_begin >= n_indices || index_begin + index_amount > n_indices) {
            mesh_error("Invalid submesh at index {}", i);
        }
    }

    // Check indices
    for (size_t i = 0; i < n_indices; ++i) {
        const Index& vi = indices()[i];
        if (vi >= n_vertices) {
            mesh_error("Index data out of bounds at index {}, was {}.", i, vi);
        }
    }

    // Check vertices
    if (n_vertices > max_vertices_per_mesh) {
        mesh_error("Too many vertices. Max is '{}', was '{}'.", max_vertices_per_mesh, n_vertices);
    }

    // Check joints
    for (JointId ji = 0; ji < n_joints; ++ji) {
        const Joint& joint = joints()[ji];

        if (joint.name.str_view().empty()) {
            mesh_error("Joint {} has no name.", ji);
        }

        for (const JointId child_id : joint.children) {
            if (child_id != joint_id_none && child_id >= joints().size()) {
                mesh_error(
                    "In children of joint '{}' (JointId {}): "
                    "JointId out of bounds: was {}, max is {}.",
                    joint.name.str_view(),
                    ji,
                    child_id,
                    joints().size());
            }
        }
    }

    // Check joint influences
    {
        if (n_influences > 0 && n_influences != n_vertices) {
            mesh_error("Number of joint influences ({}) does not match number of vertices ({}).",
                       n_influences,
                       n_vertices);
        }

        for (size_t ii = 0; ii < n_influences; ++ii) {
            const Influences& vertex_influences = influences()[ii];
            for (const JointId id : vertex_influences.ids) {
                if (id != joint_id_none && id >= joints().size()) {
                    mesh_error("In influences index {}: JointId out of bounds; was {}, max is {}.",
                               ii,
                               id,
                               n_joints);
                }
            }
        }
    }

    // Check animation clips
    {
        for (const AnimationClip& clip : animation_clips()) {
            if (clip.name.str_view().empty()) {
                mesh_error("Animation clip has no name.");
            }

            if (clip.channels.size() != n_joints) {
                mesh_error(
                    "Animation clip '{}' does not contain one channel per joint; "
                    "had {} channels, expected {}.",
                    clip.name.str_view(),
                    clip.channels.size(),
                    n_joints);
            }
        }
    }

    return status;
}

} // namespace Mg
