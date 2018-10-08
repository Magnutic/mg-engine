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

/** @file mg_texture_handle.h
 * Handle to a texture. @see TextureRepository
 */

#pragma once

#include <cstdint>

namespace Mg::gfx {

// Note 2018-09-26: a TextureHandle is 'actually' an opaque pointer to an internal structure that
// may differ depending on renderer backend. As of writing, only an OpenGL-backend exists. This may
// or may not change in the future.

/** Opaque handle to a texture. @see TextureRepository */
enum class TextureHandle : uintptr_t;

} // namespace Mg::gfx
