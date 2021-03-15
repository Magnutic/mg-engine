//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_mesh_repository.h"

#include "mg/containers/mg_flat_map.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/gfx/mg_gfx_debug_group.h"
#include "mg/gfx/mg_gfx_object_handles.h"
#include "mg/resources/mg_mesh_resource.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_stl_helpers.h"

#include "../mg_mesh.h"
#include "mg_glad.h"

#include <plf_colony.h>

#include <glm/glm.hpp>

#include <fmt/core.h>

#include <numeric>

namespace Mg::gfx {

namespace {

MeshDataView::BoundingInfo calculate_mesh_bounding_info(span<const Vertex> vertices,
                                                        span<const uint_vertex_index> indices)
{
    MG_ASSERT(!vertices.empty() && !indices.empty());
    const auto num_indices = static_cast<float>(indices.size());

    auto add_position = [&](glm::vec3 vec, uint_vertex_index vert_idx) {
        return vec + vertices[vert_idx].position;
    };
    const glm::vec3 centre =
        std::accumulate(indices.begin(), indices.end(), glm::vec3{ 0.0f }, add_position) /
        num_indices;

    auto cmp_vertex = [](const Vertex& lhs, const Vertex& rhs) {
        return glm::all(glm::lessThan(lhs.position, rhs.position));
    };
    const auto [min_it, max_it] = std::minmax_element(vertices.begin(), vertices.end(), cmp_vertex);

    const float radius = std::max(glm::distance(centre, min_it->position),
                                  glm::distance(centre, max_it->position));

    return { centre, radius };
}

} // namespace

//--------------------------------------------------------------------------------------------------

class MeshRepositoryImpl {
public:
    MeshRepositoryImpl() = default;

    ~MeshRepositoryImpl()
    {
        MG_GFX_DEBUG_GROUP("destroy MeshRepository");

        for (Mesh& mesh : m_mesh_data) {
            _clear_mesh(mesh);
        }
    }

    MG_MAKE_NON_COPYABLE(MeshRepositoryImpl);
    MG_MAKE_NON_MOVABLE(MeshRepositoryImpl);

    MeshHandle create(Identifier name, const MeshDataView& data);

    bool update(Identifier name, const MeshDataView& new_data);

    void destroy(MeshHandle handle);

    Opt<MeshHandle> get(Identifier name) const
    {
        const auto it = m_mesh_map.find(name);
        if (it == m_mesh_map.end()) {
            return nullopt;
        }
        return it->second;
    }

private:
    friend class MeshBufferImpl;

    SharedBuffer* _make_vertex_buffer(size_t size);
    SharedBuffer* _make_index_buffer(size_t size);

    struct MakeMeshParams {
        // Where to put the data
        SharedBuffer* vertex_buffer;
        size_t vbo_data_offset;
        SharedBuffer* index_buffer;
        size_t ibo_data_offset;

        // Data itself
        MeshDataView mesh_data;
        glm::vec3 centre;
        float radius;
    };

    MakeMeshParams _mesh_params_from_mesh_data(const MeshDataView& data)
    {
        MG_GFX_DEBUG_GROUP("MeshRepositoryImpl::_make_mesh_from_mesh_data");

        const auto [centre, radius] =
            data.bounding_info.value_or(calculate_mesh_bounding_info(data.vertices, data.indices));

        MakeMeshParams params = {};
        params.vertex_buffer = _make_vertex_buffer(data.vertices.size_bytes());
        params.index_buffer = _make_index_buffer(data.indices.size_bytes());
        params.vbo_data_offset = 0;
        params.ibo_data_offset = 0;
        params.centre = centre;
        params.radius = radius;
        params.mesh_data = data;

        return params;
    }

    void _make_mesh_at(Mesh& mesh, Identifier name, const MakeMeshParams& params);

