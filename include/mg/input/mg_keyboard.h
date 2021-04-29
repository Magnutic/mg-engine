//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file input/mg_keyboard.h
 * Keyboard input handling.
 */

#pragma once

#include <bitset>
#include <string>

#include "mg/input/mg_input.h"

namespace Mg {
class Window;
}

namespace Mg::input {

class Keyboard final : public IInputDevice {
public:
    /** Each value corresponds to a physical key location on the keyboard, and is named for its
     * meaning on a US-English QWERTY keyboard. To get the localised name for the key as the user
     * perceives it, use `KeyBoard::key(key_id).description()`.
     */
    enum class Key {
        // clang-format off
        Space, Apostrophe, Comma, Minus, Period, Slash, Num0, Num1, Num2, Num3, Num4, Num5, Num6,
        Num7, Num8, Num9,

        Semicolon, Equal,

        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        LeftBracket, Backslash, RightBracket,

        GraveAccent,

        World1, World2,
        Esc, Enter, Tab, Backspace, Ins, Del, Right, Left, Down, Up, PageUp, PageDown, Home, End,

        CapsLock, ScrollLock, NumLock, PrintScreen, Pause,

        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        KP_0, KP_1, KP_2, KP_3, KP_4, KP_5, KP_6, KP_7, KP_8, KP_9,
        KP_decimal, KP_divide, KP_multiply, KP_subtract, KP_add, KP_enter, KP_equal,

        LeftShift, LeftControl, LeftAlt, LeftSuper, RightShift, RightControl, RightAlt, RightSuper,
        Menu
        // clang-format on
    };

    static constexpr size_t k_num_keys = static_cast<size_t>(Key::Menu) + 1;

public:
    explicit Keyboard(const Window& window) noexcept : m_window(window) { m_key_states.reset(); }

    InputSource key(Key key_id) const noexcept
    {
        return InputSource{ *this, static_cast<InputSource::Id>(key_id) };
    }

    void refresh();

protected:
    float state(InputSource::Id id) const override;

    std::string description(InputSource::Id id) const override;

private:
    const Window& m_window;
    std::bitset<k_num_keys> m_key_states;
};

} // namespace Mg::input
