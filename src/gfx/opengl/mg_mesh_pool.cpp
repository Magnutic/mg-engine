//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_mesh_pool.h"

#include "mg/containers/mg_flat_map.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/gfx/mg_joint.h"
#include "mg/gfx/mg_mesh_data.h"
#include "mg/resource_cache/mg_resource_cache.h"
#include "mg/resources/mg_mesh_resource.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_stl_helpers.h"

#include "../mg_mesh_internal.h"
#include "mg_glad.h"

#include <plf_colony.h>

#include <cstddef>

using namespace std::literals;

namespace Mg::gfx {

struct MeshPool::Impl {
    std::shared_ptr<ResourceCache> resource_cache;

    plf::colony<SharedBuffer> vertex_buffers;
    plf::colony<SharedBuffer> index_buffers;
    plf::colony<MeshInternal> mesh_data;

    // Used for looking up a mesh by identifier.
    FlatMap<Identifier, MeshHandle, Identifier::HashCompare> mesh_map;
};

namespace {

struct MakeMeshParams {
    // Where to put the data
    SharedBuffer* vertex_buffer;
    size_t vertex_buffer_data_offset;
    SharedBuffer* index_buffer;
    size_t index_buffer_data_offset;
    SharedBuffer* influences_buffer;
    size_t influences_buffer_data_offset;