    MeshHandle _make_mesh(Identifier name, const MakeMeshParams& params)
    {
        MG_GFX_DEBUG_GROUP("MeshRepositoryImpl::_make_mesh");

        const auto it = m_mesh_data.emplace();

        Mesh& mesh = *it;

        const MeshHandle handle = _ptr_to_handle(&mesh);
        const auto [map_it, inserted] = m_mesh_map.insert({ name, handle });

        if (inserted) {
            _make_mesh_at(mesh, name, params);
            return handle;
        }

        const auto fmt_string = "Creating mesh {}: a mesh by that identifier already exists.";
        const auto error_msg = fmt::format(fmt_string, name.str_view());
        log.error(error_msg);
        throw RuntimeError{};
    }

    static MeshHandle _ptr_to_handle(Mesh* ptr)
    {
        uintptr_t handle_value;
        std::memcpy(&handle_value, &ptr, sizeof(uintptr_t));
        return MeshHandle{ handle_value };
    }

    void _clear_mesh(Mesh& mesh);

    plf::colony<SharedBuffer> m_vertex_buffers;
    plf::colony<SharedBuffer> m_index_buffers;
    plf::colony<Mesh> m_mesh_data;

    // Used for looking up a mesh by identifier.
    FlatMap<Identifier, MeshHandle, Identifier::HashCompare> m_mesh_map;
};

MeshHandle MeshRepositoryImpl::create(Identifier name, const MeshDataView& mesh_data)
{
    MG_GFX_DEBUG_GROUP("MeshRepositoryImpl::create");

    // Check precondition
    const bool has_vertices = !mesh_data.vertices.empty();
    const bool has_indices = !mesh_data.indices.empty();

    if (!has_vertices || !has_indices) {
        const std::string problem = !has_vertices ? "no vertex data" : "no index data";
        const std::string msg =
            fmt::format("MeshRepository: cannot create mesh '{}': {}.", name.str_view(), problem);
        throw std::runtime_error(msg);
    }

    const MakeMeshParams params = _mesh_params_from_mesh_data(mesh_data);
    return _make_mesh(name, params);
}

bool MeshRepositoryImpl::update(Identifier name, const MeshDataView& data)
{
    MG_GFX_DEBUG_GROUP("MeshRepositoryImpl::update");

    Opt<MeshHandle> opt_handle = get(name);

    // If not found, then we do not have a mesh using the updated resource, so ignore.
    if (!opt_handle) {
        return false;
    }

    // Use the existing Mesh to ensure MeshHandles remain valid.
    _make_mesh_at(get_mesh(*opt_handle), name, _mesh_params_from_mesh_data(data));
    log.verbose(fmt::format("MeshRepository::update(): Updated {}", name.str_view()));
    return true;
}

void MeshRepositoryImpl::destroy(MeshHandle handle)
{
    MG_GFX_DEBUG_GROUP("MeshRepositoryImpl::destroy");

    Mesh* p_mesh = &get_mesh(handle);
    const Identifier name = p_mesh->name;

    const auto it = m_mesh_data.get_iterator_from_pointer(p_mesh);
    _clear_mesh(*p_mesh);
    m_mesh_data.erase(it);

    // Erase from resource_id -> Mesh map.
    m_mesh_map.erase(name);
}

SharedBuffer* MeshRepositoryImpl::_make_vertex_buffer(size_t size)
{
    MG_GFX_DEBUG_GROUP("MeshRepositoryImpl::_make_vertex_buffer");

    GLuint vbo_id = 0;
    glGenBuffers(1, &vbo_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
    glBufferData(GL_ARRAY_BUFFER, narrow<GLsizeiptr>(size), nullptr, GL_STATIC_DRAW);

    const auto it = m_vertex_buffers.emplace();
    it->handle.set(vbo_id);
    return &*it;
}

SharedBuffer* MeshRepositoryImpl::_make_index_buffer(size_t size)
{
    MG_GFX_DEBUG_GROUP("MeshRepositoryImpl::_make_index_buffer");

    GLuint ibo_id = 0;
    glGenBuffers(1, &ibo_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, narrow<GLsizeiptr>(size), nullptr, GL_STATIC_DRAW);

    const auto it = m_index_buffers.emplace();
    it->handle.set(ibo_id);
    return &*it;
}

// Create mesh from MeshResouce, storing data in the given vertex and index buffers
void MeshRepositoryImpl::_make_mesh_at(Mesh& mesh, Identifier name, const MakeMeshParams& params)
{
    MG_GFX_DEBUG_GROUP("MeshRepositoryImpl::_make_mesh_at");

    _clear_mesh(mesh);

    mesh.name = name;
    mesh.centre = params.centre;
    mesh.radius = params.radius;

    for (const SubMesh& sm : params.mesh_data.sub_meshes) {
        mesh.submeshes.push_back({ sm.index_range.begin, sm.index_range.amount });
    }

    GLuint vao_id = 0;
    glGenVertexArrays(1, &vao_id);
    mesh.vertex_array.set(vao_id);

    glBindVertexArray(vao_id);

    { // Upload vertex data to GPU
        mesh.vertex_buffer = params.vertex_buffer;
        ++mesh.vertex_buffer->num_users;

        const auto vbo_data = params.mesh_data.vertices.as_bytes();
        const auto vbo_id = narrow<GLuint>(mesh.vertex_buffer->handle.get());
        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
        glBufferSubData(GL_ARRAY_BUFFER,
                        narrow<GLintptr>(params.vbo_data_offset),
                        narrow<GLsizeiptr>(vbo_data.size()),
                        vbo_data.data());
    }

    { // Upload index data to GPU
        mesh.index_buffer = params.index_buffer;
        ++mesh.index_buffer->num_users;

        const auto ibo_data = params.mesh_data.indices.as_bytes();
        const auto ibo_id = narrow<GLuint>(mesh.index_buffer->handle.get());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                        narrow<GLintptr>(params.ibo_data_offset),
                        narrow<GLsizeiptr>(ibo_data.size()),
                        ibo_data.data());
    }

    // Set up vertex attributes (how OpenGL is to interpret the vertex data).
    uint32_t attrib_i = 0;
    uintptr_t offset = 0;
    uint32_t stride = 0;

    for (const VertexAttribute& a : g_attrib_array) {
        stride += a.size;
    }

    for (const VertexAttribute& a : g_attrib_array) {
        const GLvoid* gl_offset{};
        std::memcpy(&gl_offset, &offset, sizeof(offset));
        glVertexAttribPointer(attrib_i,
                              narrow<GLint>(a.num),
                              static_cast<uint32_t>(a.type),
                              static_cast<GLboolean>(a.normalised),
                              narrow<int32_t>(stride),
                              gl_offset); // NOLINT

        glEnableVertexAttribArray(attrib_i);

        offset += a.size;
        ++attrib_i;
    }

    glBindVertexArray(0);
}

void MeshRepositoryImpl::_clear_mesh(Mesh& mesh)
{
    MG_GFX_DEBUG_GROUP("MeshRepositoryImpl::_clear_mesh");

    const auto vao_id = narrow<GLuint>(mesh.vertex_array.get());
    mesh.vertex_array.set(0);

    if (vao_id == 0) {
        return;
    }

    const auto* meshname[[maybe_unused]] = mesh.name.c_str();

    MG_LOG_DEBUG(fmt::format("Deleting Mesh '{}'", vao_id, meshname));
    glDeleteVertexArrays(1, &vao_id);

    MG_ASSERT(mesh.vertex_buffer && mesh.index_buffer);

    if (--mesh.vertex_buffer->num_users == 0) {
        const auto vbo_id = narrow<GLuint>(mesh.vertex_buffer->handle.get());
        glDeleteBuffers(1, &vbo_id);
        const auto vbo_it = m_vertex_buffers.get_iterator_from_pointer(mesh.vertex_buffer);
        m_vertex_buffers.erase(vbo_it);
    }

    if (--mesh.index_buffer->num_users == 0) {
        const auto ibo_id = narrow<GLuint>(mesh.index_buffer->handle.get());
        glDeleteBuffers(1, &ibo_id);
        const auto ibo_it = m_index_buffers.get_iterator_from_pointer(mesh.index_buffer);
        m_index_buffers.erase(ibo_it);
    }
}

class MeshBufferImpl {
public:
    MeshBufferImpl(MeshRepositoryImpl& mesh_repository,
                   VertexBufferSize vertex_buffer_size,
                   IndexBufferSize index_buffer_size)
        : m_mesh_repository(&mesh_repository)
        , m_vbo_size(static_cast<size_t>(vertex_buffer_size))
        , m_ibo_size(static_cast<size_t>(index_buffer_size))
        , m_vertex_buffer(mesh_repository._make_vertex_buffer(m_vbo_size))
        , m_index_buffer(mesh_repository._make_index_buffer(m_ibo_size))
    {}

