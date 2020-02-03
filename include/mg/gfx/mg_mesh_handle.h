//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_mesh_handle.h
 * Handle to a mesh. @see MeshRepository
 */

#pragma once

#include <cstdint>

namespace Mg::gfx {

// Note 2018-09-26: a MeshHandle is 'actually' an opaque pointer to an internal structure that may
// differ depending on renderer backend. As of writing, only an OpenGL-backend exists. This may or
// may not change in the future.

/** Opaque handle to a mesh. @see MeshRepository */
enum class MeshHandle : uintptr_t;

} // namespace Mg::gfx
