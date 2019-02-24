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

namespace Mg::input {

// Name look-up cache
static std::array<const char*, Keyboard::k_num_keys> key_names;

// Map from Keyboard::Key enum values to GLFW key codes.
static constexpr std::array<int, Keyboard::k_num_keys> k_glfw_key_codes{ {
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

static int glfw_key_code(Keyboard::Key key)
{
    // Slightly evil enum to index cast, relies on k_glfw_key_codes having same layout as Key enum.
    return k_glfw_key_codes.at(static_cast<size_t>(key));
}

std::string Keyboard::description(InputSource::Id id) const
{
    const auto key = static_cast<Key>(id);

    // Check if name is in key_names cache
    const char** p_name = &key_names.at(id);
    if (*p_name != nullptr) { return *p_name; }

    // Ask GLFW for localised key name
    // TODO: bug in GLFW < 3.3 makes this return LATIN-1 instead of UTF-8 when run in an X11 env.
    // Enforce use of GLFW >= 3.3 in build system?
    const char* key_name = glfwGetKeyName(glfw_key_code(key), 0);

    if (key_name != nullptr) { return key_name; }

    // If glfwGetKeyName does not return localised name, fall back to these.
    g_log.write_warning(
        fmt::format("Input: could not get localised name of key {}.", glfw_key_code(key)));

    switch (key) {
    case Key::Space:
        *p_name = "Space";
        break;
    case Key::Backslash:
        *p_name = "Backslash";
        break;
    case Key::World1:
        *p_name = "World_1";
        break;
    case Key::World2:
        *p_name = "World_2";
        break;
    case Key::Esc:
        *p_name = "Escape";
        break;
    case Key::Enter:
        *p_name = "Enter";
        break;
    case Key::Tab:
        *p_name = "Tab";
        break;
    case Key::Backspace:
        *p_name = "Backspace";
        break;
    case Key::Ins:
        *p_name = "Insert";
        break;
    case Key::Del:
        *p_name = "Delete";
        break;
    case Key::Right:
        *p_name = "Right";
        break;
    case Key::Left:
        *p_name = "Left";
        break;
    case Key::Down:
        *p_name = "Down";
        break;
    case Key::Up:
        *p_name = "Up";
        break;
    case Key::PageUp:
        *p_name = "Page Up";
        break;
    case Key::PageDown:
        *p_name = "Page Down";
        break;
    case Key::Home:
        *p_name = "Home";
        break;
    case Key::End:
        *p_name = "End";
        break;
    case Key::CapsLock:
        *p_name = "Caps Lock";
        break;
    case Key::ScrollLock:
        *p_name = "Scroll Lock";
        break;
    case Key::NumLock:
        *p_name = "Num Lock";
        break;
    case Key::PrintScreen:
        *p_name = "Print Screen";
        break;
    case Key::Pause:
        *p_name = "Pause";
        break;
    case Key::F1:
        *p_name = "F1";
        break;
    case Key::F2:
        *p_name = "F2";
        break;
    case Key::F3:
        *p_name = "F3";
        break;
    case Key::F4:
        *p_name = "F4";
        break;
    case Key::F5:
        *p_name = "F5";
        break;
    case Key::F6:
        *p_name = "F6";
        break;
    case Key::F7:
        *p_name = "F7";
        break;
    case Key::F8:
        *p_name = "F8";
        break;
    case Key::F9:
        *p_name = "F9";
        break;
    case Key::F10:
        *p_name = "F10";
        break;
    case Key::F11:
        *p_name = "F11";
        break;
    case Key::F12:
        *p_name = "F12";
        break;
    case Key::KP_0:
        *p_name = "Keypad 0";
        break;
    case Key::KP_1:
        *p_name = "Keypad 1";
        break;
    case Key::KP_2:
        *p_name = "Keypad 2";
        break;
    case Key::KP_3:
        *p_name = "Keypad 3";
        break;
    case Key::KP_4:
        *p_name = "Keypad 4";
        break;
    case Key::KP_5:
        *p_name = "Keypad 5";
        break;
    case Key::KP_6:
        *p_name = "Keypad 6";
        break;
    case Key::KP_7:
        *p_name = "Keypad 7";
        break;
    case Key::KP_8:
        *p_name = "Keypad 8";
        break;
    case Key::KP_9:
        *p_name = "Keypad 9";
        break;
    case Key::KP_decimal:
        *p_name = "Keypad Decimal";
        break;
    case Key::KP_divide:
        *p_name = "Keypad Divide";
        break;
    case Key::KP_multiply:
        *p_name = "Keypad Multiply";
        break;
    case Key::KP_subtract:
        *p_name = "Keypad Subtract";
        break;
    case Key::KP_add:
        *p_name = "Keypad Add";
        break;
    case Key::KP_enter:
        *p_name = "Keypad Enter";
        break;
    case Key::KP_equal:
        *p_name = "Keypad Equal";
        break;
    case Key::LeftShift:
        *p_name = "Left Shift";
        break;
    case Key::LeftControl:
        *p_name = "Left Control";
        break;
    case Key::LeftAlt:
        *p_name = "Left Alt";
        break;
    case Key::LeftSuper:
        *p_name = "Left Super";
        break;
    case Key::RightShift:
        *p_name = "Right Shift";
        break;
    case Key::RightControl:
        *p_name = "Right Control";
        break;
    case Key::RightAlt:
        *p_name = "Right Alt";
        break;
    case Key::RightSuper:
        *p_name = "Right Super";
        break;
    case Key::Menu:
        *p_name = "Menu";
        break;
    default:
        g_log.write_error(fmt::format("Keyboard::description(): unexpected id {}", id));
        MG_ASSERT(false);
    }

    return *p_name;
}

float Keyboard::state(InputSource::Id id) const
{
    return static_cast<float>(m_key_states.test(id));
}

void Keyboard::refresh()
{
    auto refresh_state = [&](size_t key_id) {
        auto glfw_key_code   = k_glfw_key_codes.at(key_id);
        bool is_pressed      = glfwGetKey(m_window.glfw_window(), glfw_key_code) == GLFW_PRESS;
        m_key_states[key_id] = is_pressed;
    };

    for (size_t i = 0; i < k_num_keys; ++i) { refresh_state(i); }
}

} // namespace Mg::input