    // Data itself
    Mesh::MeshDataView mesh_data;
    BoundingSphere bounding_sphere;
    AxisAlignedBoundingBox aabb;
};

SharedBuffer* make_vertex_buffer(MeshPool::Impl& impl, size_t size)
{
    MG_ASSERT(size > 0);
    MG_GFX_DEBUG_GROUP("MeshPool::Impl::make_vertex_buffer");

    GLuint vertex_buffer_id = 0;
    glGenBuffers(1, &vertex_buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
    glBufferData(GL_ARRAY_BUFFER, as<GLsizeiptr>(size), nullptr, GL_STATIC_DRAW);

    const auto it = impl.vertex_buffers.emplace();
    it->handle.set(vertex_buffer_id);
    return &*it;
}

SharedBuffer* make_index_buffer(MeshPool::Impl& impl, size_t size)
{
    MG_GFX_DEBUG_GROUP("MeshPool::Impl::make_index_buffer");

    GLuint index_buffer_id = 0;
    glGenBuffers(1, &index_buffer_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, as<GLsizeiptr>(size), nullptr, GL_STATIC_DRAW);

    const auto it = impl.index_buffers.emplace();
    it->handle.set(index_buffer_id);
    return &*it;
}

Opt<MeshHandle> find(const MeshPool::Impl& impl, Identifier name)
{
    const auto it = impl.mesh_map.find(name);
    if (it == impl.mesh_map.end()) {
        return nullopt;
    }
    return it->second;
}

MakeMeshParams mesh_params_from_mesh_data(MeshPool::Impl& impl, const Mesh::MeshDataView& mesh_data)
{
    MG_GFX_DEBUG_GROUP("MeshPool::Impl::make_mesh_from_mesh_data");

    const bool has_influences = !mesh_data.influences.empty();

    MakeMeshParams params = {};

    params.vertex_buffer = make_vertex_buffer(impl, mesh_data.vertices.size_bytes());
    params.index_buffer = make_index_buffer(impl, mesh_data.indices.size_bytes());
    params.influences_buffer = has_influences
                                   ? make_vertex_buffer(impl, mesh_data.influences.size_bytes())
                                   : nullptr;
    params.vertex_buffer_data_offset = 0;
    params.index_buffer_data_offset = 0;
    params.influences_buffer_data_offset = 0;
    params.mesh_data = mesh_data;
    params.bounding_sphere =
        mesh_data.bounding_sphere.value_or(calculate_mesh_bounding_sphere(mesh_data.vertices));
    params.aabb = mesh_data.aabb.value_or(calculate_mesh_bounding_box(mesh_data.vertices));

    return params;
}

void clear_mesh(MeshPool::Impl& impl, MeshInternal& mesh)
{
    MG_GFX_DEBUG_GROUP("MeshPool::Impl::clear_mesh");

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
    unref_buffer(mesh.influences_buffer, impl.vertex_buffers);
}

void setup_vertex_attribute(const VertexAttribute& attribute,
                            const int32_t stride,
                            const size_t offset)
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
void setup_vertex_attributes(std::span<const VertexAttribute> vertex_attributes)
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

// Create mesh GPU buffers inside `mesh` from the data referenced by `params`.
void make_mesh_at(MeshPool::Impl& impl,
                  MeshInternal& mesh,
                  Identifier name,
                  const MakeMeshParams& params)
{
    MG_GFX_DEBUG_GROUP("MeshPool::Impl::make_mesh_at");
    const bool has_skeletal_animation_data = !params.mesh_data.influences.empty();

    clear_mesh(impl, mesh);

    mesh.name = name;
    mesh.bounding_sphere = params.bounding_sphere;
    mesh.aabb = params.aabb;

    for (const Mesh::Submesh& sm : params.mesh_data.submeshes) {
        mesh.submeshes.push_back({ sm.index_range.begin, sm.index_range.amount });
    }

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

        setup_vertex_attributes(Mesh::vertex_attributes);
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
    if (has_skeletal_animation_data) {
        MG_ASSERT(params.influences_buffer != nullptr);

        mesh.influences_buffer = params.influences_buffer;
        ++mesh.influences_buffer->num_users;

        const auto influences_data = std::as_bytes(params.mesh_data.influences);
        const auto influences_buffer_id = mesh.influences_buffer->handle.as_gl_id();

        glBindBuffer(GL_ARRAY_BUFFER, influences_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER,
                        as<GLintptr>(params.influences_buffer_data_offset),
                        as<GLsizeiptr>(influences_data.size()),
                        influences_data.data());

        setup_vertex_attributes(Mesh::influences_attributes);
    }

    glBindVertexArray(0);
}


MeshHandle make_mesh(MeshPool::Impl& impl, Identifier name, const MakeMeshParams& params)
{
    MG_GFX_DEBUG_GROUP("MeshPool::Impl::make_mesh");

    const auto it = impl.mesh_data.emplace();

    MeshInternal& mesh = *it;

    const MeshHandle handle = make_mesh_handle(&mesh);
    const auto [map_it, inserted] = impl.mesh_map.insert({ name, handle });

    if (inserted) {
        make_mesh_at(impl, mesh, name, params);
        return handle;
    }

    throw RuntimeError{ "Creating mesh {}: a mesh by that identifier already exists.",
                        name.str_view() };
}

MeshHandle create(MeshPool::Impl& impl, const Mesh::MeshDataView& mesh_data, Identifier name)
{
    MG_GFX_DEBUG_GROUP("MeshPool::Impl::create");

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

void destroy(MeshPool::Impl& impl, MeshHandle handle)
{
    MG_GFX_DEBUG_GROUP("MeshPool::Impl::destroy");

    MeshInternal* p_mesh = &get_mesh(handle);
    const Identifier name = p_mesh->name;

    const auto it = impl.mesh_data.get_iterator(p_mesh);
    clear_mesh(impl, *p_mesh);
    impl.mesh_data.erase(it);

    // Erase from resource_id -> Mesh map.
    impl.mesh_map.erase(name);
}

} // namespace

class MeshBuffer::Impl {
public:
    Impl(MeshPool::Impl& pool_impl,
         VertexBufferSize vertex_buffer_size,
         IndexBufferSize index_buffer_size,
         InfluencesBufferSize influences_buffer_size)
        : m_pool_impl(&pool_impl)
        , m_vertex_buffer_size(static_cast<size_t>(vertex_buffer_size))
        , m_index_buffer_size(static_cast<size_t>(index_buffer_size))
        , m_influences_buffer_size(static_cast<size_t>(influences_buffer_size))
        , m_vertex_buffer(make_vertex_buffer(pool_impl, m_vertex_buffer_size))
        , m_index_buffer(make_index_buffer(pool_impl, m_index_buffer_size))
        , m_influences_buffer(m_influences_buffer_size > 0
                                  ? make_vertex_buffer(pool_impl, m_influences_buffer_size)
                                  : nullptr)
    {
        MG_ASSERT(m_vertex_buffer_size > 0);
        MG_ASSERT(m_index_buffer_size > 0);
    }

    MeshBuffer::CreateReturn create_in_buffer(const Mesh::MeshDataView& mesh_data, Identifier name)
    {
        MG_GFX_DEBUG_GROUP("MeshBuffer::Impl::create");

        if (mesh_data.vertices.size_bytes() + m_vertex_buffer_offset > m_vertex_buffer_size) {
            return { nullopt, MeshBuffer::ReturnCode::Vertex_buffer_full };
        }

        if (mesh_data.indices.size_bytes() + m_index_buffer_offset > m_index_buffer_size) {
            return { nullopt, MeshBuffer::ReturnCode::Index_buffer_full };
        }

        if (mesh_data.influences.size_bytes() + m_influences_buffer_offset >
            m_influences_buffer_size) {
            return { nullopt, MeshBuffer::ReturnCode::Influences_buffer_full };
        }

        MakeMeshParams params = mesh_params_from_mesh_data(*m_pool_impl, mesh_data);
        const MeshHandle mesh_handle = make_mesh(*m_pool_impl, name, params);

        m_vertex_buffer_offset += mesh_data.vertices.size_bytes();
        m_index_buffer_offset += mesh_data.indices.size_bytes();
        m_influences_buffer_offset += mesh_data.influences.size_bytes();

        return { mesh_handle, MeshBuffer::ReturnCode::Success };
    }

private:
    MeshPool::Impl* m_pool_impl = nullptr;

    // Data offsets; where to put next mesh's data.
    size_t m_vertex_buffer_offset = 0;     // Current offset into vertex buffer
    size_t m_index_buffer_offset = 0;      // Current offset into index buffer
    size_t m_influences_buffer_offset = 0; // Current offset into influences buffer

    size_t m_vertex_buffer_size = 0;
    size_t m_index_buffer_size = 0;
    size_t m_influences_buffer_size = 0;

    SharedBuffer* m_vertex_buffer = nullptr;
    SharedBuffer* m_index_buffer = nullptr;
    SharedBuffer* m_influences_buffer = nullptr;
};

//--------------------------------------------------------------------------------------------------

MeshBuffer::MeshBuffer(MeshPool::Impl& mesh_pool,
                       VertexBufferSize vertex_buffer_size,
                       IndexBufferSize index_buffer_size,
                       InfluencesBufferSize influences_buffer_size)
    : m_impl(mesh_pool, vertex_buffer_size, index_buffer_size, influences_buffer_size)
{}

MeshBuffer::CreateReturn MeshBuffer::create_in_buffer(const MeshResource& resource)
{
    return m_impl->create_in_buffer(resource.data_view(), resource.resource_id());
}
MeshBuffer::CreateReturn MeshBuffer::create_in_buffer(const Mesh::MeshDataView& mesh_data,
                                                      Identifier name)
{
    return m_impl->create_in_buffer(mesh_data, name);
}

//--------------------------------------------------------------------------------------------------

MeshPool::MeshPool(std::shared_ptr<ResourceCache> resource_cache)
{
    m_impl->resource_cache = std::move(resource_cache);

    // Set up callback to regenerate mesh when its originating MeshResource has changed.
    auto reload_callback = [](void* pool, const Mg::FileChangedEvent& event) {
        try {
            ResourceAccessGuard<MeshResource> access{ event.resource };
            static_cast<MeshPool*>(pool)->update(access->data_view(), access->resource_id());
        }
        catch (...) {
            Mg::log.error("Failed to reload MeshResource '{}'. Keeping old version.",
                          event.resource.resource_id().str_view());
        }
    };

    m_impl->resource_cache->set_resource_reload_callback("MeshResource"_id, reload_callback, this);
}

MeshPool::~MeshPool()
{
    MG_GFX_DEBUG_GROUP("destroy MeshPool");

    for (MeshInternal& mesh : m_impl->mesh_data) {
        clear_mesh(*m_impl, mesh);
    }

    m_impl->resource_cache->remove_resource_reload_callback("MeshResource"_id);
}

MeshHandle MeshPool::get_or_load(Identifier resource_id)
{
    MG_GFX_DEBUG_GROUP("MeshPool::create")
    if (auto opt_handle = find(resource_id); opt_handle) {
        return opt_handle.value();
    }
    auto access = m_impl->resource_cache->access_resource<MeshResource>(resource_id);
    return create(access->data_view(), access->resource_id());
}

MeshHandle MeshPool::create(const Mesh::MeshDataView& mesh_data, Identifier name)
{
    MG_GFX_DEBUG_GROUP("MeshPool::create")
    return ::Mg::gfx::create(*m_impl, mesh_data, name);
}

Opt<MeshHandle> MeshPool::find(Identifier name) const
{
    return ::Mg::gfx::find(*m_impl, name);
}

void MeshPool::destroy(MeshHandle handle)
{
    MG_GFX_DEBUG_GROUP("MeshPool::destroy")
    ::Mg::gfx::destroy(*m_impl, handle);
}

bool MeshPool::update(const Mesh::MeshDataView& mesh_data, Identifier name)
{
    MG_GFX_DEBUG_GROUP("MeshPool::update")

    Opt<MeshHandle> opt_handle = find(name);

    // If not found, then we do not have a mesh using the updated resource, so ignore.
    if (!opt_handle) {
        return false;
    }

    // Use the existing Mesh to ensure MeshHandles remain valid.
    make_mesh_at(*m_impl,
                 get_mesh(*opt_handle),
                 name,
                 mesh_params_from_mesh_data(*m_impl, mesh_data));
    log.verbose("MeshPool::update(): Updated {}", name.str_view());
    return true;
}

bool MeshPool::update(const MeshResource& mesh_res)
{
    MG_GFX_DEBUG_GROUP("MeshPool::update")
    return update(mesh_res.data_view(), mesh_res.resource_id());
}

MeshBuffer MeshPool::new_mesh_buffer(VertexBufferSize vertex_buffer_size,
                                     IndexBufferSize index_buffer_size,
                                     InfluencesBufferSize influences_buffer_size)
{
    MG_GFX_DEBUG_GROUP("MeshPool::new_mesh_buffer")
    return MeshBuffer{ *m_impl, vertex_buffer_size, index_buffer_size, influences_buffer_size };
}

} // namespace Mg::gfx
