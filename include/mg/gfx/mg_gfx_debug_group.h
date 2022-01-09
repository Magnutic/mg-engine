//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_gfx_debug_group.h
 * RAII-guard for defining a graphics debugging scope.
 * This is be used to organize e.g. OpenGL calls when inspected with apitrace.
 */

#include "mg/utils/mg_macros.h"

namespace Mg::gfx::detail {

class GfxDebugGroupGuard {
public:
    explicit GfxDebugGroupGuard(const char* message);
    ~GfxDebugGroupGuard();

    MG_MAKE_NON_MOVABLE(GfxDebugGroupGuard);
    MG_MAKE_NON_COPYABLE(GfxDebugGroupGuard);
};

} // namespace Mg::gfx::detail

#if MG_ENABLE_GFX_DEBUG_GROUPS
/* RAII-guard for defining a graphics debugging scope.
 * This is be used to organize e.g. OpenGL calls when inspected with apitrace.
 */
#    define MG_GFX_DEBUG_GROUP(message) \
        ::Mg::gfx::detail::GfxDebugGroupGuard gfxDebugGroup_(message);
#else
#    define MG_GFX_DEBUG_GROUP(message)
#endif
