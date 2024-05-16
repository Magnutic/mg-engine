//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_mesh_pool.h"

#include "mg/containers/mg_flat_map.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/gfx/mg_animation.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_joint.h"
#include "mg/gfx/mg_material_pool.h"
#include "mg/gfx/mg_mesh.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/utils/mg_assert.h"

#include "opengl/mg_glad.h"

#include <plf_colony.h>

#include <cstddef>

using namespace std::literals;

namespace Mg {
class ResourceCache;
}

namespace Mg::gfx {

struct MeshPoolImpl {
    std::shared_ptr<ResourceCache> resource_cache;
    std::shared_ptr<MaterialPool> material_pool;

    plf::colony<SharedBuffer> vertex_buffers;
    plf::colony<SharedBuffer> index_buffers;
    plf::colony<Mesh> mesh_data;
    plf::colony<AnimationData> animation_data;

    // Used for looking up a mesh by identifier.
    FlatMap<Identifier, Mesh*, Identifier::HashCompare> mesh_map;
};

struct MakeMeshParams {
    // Where to put the data
    SharedBuffer* vertex_buffer;
    size_t vertex_buffer_data_offset;
    SharedBuffer* index_buffer;
    size_t index_buffer_data_offset;
    SharedBuffer* influences_buffer;
    size_t influences_buffer_data_offset;

