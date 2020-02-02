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
    Mouse(const Window& window) noexcept : m_window(window) { m_button_states.reset(); }

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
