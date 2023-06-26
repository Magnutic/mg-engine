//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_renderer.h
 * Mesh renderer using the clustered forward rendering technique.
 */

#pragma once

#include "mg/gfx/mg_light_grid_config.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_impl_ptr.h"
#include "mg/utils/mg_macros.h"

namespace Mg::gfx {

struct Light;
class ICamera;
class IRenderTarget;
class RenderCommandList;
class Material;

struct RenderParameters {
    float current_time;
    float camera_exposure;
};

class MeshRenderer {
public:
    MeshRenderer(const LightGridConfig& light_grid_config);

    /** Render the supplied list of meshes. */
    void render(const ICamera& cam,
                const RenderCommandList& command_list,
                std::span<const Light> lights,
                const IRenderTarget& render_target,
                RenderParameters params);

    /** Drop all shaders generated for this renderer. This means that each shader will be recompiled
     * from source on the next use. This is useful for hot-reloading of shader assets.
     */
    void drop_shaders();

    struct Impl;

private:
    ImplPtr<Impl> m_impl;
};

} // namespace Mg::gfx