    // Data itself
    mesh_data::MeshDataView mesh_data;
    BoundingSphere bounding_sphere;
    AxisAlignedBoundingBox aabb;
};

inline SharedBuffer* make_vertex_buffer(MeshPoolImpl& impl, size_t size)
{
    MG_ASSERT(size > 0);
    MG_GFX_DEBUG_GROUP("MeshPoolImpl::make_vertex_buffer");

    GLuint vertex_buffer_id = 0;
    glGenBuffers(1, &vertex_buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
    glBufferData(GL_ARRAY_BUFFER, as<GLsizeiptr>(size), nullptr, GL_STATIC_DRAW);

    const auto it = impl.vertex_buffers.emplace();
    it->handle.set(vertex_buffer_id);
    return &*it;
}

inline SharedBuffer* make_index_buffer(MeshPoolImpl& impl, size_t size)
{
    MG_GFX_DEBUG_GROUP("MeshPoolImpl::make_index_buffer");

    GLuint index_buffer_id = 0;
    glGenBuffers(1, &index_buffer_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, as<GLsizeiptr>(size), nullptr, GL_STATIC_DRAW);

    const auto it = impl.index_buffers.emplace();
    it->handle.set(index_buffer_id);
    return &*it;
}

inline Mesh* find(const MeshPoolImpl& impl, Identifier name)
{
    const auto it = impl.mesh_map.find(name);
    if (it == impl.mesh_map.end()) {
        return nullptr;
    }
    return it->second;
}

inline MakeMeshParams mesh_params_from_mesh_data(MeshPoolImpl& impl,
                                                 const mesh_data::MeshDataView& mesh_data)
{
    MG_GFX_DEBUG_GROUP("MeshPoolImpl::make_mesh_from_mesh_data");

    const bool has_animation_data = mesh_data.animation_data.has_value();

    MakeMeshParams params = {};

    params.vertex_buffer = make_vertex_buffer(impl, mesh_data.vertices.size_bytes());
    params.index_buffer = make_index_buffer(impl, mesh_data.indices.size_bytes());
    params.influences_buffer =
        has_animation_data
            ? make_vertex_buffer(impl, mesh_data.animation_data->influences.size_bytes())
            : nullptr;
    params.vertex_buffer_data_offset = 0;
    params.index_buffer_data_offset = 0;
    params.influences_buffer_data_offset = 0;
    params.mesh_data = mesh_data;
    params.bounding_sphere =
        mesh_data.bounding_sphere
            .or_else([&] { return calculate_mesh_bounding_sphere(mesh_data.vertices); })
            .value();
    params.aabb = mesh_data.aabb
                      .or_else([&] { return calculate_mesh_bounding_box(mesh_data.vertices); })
                      .value();

    return params;
}

inline void clear_mesh(MeshPoolImpl& impl, Mesh& mesh)
{
    MG_GFX_DEBUG_GROUP("MeshPoolImpl::clear_mesh");

    const auto vertex_array_id = mesh.vertex_array.as_gl_id();
    mesh.vertex_array.set(0);

    if (vertex_array_id == 0) {
        return;
    }

    MG_LOG_DEBUG("Unloading mesh '{}' (VAO {})", mesh.name.str_view(), vertex_array_id);
    glDeleteVertexArrays(1, &vertex_array_id);

    MG_ASSERT(mesh.vertex_buffer && mesh.index_buffer);

    // Un-reference shared buffer and delete, if this was the only referer.
    auto unref_buffer = [](SharedBuffer* buffer, auto& buffer_container) {
        if (buffer && --buffer->num_users == 0) {
            const auto buffer_id = buffer->handle.as_gl_id();
            glDeleteBuffers(1, &buffer_id);
            const auto it = buffer_container.get_iterator(buffer);
            buffer_container.erase(it);
        }
    };

    unref_buffer(mesh.vertex_buffer, impl.vertex_buffers);
    unref_buffer(mesh.index_buffer, impl.index_buffers);

    if (mesh.animation_data) {
        unref_buffer(mesh.animation_data->influences_buffer, impl.vertex_buffers);
        const auto it = impl.animation_data.get_iterator(mesh.animation_data);
        impl.animation_data.erase(it);
    }
}

inline void
setup_vertex_attribute(const VertexAttribute& attribute, const int32_t stride, const size_t offset)
{
    const bool is_normalized = attribute.int_value_meaning == IntValueMeaning::Normalize;

    glVertexAttribPointer(attribute.binding_location,
                          as<GLint>(attribute.num_elements),
                          static_cast<GLuint>(attribute.type),
                          static_cast<GLboolean>(is_normalized),
                          stride,
                          reinterpret_cast<GLvoid*>(offset)); // NOLINT

    glEnableVertexAttribArray(attribute.binding_location);
}

// Set up vertex attributes (how OpenGL is to interpret the vertex impl).
inline void setup_vertex_attributes(std::span<const VertexAttribute> vertex_attributes)
{
    int32_t stride = 0;
    for (const VertexAttribute& vertex_attribute : vertex_attributes) {
        stride += as<int>(vertex_attribute.size);
    }

    uintptr_t offset = 0;

    for (const VertexAttribute& vertex_attribute : vertex_attributes) {
        setup_vertex_attribute(vertex_attribute, stride, offset);
        offset += vertex_attribute.size;
    }
}

inline AnimationData* make_animation_data(MeshPoolImpl& impl,
                                          const Identifier name,
                                          const mesh_data::AnimationDataView& data,
                                          SharedBuffer* influences_buffer)
{
    AnimationData& result = *impl.animation_data.emplace();
    result.clips.resize(data.animation_clips.size());
    std::ranges::copy(data.animation_clips, result.clips.begin());
    result.influences_buffer = influences_buffer;
    ++influences_buffer->num_users;
    result.skeleton = Skeleton{ name, data.skeleton_root_transform, data.joints.size() };
    std::ranges::copy(data.joints, result.skeleton.joints().begin());
    return &result;
}

// Create mesh GPU buffers inside `mesh` from the data referenced by `params`.
inline void
make_mesh_at(MeshPoolImpl& impl, Mesh& mesh, Identifier name, const MakeMeshParams& params)
{
    MG_GFX_DEBUG_GROUP("MeshPoolImpl::make_mesh_at");
    clear_mesh(impl, mesh);

    mesh.name = name;
    mesh.bounding_sphere = params.bounding_sphere;
    mesh.aabb = params.aabb;

    mesh.submeshes.resize(params.mesh_data.submeshes.size());
    std::ranges::copy(params.mesh_data.submeshes, mesh.submeshes.begin());

    GLuint vertex_array_id = 0;
    glGenVertexArrays(1, &vertex_array_id);
    mesh.vertex_array.set(vertex_array_id);

    glBindVertexArray(vertex_array_id);

    { // Upload vertex data to GPU
        mesh.vertex_buffer = params.vertex_buffer;
        ++mesh.vertex_buffer->num_users;

        const auto vertex_data = std::as_bytes(params.mesh_data.vertices);
        const auto vertex_buffer_id = mesh.vertex_buffer->handle.as_gl_id();

        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER,
                        as<GLintptr>(params.vertex_buffer_data_offset),
                        as<GLsizeiptr>(vertex_data.size()),
                        vertex_data.data());

        setup_vertex_attributes(mesh_data::vertex_attributes);
    }

    { // Upload index data to GPU
        mesh.index_buffer = params.index_buffer;
        ++mesh.index_buffer->num_users;

        const auto index_buffer_data = std::as_bytes(params.mesh_data.indices);
        const auto index_buffer_id = mesh.index_buffer->handle.as_gl_id();

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                        as<GLintptr>(params.index_buffer_data_offset),
                        as<GLsizeiptr>(index_buffer_data.size()),
                        index_buffer_data.data());
    }


