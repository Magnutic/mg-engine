//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/input/mg_keyboard.h"

#include "mg/core/mg_log.h"
#include "mg/utils/mg_assert.h"

#include <GLFW/glfw3.h>

namespace Mg::input {

namespace {

const char* default_key_name(const Key key)
{
    switch (key) {
    case Key::Space:
        return "Space";
    case Key::Backslash:
        return "Backslash";
    case Key::World1:
        return "World_1";
    case Key::World2:
        return "World_2";
    case Key::Esc:
        return "Escape";
    case Key::Enter:
        return "Enter";
    case Key::Tab:
        return "Tab";
    case Key::Backspace:
        return "Backspace";
    case Key::Ins:
        return "Insert";
    case Key::Del:
        return "Delete";
    case Key::Right:
        return "Right";
    case Key::Left:
        return "Left";
    case Key::Down:
        return "Down";
    case Key::Up:
        return "Up";
    case Key::PageUp:
        return "Page Up";
    case Key::PageDown:
        return "Page Down";
    case Key::Home:
        return "Home";
    case Key::End:
        return "End";
    case Key::CapsLock:
        return "Caps Lock";
    case Key::ScrollLock:
        return "Scroll Lock";
    case Key::NumLock:
        return "Num Lock";
    case Key::PrintScreen:
        return "Print Screen";
    case Key::Pause:
        return "Pause";
    case Key::F1:
        return "F1";
    case Key::F2:
        return "F2";
    case Key::F3:
        return "F3";
    case Key::F4:
        return "F4";
    case Key::F5:
        return "F5";
    case Key::F6:
        return "F6";
    case Key::F7:
        return "F7";
    case Key::F8:
        return "F8";
    case Key::F9:
        return "F9";
    case Key::F10:
        return "F10";
    case Key::F11:
        return "F11";
    case Key::F12:
        return "F12";
    case Key::KP_0:
        return "Keypad 0";
    case Key::KP_1:
        return "Keypad 1";
    case Key::KP_2:
        return "Keypad 2";
    case Key::KP_3:
        return "Keypad 3";
    case Key::KP_4:
        return "Keypad 4";
    case Key::KP_5:
        return "Keypad 5";
    case Key::KP_6:
        return "Keypad 6";
    case Key::KP_7:
        return "Keypad 7";
    case Key::KP_8:
        return "Keypad 8";
    case Key::KP_9:
        return "Keypad 9";
    case Key::KP_decimal:
        return "Keypad Decimal";
    case Key::KP_divide:
        return "Keypad Divide";
    case Key::KP_multiply:
        return "Keypad Multiply";
    case Key::KP_subtract:
        return "Keypad Subtract";
    case Key::KP_add:
        return "Keypad Add";
    case Key::KP_enter:
        return "Keypad Enter";
    case Key::KP_equal:
        return "Keypad Equal";
    case Key::LeftShift:
        return "Left Shift";
    case Key::LeftControl:
        return "Left Control";
    case Key::LeftAlt:
        return "Left Alt";
    case Key::LeftSuper:
        return "Left Super";
    case Key::RightShift:
        return "Right Shift";
    case Key::RightControl:
        return "Right Control";
    case Key::RightAlt:
        return "Right Alt";
    case Key::RightSuper:
        return "Right Super";
    case Key::Menu:
        return "Menu";
    default:
        log.error("localized_key_name(): unexpected key id {}", static_cast<int>(key));
        MG_ASSERT(false);
    }
};

int glfw_key_code(Key key)
{
    return static_cast<int>(key);
}

} // namespace

std::string localized_key_name(const Key key)
{
    // Ask GLFW for localized key name
    const char* localized_name = glfwGetKeyName(glfw_key_code(key), 0);
    if (localized_name != nullptr) {
        return localized_name;
    }

    // If glfwGetKeyName does not return localized name, fall back to these.
    log.warning("Input: could not get localized name of key {}.", glfw_key_code(key));
    return default_key_name(key);
}

} // namespace Mg::input
