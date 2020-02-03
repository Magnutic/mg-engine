//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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
