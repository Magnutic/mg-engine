//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file input/mg_mouse.h
 * Mouse input handling.
 */

#pragma once

#include <string>

namespace Mg::input {

enum class MouseButton { left, right, middle, button4, button5, button6, button7, button8 };

std::string mouse_button_name(MouseButton button);

} // namespace Mg::input
