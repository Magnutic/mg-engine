//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_mesh_pool.h"

#include "mg/core/mg_log.h"
#include "mg/resource_cache/mg_resource_cache.h"
#include "mg/resources/mg_mesh_resource.h"
#include "mg_mesh_pool_impl.h"

namespace Mg::gfx {

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

    for (auto& mesh : m_impl->mesh_data) {
        clear_mesh(*m_impl, mesh);
    }

    m_impl->resource_cache->remove_resource_reload_callback("MeshResource"_id);
}

const Mesh* MeshPool::get_or_load(Identifier resource_id)
{
    MG_GFX_DEBUG_GROUP("MeshPool::create")
    if (auto* mesh = find(resource_id); mesh) {
        return mesh;
    }
    auto access = m_impl->resource_cache->access_resource<MeshResource>(resource_id);
    return create(access->data_view(), access->resource_id());
}

const Mesh* MeshPool::create(const mesh_data::MeshDataView& mesh_data, Identifier name)
{
    MG_GFX_DEBUG_GROUP("MeshPool::create")
    return ::Mg::gfx::create(*m_impl, mesh_data, name);
}

const Mesh* MeshPool::find(Identifier name) const
{
    return ::Mg::gfx::find(*m_impl, name);
}

void MeshPool::destroy(const Mesh* handle)
{
    MG_GFX_DEBUG_GROUP("MeshPool::destroy")
    ::Mg::gfx::destroy(*m_impl, handle);
}

bool MeshPool::update(const mesh_data::MeshDataView& mesh_data, Identifier name)
{
    MG_GFX_DEBUG_GROUP("MeshPool::update")

    Mesh* mesh = ::Mg::gfx::find(*m_impl, name);

    // If not found, then we do not have a mesh using the updated resource, so ignore.
    if (!mesh) {
        return false;
    }

    // Use the existing Mesh to ensure MeshHandles remain valid.
    make_mesh_at(*m_impl, *mesh, name, mesh_params_from_mesh_data(*m_impl, mesh_data));
    log.verbose("MeshPool::update(): Updated {}", name.str_view());
    return true;
}

bool MeshPool::update(const MeshResource& mesh_res)
{
    MG_GFX_DEBUG_GROUP("MeshPool::update")
    return update(mesh_res.data_view(), mesh_res.resource_id());
}

MeshBuffer MeshPool::new_mesh_buffer(mesh_data::VertexBufferSize vertex_buffer_size,
                                     mesh_data::IndexBufferSize index_buffer_size,
                                     mesh_data::InfluencesBufferSize influences_buffer_size)
{
    MG_GFX_DEBUG_GROUP("MeshPool::new_mesh_buffer")
    return MeshBuffer{ *m_impl, vertex_buffer_size, index_buffer_size, influences_buffer_size };
}

} // namespace Mg::gfx
