//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/input/mg_mouse.h"

#include <format>

namespace Mg::input {

std::string mouse_button_name(MouseButton button)
{
    return std::format("Mouse button {}", int(button) + 1);
}

} // namespace Mg::input
