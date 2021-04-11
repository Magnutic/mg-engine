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
    ~MeshRenderer();

    MG_MAKE_DEFAULT_MOVABLE(MeshRenderer);
    MG_MAKE_NON_COPYABLE(MeshRenderer);

    /** Render the supplied list of meshes. */
    void render(const ICamera& cam,
                const RenderCommandList& command_list,
                span<const Light> lights,
                RenderParameters params);

    // TODO prepare_shader does not really work, because the driver will not compile the shaders
    // until first use anyway. I need some way to issue dummy draw calls to force the driver to
    // compile the shaders.

    /** Prepare shader needed for rendering with `material`. This is not strictly necessary as
     * shaders will be compiled as needed on first use. Using this function, however, can reduce
     * stuttering by ensuring shaders are compiled in advance.
     *
     * Static and animated meshes use different shaders, which means that for the same material to
     * be used for an animated and for a static mesh, two shaders must be compiled. The bool
     * parameters control which of these shaders should be prepared.
     *
     * @param material Which material to prepare shaders for.
     * @param prepare_for_static_mesh Whether shader should be prepared for rendering static
     * (non-animated) meshes.
     * @param prepare_for_static_mesh Whether shader should be prepared for rendering animated
     * (skinned) meshes.
     */
    void prepare_shader(const Material& material,
                        const bool prepare_for_static_mesh,
                        const bool prepare_for_animated_mesh);

    /** Drop all shaders generated for this renderer. This means that each shader will be recompiled
     * from source on the next use. This is useful for hot-reloading of shader assets.
     */
    void drop_shaders();
};

} // namespace Mg::gfx
