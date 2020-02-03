//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_material_repository.h
 * Creates, stores, and updates materials.
 */

#pragma once

#include "mg/core/mg_identifier.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

namespace Mg {
class ShaderResource;
}

namespace Mg::gfx {

class Material;
struct MaterialRepositoryData;

class MaterialRepository : PImplMixin<MaterialRepositoryData> {
public:
    MG_MAKE_NON_MOVABLE(MaterialRepository);
    MG_MAKE_NON_COPYABLE(MaterialRepository);

    MaterialRepository();
    ~MaterialRepository();

    Material* create(Identifier id, ResourceHandle<ShaderResource> shader);

    void destroy(const Material* handle);
};

} // namespace Mg::gfx
