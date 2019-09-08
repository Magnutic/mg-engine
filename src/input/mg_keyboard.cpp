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

#include "mg/input/mg_keyboard.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_window.h"
#include "mg/utils/mg_gsl.h"

#include <GLFW/glfw3.h>

#include <fmt/core.h>

#include <array>

namespace Mg::input {

namespace {

// Name look-up cache
std::array<std::string, Keyboard::k_num_keys> g_key_names{};

const char* default_key_name(Keyboard::Key key)
{
    switch (key) {
    case Keyboard::Key::Space:
        return "Space";
    case Keyboard::Key::Backslash:
        return "Backslash";
    case Keyboard::Key::World1:
        return "World_1";
    case Keyboard::Key::World2:
        return "World_2";
    case Keyboard::Key::Esc:
        return "Escape";
    case Keyboard::Key::Enter:
        return "Enter";
    case Keyboard::Key::Tab:
        return "Tab";
    case Keyboard::Key::Backspace:
        return "Backspace";
    case Keyboard::Key::Ins:
        return "Insert";
    case Keyboard::Key::Del:
        return "Delete";
    case Keyboard::Key::Right:
        return "Right";
    case Keyboard::Key::Left:
        return "Left";
    case Keyboard::Key::Down:
        return "Down";
    case Keyboard::Key::Up:
        return "Up";
    case Keyboard::Key::PageUp:
        return "Page Up";
    case Keyboard::Key::PageDown:
        return "Page Down";
    case Keyboard::Key::Home:
        return "Home";
    case Keyboard::Key::End:
        return "End";
    case Keyboard::Key::CapsLock:
        return "Caps Lock";
    case Keyboard::Key::ScrollLock:
        return "Scroll Lock";
    case Keyboard::Key::NumLock:
        return "Num Lock";
    case Keyboard::Key::PrintScreen:
        return "Print Screen";
    case Keyboard::Key::Pause:
        return "Pause";
    case Keyboard::Key::F1:
        return "F1";
    case Keyboard::Key::F2:
        return "F2";
    case Keyboard::Key::F3:
        return "F3";
    case Keyboard::Key::F4:
        return "F4";
    case Keyboard::Key::F5:
        return "F5";
    case Keyboard::Key::F6:
        return "F6";
    case Keyboard::Key::F7:
        return "F7";
    case Keyboard::Key::F8:
        return "F8";
    case Keyboard::Key::F9:
        return "F9";
    case Keyboard::Key::F10:
        return "F10";
    case Keyboard::Key::F11:
        return "F11";
    case Keyboard::Key::F12:
        return "F12";
    case Keyboard::Key::KP_0:
        return "Keypad 0";
    case Keyboard::Key::KP_1:
        return "Keypad 1";
    case Keyboard::Key::KP_2:
        return "Keypad 2";
    case Keyboard::Key::KP_3:
        return "Keypad 3";
    case Keyboard::Key::KP_4:
        return "Keypad 4";
    case Keyboard::Key::KP_5:
        return "Keypad 5";
    case Keyboard::Key::KP_6:
        return "Keypad 6";
    case Keyboard::Key::KP_7:
        return "Keypad 7";
    case Keyboard::Key::KP_8:
        return "Keypad 8";
    case Keyboard::Key::KP_9:
        return "Keypad 9";
    case Keyboard::Key::KP_decimal:
        return "Keypad Decimal";
    case Keyboard::Key::KP_divide:
        return "Keypad Divide";
    case Keyboard::Key::KP_multiply:
        return "Keypad Multiply";
    case Keyboard::Key::KP_subtract:
        return "Keypad Subtract";
    case Keyboard::Key::KP_add:
        return "Keypad Add";
    case Keyboard::Key::KP_enter:
        return "Keypad Enter";
    case Keyboard::Key::KP_equal:
        return "Keypad Equal";
    case Keyboard::Key::LeftShift:
        return "Left Shift";
    case Keyboard::Key::LeftControl:
        return "Left Control";
    case Keyboard::Key::LeftAlt:
        return "Left Alt";
    case Keyboard::Key::LeftSuper:
        return "Left Super";
    case Keyboard::Key::RightShift:
        return "Right Shift";
    case Keyboard::Key::RightControl:
        return "Right Control";
    case Keyboard::Key::RightAlt:
        return "Right Alt";
    case Keyboard::Key::RightSuper:
        return "Right Super";
    case Keyboard::Key::Menu:
        return "Menu";
    default:
        g_log.write_error(fmt::format("Keyboard::description(): unexpected id {}",
                                      static_cast<InputSource::Id>(key)));
        MG_ASSERT(false);
    }
};

// Map from Keyboard::Key enum values to GLFW key codes.
constexpr std::array<int, Keyboard::k_num_keys> k_glfw_key_codes{ {
    32, // Space

    39, // Apostrophe

    44, // Comma
    45, // Minus
    46, // Period
    47, // Slash
    48, // Num0
    49, // Num1
    50, // Num2
    51, // Num3
    52, // Num4
    53, // Num5
    54, // Num6
    55, // Num7
    56, // Num8
    57, // Num9

    59, // Semicolon
    61, // Equal

    65, // A
    66, // B
    67, // C
    68, // D
    69, // E
    70, // F
    71, // G
    72, // H
    73, // I
    74, // J
    75, // K
    76, // L
    77, // M
    78, // N
    79, // O
    80, // P
    81, // Q
    82, // R
    83, // S
    84, // T
    85, // U
    86, // V
    87, // W
    88, // X
    89, // Y
    90, // Z
    91, // LeftBracket
    92, // Backslash
    93, // RightBracket

    96, // GraveAccent

    161, // World1
    162, // World2

    256, // Esc
    257, // Enter
    258, // Tab
    259, // Backspace
    260, // Ins
    261, // Del
    262, // Right
    263, // Left
    264, // Down
    265, // Up
    266, // PageUp
    267, // PageDown
    268, // Home
    269, // End

    280, // CapsLock
    281, // ScrollLock
    282, // NumLock
    283, // PrintScreen
    284, // Pause

    290, // F1
    291, // F2
    292, // F3
    293, // F4
    294, // F5
    295, // F6
    296, // F7
    297, // F8
    298, // F9
    299, // F10
    300, // F11
    301, // F12

    320, // KP_0
    321, // KP_1
    322, // KP_2
    323, // KP_3
    324, // KP_4
    325, // KP_5
    326, // KP_6
    327, // KP_7
    328, // KP_8
    329, // KP_9
    330, // KP_decimal
    331, // KP_divide
    332, // KP_multiply
    333, // KP_subtract
    334, // KP_add
    335, // KP_enter
    336, // KP_equal

    340, // LeftShift
    341, // LeftControl
    342, // LeftAlt
    343, // LeftSuper
    344, // RightShift
    345, // RightControl
    346, // RightAlt
    347, // RightSuper
    348  // Menu
} };

int glfw_key_code(Keyboard::Key key)
{
    // Slightly evil enum to index cast, relies on k_glfw_key_codes having same layout as Key enum.
    return k_glfw_key_codes.at(static_cast<size_t>(key));
}

} // namespace

std::string Keyboard::description(InputSource::Id id) const
{
    const auto key = static_cast<Key>(id);

    // Check if name is in g_key_names cache
    std::string& name = g_key_names.at(id);
    if (name != "") { return name; }

    // Ask GLFW for localised key name
    // TODO: bug in GLFW < 3.3 makes this return LATIN-1 instead of UTF-8 when run in an X11 env.
    // Enforce use of GLFW >= 3.3 in build system?
    const char* localised_name = glfwGetKeyName(glfw_key_code(key), 0);

    if (localised_name != nullptr) {
        name = localised_name;
        return name;
    }

    // If glfwGetKeyName does not return localised name, fall back to these.
    g_log.write_warning(
        fmt::format("Input: could not get localised name of key {}.", glfw_key_code(key)));

    name = default_key_name(key);
    return name;
}

float Keyboard::state(InputSource::Id id) const
{
    return static_cast<float>(m_key_states.test(id));
}

void Keyboard::refresh()
{
    const auto refresh_state = [&](size_t key_id) {
        const auto glfw_key_code = k_glfw_key_codes.at(key_id);
        const bool is_pressed    = glfwGetKey(m_window.glfw_window(), glfw_key_code) == GLFW_PRESS;
        m_key_states[key_id]     = is_pressed;
    };

    for (size_t i = 0; i < k_num_keys; ++i) { refresh_state(i); }
}

} // namespace Mg::input