    // For meshes with skeletal animation, we must also upload the joint influences for each vertex.
    if (params.mesh_data.animation_data.has_value()) {
        MG_ASSERT(params.influences_buffer != nullptr);

        const auto skeleton_name =
            Identifier::from_runtime_string(std::string{ name.str_view() } + "_skeleton");

        mesh.animation_data = make_animation_data(impl,
                                                  skeleton_name,
                                                  params.mesh_data.animation_data.value(),
                                                  params.influences_buffer);

        const auto influences_data = std::as_bytes(params.mesh_data.animation_data->influences);
        const auto influences_buffer_id = mesh.animation_data->influences_buffer->handle.as_gl_id();

        glBindBuffer(GL_ARRAY_BUFFER, influences_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER,
                        as<GLintptr>(params.influences_buffer_data_offset),
                        as<GLsizeiptr>(influences_data.size()),
                        influences_data.data());

        setup_vertex_attributes(mesh_data::influences_attributes);
    }

    glBindVertexArray(0);
}


inline const Mesh* make_mesh(MeshPoolImpl& impl, Identifier name, const MakeMeshParams& params)
{
    MG_GFX_DEBUG_GROUP("MeshPoolImpl::make_mesh");

    Mesh& mesh = *impl.mesh_data.emplace();

    const auto [map_it, inserted] = impl.mesh_map.insert({ name, &mesh });

    if (inserted) {
        make_mesh_at(impl, mesh, name, params);
        return &mesh;
    }

    throw RuntimeError{ "Creating mesh {}: a mesh by that identifier already exists.",
                        name.str_view() };
}

inline const Mesh*
create(MeshPoolImpl& impl, const mesh_data::MeshDataView& mesh_data, Identifier name)
{
    MG_GFX_DEBUG_GROUP("MeshPoolImpl::create");

    // Check precondition
    const bool has_vertices = !mesh_data.vertices.empty();
    const bool has_indices = !mesh_data.indices.empty();

    if (!has_vertices || !has_indices) {
        const auto problem = (!has_vertices) ? "no vertex data"sv : "no index data"sv;
        throw RuntimeError{ "MeshPool: cannot create mesh '{}': {}.", name.str_view(), problem };
    }

    const MakeMeshParams params = mesh_params_from_mesh_data(impl, mesh_data);
    return make_mesh(impl, name, params);
}

inline void destroy(MeshPoolImpl& impl, const Mesh* handle)
{
    MG_GFX_DEBUG_GROUP("MeshPoolImpl::destroy");
    MG_ASSERT(handle != nullptr);

    auto mesh_map_it = impl.mesh_map.find(handle->name);
    clear_mesh(impl, *mesh_map_it->second);

    // Erase from resource_id -> Mesh map.
    impl.mesh_map.erase(mesh_map_it);

    const auto mesh_data_it = impl.mesh_data.get_iterator(handle);
    impl.mesh_data.erase(mesh_data_it);
}

} // namespace Mg::gfx
