//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_render_command_list_data.h
 * Handle to data associated with render command list.
 * @see Mg::gfx::RenderCommandList
 */

#pragma once

#include <cstdint>

namespace Mg::gfx {

/** Handle to internal render command data buffer.
 * Actual type is internal (see mg_render_command_data.h)
 */
enum class RenderCommandDataHandle : uintptr_t;

} // namespace Mg::gfx
