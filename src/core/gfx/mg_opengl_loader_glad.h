//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2025, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#pragma once

#ifdef WIN32
#    ifndef APIENTRY
#        define APIENTRY __stdcall
#    endif
#endif

#include <glad/glad.h>

namespace Mg {
class Window;
}

namespace Mg::gfx {

void init_opengl_context(Window& window);

} // namespace Mg::gfx
