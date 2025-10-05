//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_material_binding.h
 * .
 */

#pragma once

#include "mg/core/mg_identifier.h"

namespace Mg::gfx {

class Material;

/** Tells which material to use when rendering. */
struct MaterialBinding {
    /** Binding id in the mesh. This identifies which submeshes shall use the material. */
    Identifier material_binding_id;

    /** Material to use. */
    const Material* material = nullptr;
};


} // namespace Mg::gfx
