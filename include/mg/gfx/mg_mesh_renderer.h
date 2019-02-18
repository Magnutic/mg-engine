//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_mesh_renderer.h
 * Mesh renderer using the clustered forward rendering technique.
 */

#pragma once

#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

namespace Mg::gfx {

struct Light;
class ICamera;
class RenderCommandList;
class Material;

struct MeshRendererData;

// TODO: Should be renamed to just MeshRenderer, but defer that for a while to avoid nasty merge
// conflicts.
class MeshRenderer : PimplMixin<MeshRendererData> {
public:
    MeshRenderer();
    MG_MAKE_DEFAULT_MOVABLE(MeshRenderer);
    MG_MAKE_NON_COPYABLE(MeshRenderer);
    ~MeshRenderer();

    /** Drop all shaders generated for this renderer. This means that each shader will be recompiled
     * from source on the next use. This is useful for hot-reloading of shader assets.
     */
    void drop_shaders();

    /** Render the supplied list of meshes. */
    void render(const ICamera& cam, const RenderCommandList& mesh_list, span<const Light> lights);
};

} // namespace Mg::gfx
