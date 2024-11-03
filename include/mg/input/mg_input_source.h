//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_input_source.h
 * Virtual interface for input sources.
 */

#pragma once

#include "mg/input/mg_keyboard.h"
#include "mg/input/mg_mouse.h"
#include "mg/utils/mg_macros.h"

namespace Mg::input {

enum class InputEvent {
    Press,
    Release,
};

class IButtonEventHandler {
public:
    MG_INTERFACE_BOILERPLATE(IButtonEventHandler);

    virtual void handle_key_event(Key key, InputEvent event) = 0;
    virtual void handle_mouse_button_event(MouseButton button, InputEvent event) = 0;
};

class IMouseMovementEventHandler {
public:
    MG_INTERFACE_BOILERPLATE(IMouseMovementEventHandler);

    virtual void handle_mouse_move_event(float x, float y, bool is_cursor_locked_to_window) = 0;
};

class IScrollEventHandler {
public:
    MG_INTERFACE_BOILERPLATE(IScrollEventHandler);

    virtual void handle_scroll_event(float xoffset, float yoffset) = 0;
};

class IInputSource {
public:
    MG_INTERFACE_BOILERPLATE(IInputSource);

    virtual void register_button_event_handler(input::IButtonEventHandler& handler) = 0;
    virtual void deregister_button_event_handler(input::IButtonEventHandler& handler) = 0;

    virtual void
    register_mouse_movement_event_handler(input::IMouseMovementEventHandler& handler) = 0;
    virtual void
    deregister_mouse_movement_event_handler(input::IMouseMovementEventHandler& handler) = 0;

    virtual void register_scroll_event_handler(input::IScrollEventHandler& handler) = 0;
    virtual void deregister_scroll_event_handler(input::IScrollEventHandler& handler) = 0;
};

} // namespace Mg::input