    MeshBuffer::CreateReturn create(const MeshResource& resource)
    {
        MG_GFX_DEBUG_GROUP("MeshBufferImpl::create");

        if (resource.vertices().size_bytes() + m_vbo_offset > m_vbo_size) {
            return { nullopt, MeshBuffer::ReturnCode::Vertex_buffer_full };
        }

        if (resource.indices().size_bytes() + m_ibo_offset > m_ibo_size) {
            return { nullopt, MeshBuffer::ReturnCode::Index_buffer_full };
        }

        const MeshDataView data = resource.data_view();

        const auto [centre, radius] =
            data.bounding_info.value_or(calculate_mesh_bounding_info(data.vertices, data.indices));

        MeshRepositoryImpl::MakeMeshParams params = {};
        params.vertex_buffer = m_vertex_buffer;
        params.index_buffer = m_index_buffer;
        params.vbo_data_offset = 0;
        params.ibo_data_offset = 0;
        params.centre = centre;
        params.radius = radius;
        params.mesh_data = data;

        const MeshHandle mesh_handle =
            m_mesh_repository->_make_mesh(resource.resource_id(),
                                          m_mesh_repository->_mesh_params_from_mesh_data(
                                              resource.data_view()));

        m_vbo_offset += resource.vertices().size_bytes();
        m_ibo_offset += resource.indices().size_bytes();

        return { mesh_handle, MeshBuffer::ReturnCode::Success };
    }

private:
    MeshRepositoryImpl* m_mesh_repository = nullptr;

