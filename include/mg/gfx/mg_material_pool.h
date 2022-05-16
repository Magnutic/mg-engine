//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_material_pool.h
 * Creates, stores, and updates materials.
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/core/mg_identifier.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

namespace Mg {
class ShaderResource;
class MaterialResource;
} // namespace Mg

namespace Mg::gfx {

class Material;
struct MaterialPoolData;
class TexturePool;

class MaterialPool : PImplMixin<MaterialPoolData> {
public:
    MG_MAKE_NON_MOVABLE(MaterialPool);
    MG_MAKE_NON_COPYABLE(MaterialPool);

    explicit MaterialPool(std::shared_ptr<gfx::TexturePool> texture_pool);
    ~MaterialPool();

    Material* create(Identifier id, ResourceHandle<ShaderResource> shader_resource_handle);
    Material* create(const MaterialResource& material_resource);

    void update(const MaterialResource& material_resource);

    void destroy(const Material* handle);

    Material* find(Identifier id);
    const Material* find(Identifier id) const;

    Array<Material*> get_all_materials();
    Array<const Material*> get_all_materials() const;
};

} // namespace Mg::gfx
