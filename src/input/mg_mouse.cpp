//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/input/mg_mouse.h"

#include <fmt/core.h>

namespace Mg::input {

std::string mouse_button_name(MouseButton button)
{
    return fmt::format("Mouse button {}", int(button) + 1);
}

} // namespace Mg::input
