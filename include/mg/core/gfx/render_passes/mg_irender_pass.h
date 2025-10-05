//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_irender_pass.h
 */

#pragma once

#include "mg/core/gfx/mg_camera.h"
#include "mg/utils/mg_macros.h"

namespace Mg::gfx {

struct RenderParams {
    const ICamera& camera;
    float time_since_init;
};

class IRenderPass {
public:
    MG_INTERFACE_BOILERPLATE(IRenderPass);
    virtual void render(const RenderParams&) = 0;
};

} // namespace Mg::gfx
