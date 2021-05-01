//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file input/mg_mouse.h
 * Mouse input handling.
 */

#pragma once

#include "mg/input/mg_input.h"

#include <bitset>

namespace Mg {
class Window;
}

namespace Mg::input {

/** Mouse input-device implementation. */
class Mouse final : public IInputDevice {
public:
    explicit Mouse(const Window& window) noexcept : m_window(window) { m_button_states.reset(); }

    enum class Button { left, right, middle, button4, button5, button6, button7, button8 };
    static constexpr size_t k_num_buttons = 8;

    enum class Axis {
        pos_x,   /* X-coordinate in absolute position. */
        pos_y,   /* Y-coordinate in absolute position. */
        delta_x, /* Difference in X-coordinate since last refresh. */
        delta_y, /* Difference in Y-coordinate since last refresh. */
    };
    static constexpr size_t k_num_axes = 4;

    static constexpr size_t k_max_mouse_input_id = k_num_buttons + k_num_axes;

    InputSource button(Button button) const;
    InputSource axis(Axis axis) const;

    struct CursorPosition {
        float x = 0.0f;
        float y = 0.0f;
    };

    CursorPosition get_cursor_position() const;

    void refresh();

protected:
    float state(InputSource::Id id) const override;
    std::string description(InputSource::Id id) const override;

private:
    const Window& m_window;

    std::bitset<k_num_buttons> m_button_states;

    float m_x_pos = 0.0f, m_y_pos = 0.0f, m_x_delta = 0.0f, m_y_delta = 0.0f;
};
} // namespace Mg::input
