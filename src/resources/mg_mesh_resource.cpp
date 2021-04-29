//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/resources/mg_mesh_resource.h"

#include "mg/core/mg_log.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/resource_cache/mg_resource_loading_input.h"
#include "mg/resources/mg_mesh_resource_data.h"

#include <fmt/core.h>

#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace Mg {

using namespace gfx::Mesh;

// TODO allocate all mesh data with one joint allocation

struct MeshResource::Data {
    Array<Vertex> vertices;
    Array<Index> indices;
    Array<Submesh> submeshes;
    Array<Influences> influences;
    Array<Joint> joints;
    Array<AnimationClip> animation_clips;

    glm::mat4 skeleton_root_transform = glm::mat4(1.0f);

    BoundingSphere bounding_sphere;
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

        std::memcpy(&elem, as_uint8_t_ptr(&bytestream[read_offset]), size);
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

    std::memcpy(&out, as_uint8_t_ptr(bytestream.data()), sizeof(T));
    return sizeof(T);
}

struct LoadResult {
    std::unique_ptr<MeshResource::Data> data;
    std::string error_reason;
};

template<typename T> Array<T> read_range(span<const std::byte> bytestream, FileDataRange range)
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

std::string_view read_string(span<const std::byte> bytestream, FileDataRange range)
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
        uint32_t n_vertices = 0;  // Number of unique vertices
        uint32_t n_indices = 0;   // Number of vertex indices
        uint32_t n_submeshes = 0; // Number of submeshes
        glm::vec3 centre = {};    // Mesh centre coordinate
        float radius = 0.0f;      // Mesh radius
        glm::vec3 abb_min = {};   // Axis-aligned bounding box
        glm::vec3 abb_max = {};   // Axis-aligned bounding box
    };

    const span<const std::byte> bytestream = input.resource_data();
    size_t pos = 0;

    Header header;
    pos += load_to_struct(bytestream, header);

    LoadResult result = {};
    result.data = std::make_unique<MeshResource::Data>();

    // Allocate memory for mesh data
    result.data->submeshes = Array<Submesh>::make_for_overwrite(header.n_submeshes);
    result.data->vertices = Array<Vertex>::make_for_overwrite(header.n_vertices);
    result.data->indices = Array<Index>::make_for_overwrite(header.n_indices);

    result.data->bounding_sphere.centre = header.centre;
    result.data->bounding_sphere.radius = header.radius;

    pos +=
        load_to_array(bytestream.subspan(pos), result.data->submeshes, ElemSize{ 8 }, Stride{ 12 });

    pos += load_to_array(bytestream.subspan(pos),
                         result.data->vertices,
                         ElemSize{ sizeof(Vertex) },
                         Stride{ 60 });

    const bool success = load_to_array(bytestream.subspan(pos), result.data->indices) > 0;

    if (!success) {
        result.data.reset();
        result.error_reason = "Missing data.";
    }

    return result;
}

LoadResult load_version_2(ResourceLoadingInput& input, [[maybe_unused]] std::string_view meshname)
{
    const span<const std::byte> bytestream = input.resource_data();

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

    auto submesh_records = read_range<MeshResourceData::Submesh>(bytestream, header.submeshes);
    result.data->submeshes = Array<Submesh>::make_for_overwrite(submesh_records.size());

    for (size_t i = 0; i < submesh_records.size(); ++i) {
        const MeshResourceData::Submesh& record = submesh_records[i];
        Submesh& submesh = result.data->submeshes[i];

        submesh.index_range.begin = record.begin;
        submesh.index_range.amount = record.num_indices;
        submesh.name = Identifier::from_runtime_string(get_string(record.name));
        MG_LOG_DEBUG("Loading mesh '{}': found sub-mesh '{}'", meshname, submesh.name.str_view());
        // TODO record.material
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
    return { vertices(), indices(), submeshes(), influences(), bounding_sphere() };
}

span<const Vertex> MeshResource::vertices() const noexcept
{
    return m_data ? m_data->vertices : span<const Vertex>{};
}
span<const Index> MeshResource::indices() const noexcept
{
    return m_data ? m_data->indices : span<const Index>{};
}
span<const Submesh> MeshResource::submeshes() const noexcept
{
    return m_data ? m_data->submeshes : span<const Submesh>{};
}
span<const Influences> MeshResource::influences() const noexcept
{
    return m_data ? m_data->influences : span<const Influences>{};
}
span<const Joint> MeshResource::joints() const noexcept
{
    return m_data ? m_data->joints : span<const Joint>{};
}
span<const AnimationClip> MeshResource::animation_clips() const noexcept
{
    return m_data ? m_data->animation_clips : span<const AnimationClip>{};
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

LoadResourceResult MeshResource::load_resource_impl(ResourceLoadingInput& input)
{
    span<const std::byte> bytestream = input.resource_data();
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

    if (load_result.data == nullptr) {
        return LoadResourceResult::data_error(load_result.error_reason);
    }

    m_data = std::move(load_result.data);

    if (!validate()) {
        return LoadResourceResult::data_error("Mesh validation failed.");
    }

    return LoadResourceResult::success();
}

bool MeshResource::validate() const
{
    bool status = true;
    const auto mesh_error = [&](std::string_view msg, auto&&... args) {
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
