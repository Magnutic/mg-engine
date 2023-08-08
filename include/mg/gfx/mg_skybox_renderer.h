//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2023, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_skybox_renderer.h
 * Draws a skybox environment.
 */

#pragma once

#include "mg/gfx/mg_texture2d.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_impl_ptr.h"

namespace Mg::gfx {

class Material;
class ICamera;
class IRenderTarget;

class SkyboxRenderer {
public:
    SkyboxRenderer();
    ~SkyboxRenderer();

    MG_MAKE_NON_MOVABLE(SkyboxRenderer);
    MG_MAKE_NON_COPYABLE(SkyboxRenderer);

    void draw(const IRenderTarget& render_target,
              const ICamera& camera,
              const Material& material);

    void drop_shaders();

    struct Impl;

private:
    ImplPtr<Impl> m_impl;
};

} // namespace Mg::gfx
