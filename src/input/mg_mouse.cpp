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

#include "mg/input/mg_mouse.h"

#include <GLFW/glfw3.h>

#include <fmt/core.h>

#include "mg/core/mg_log.h"
#include "mg/core/mg_window.h"

namespace Mg::input {

namespace {

//--------------------------------------------------------------------------------------------------
// Convert between mouse input type enums and internal integral representation.
//--------------------------------------------------------------------------------------------------

enum class MouseType { Button, Axis };

MouseType type(InputSource::Id id)
{
    MG_ASSERT(id < Mouse::k_max_mouse_input_id);

    if (id < Mouse::k_num_buttons) { return MouseType::Button; }

    return MouseType::Axis;
}

InputSource::Id to_input_id(Mouse::Button button)
{
    return InputSource::Id(button);
}

Mouse::Button to_button(InputSource::Id id)
{
    MG_ASSERT_DEBUG(type(id) == MouseType::Button);
    return static_cast<Mouse::Button>(id);
}

InputSource::Id to_input_id(Mouse::Axis axis)
{
    return InputSource::Id(axis) + Mouse::k_num_buttons;
}

Mouse::Axis to_axis(InputSource::Id id)
{
    MG_ASSERT_DEBUG(type(id) == MouseType::Axis);
    return static_cast<Mouse::Axis>(id - Mouse::k_num_buttons);
}

std::string button_description(Mouse::Button button)
{
    return fmt::format("Mouse button {}", int(button) + 1);
}

std::string axis_description(Mouse::Axis axis)
{
    switch (axis) {
    case Mouse::Axis::pos_x:
        return "Mouse Position X";
    case Mouse::Axis::pos_y:
        return "Mouse Position Y";
    case Mouse::Axis::delta_x:
        return "Mouse Delta X";
    case Mouse::Axis::delta_y:
        return "Mouse Delta Y";
    default:
        MG_ASSERT(false);
        return "";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Mouse implementation
//--------------------------------------------------------------------------------------------------


InputSource Mouse::button(Button button) const
{
    return InputSource{ *this, to_input_id(button) };
}

InputSource Mouse::axis(Axis axis) const
{
    return InputSource{ *this, to_input_id(axis) };
}

std::string Mouse::description(InputSource::Id id) const
{
    switch (type(id)) {
    case MouseType::Button:
        return button_description(to_button(id));
    case MouseType::Axis:
        return axis_description(to_axis(id));
    default:
        MG_ASSERT(false);
        return "";
    }
}

float Mouse::state(InputSource::Id id) const
{
    switch (type(id)) {
    case MouseType::Button:
        return float(m_button_states.test(static_cast<size_t>(to_button(id))));
    case MouseType::Axis:
        switch (to_axis(id)) {
        case Axis::pos_x:
            return m_x_pos;
        case Axis::pos_y:
            return m_y_pos;
        case Axis::delta_x:
            return m_x_delta;
        case Axis::delta_y:
            return m_y_delta;
        }
        [[fallthrough]];
    default:
        MG_ASSERT(false);
        return {};
    }
}

void Mouse::refresh()
{
    GLFWwindow* window_handle = m_window.glfw_window();

    for (size_t i = 0; i < k_num_buttons; ++i) {
        m_button_states[i] = (glfwGetMouseButton(window_handle, int(i)) == GLFW_PRESS);
    }

    float prev_x_pos = m_x_pos;
    float prev_y_pos = m_y_pos;

    {
        double x, y;
        glfwGetCursorPos(window_handle, &x, &y);
        m_x_pos = float(x);
        m_y_pos = float(y);
    }

    m_x_delta = m_x_pos - prev_x_pos;
    m_y_delta = m_y_pos - prev_y_pos;
}

} // namespace Mg::input