    size_t m_vbo_offset = 0; // Current offset into vertex buffer -- where to put next mesh's data
    size_t m_ibo_offset = 0; // Current offset into index buffer -- where to put next mesh's data

    size_t m_vbo_size = 0;
    size_t m_ibo_size = 0;

    SharedBuffer* m_vertex_buffer = nullptr;
    SharedBuffer* m_index_buffer = nullptr;
};

//--------------------------------------------------------------------------------------------------

MeshBuffer::~MeshBuffer() = default;

MeshBuffer::CreateReturn MeshBuffer::create(const MeshResource& resource)
{
    return impl().create(resource);
}

//--------------------------------------------------------------------------------------------------

MeshRepository::MeshRepository() = default;
MeshRepository::~MeshRepository() = default;

MeshHandle MeshRepository::create(const MeshResource& mesh_res)
{
    MG_GFX_DEBUG_GROUP("MeshRepository::create")
    return impl().create(mesh_res.resource_id(), mesh_res.data_view());
}

MeshHandle MeshRepository::create(const MeshDataView& mesh_data, Identifier name)
{
    MG_GFX_DEBUG_GROUP("MeshRepository::create")
    return impl().create(name, mesh_data);
}

Opt<MeshHandle> MeshRepository::get(Identifier name) const
{
    return impl().get(name);
}

void MeshRepository::destroy(MeshHandle handle)
{
    MG_GFX_DEBUG_GROUP("MeshRepository::destroy")
    impl().destroy(handle);
}

bool MeshRepository::update(const MeshResource& mesh_res)
{
    MG_GFX_DEBUG_GROUP("MeshRepository::update")
    return impl().update(mesh_res.resource_id(), mesh_res.data_view());
}

MeshBuffer MeshRepository::new_mesh_buffer(VertexBufferSize vertex_buffer_size,
                                           IndexBufferSize index_buffer_size)
{
    MG_GFX_DEBUG_GROUP("MeshRepository::new_mesh_buffer")
    return MeshBuffer{ impl(), vertex_buffer_size, index_buffer_size };
}

} // namespace Mg::gfx
