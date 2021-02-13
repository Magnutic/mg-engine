//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
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

    if (id < Mouse::k_num_buttons) {
        return MouseType::Button;
    }

    return MouseType::Axis;
}

InputSource::Id to_input_id(Mouse::Button button)
{
    return static_cast<InputSource::Id>(button);
}

Mouse::Button to_button(InputSource::Id id)
{
    MG_ASSERT_DEBUG(type(id) == MouseType::Button);
    return static_cast<Mouse::Button>(id);
}

InputSource::Id to_input_id(Mouse::Axis axis)
{
    return static_cast<InputSource::Id>(axis) + Mouse::k_num_buttons;
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
    case MouseType::Button: {
        const auto button_index = static_cast<size_t>(to_button(id));
        return narrow_cast<float>(m_button_states.test(button_index));
    }
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
        m_button_states[i] = (glfwGetMouseButton(window_handle, narrow<int>(i)) == GLFW_PRESS);
    }

    const float prev_x_pos = m_x_pos;
    const float prev_y_pos = m_y_pos;

    {
        double x{};
        double y{};
        glfwGetCursorPos(window_handle, &x, &y);
        m_x_pos = narrow_cast<float>(x);
        m_y_pos = narrow_cast<float>(y);
    }

    m_x_delta = m_x_pos - prev_x_pos;
    m_y_delta = m_y_pos - prev_y_pos;
}

} // namespace Mg::input
