//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/gfx/mg_gfx_debug_group.h"

#include "mg_glad.h"

namespace Mg::gfx::detail {

GfxDebugGroupGuard ::GfxDebugGroupGuard(const char* message)
{
    if (GLAD_GL_KHR_debug != 0) {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, message);
    }
}

GfxDebugGroupGuard::~GfxDebugGroupGuard()
{
    if (GLAD_GL_KHR_debug != 0) {
        glPopDebugGroup();
    }
}

} // namespace Mg::gfx::detail
