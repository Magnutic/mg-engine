//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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

struct RenderParameters {
    float current_time;
    float camera_exposure;
};

class MeshRenderer : PImplMixin<MeshRendererData> {
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
    void render(const ICamera& cam,
                const RenderCommandList& command_list,
                span<const Light> lights,
                RenderParameters params);
};

} // namespace Mg::gfx
