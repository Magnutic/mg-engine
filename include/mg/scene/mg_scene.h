//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_scene.h
 */

#pragma once

#include "mg/gfx/mg_material_pool.h"
#include "mg/gfx/mg_mesh_pool.h"
#include "mg/gfx/mg_texture_pool.h"
#include "mg/resource_cache/mg_resource_cache.h"

namespace Mg {

class SceneResources {
public:
    explicit SceneResources(std::shared_ptr<ResourceCache> resource_cache)
        : m_resource_cache{ std::move(resource_cache) }
        , m_mesh_pool{ std::make_shared<gfx::MeshPool>(m_resource_cache) }
        , m_texture_pool{ std::make_shared<gfx::TexturePool>(m_resource_cache) }
        , m_material_pool{ std::make_shared<gfx::MaterialPool>(m_resource_cache, m_texture_pool) }
    {}

protected:
    std::shared_ptr<ResourceCache> m_resource_cache;
    std::shared_ptr<gfx::MeshPool> m_mesh_pool;
    std::shared_ptr<gfx::TexturePool> m_texture_pool;
    std::shared_ptr<gfx::MaterialPool> m_material_pool;
};

} // namespace Mg
