//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_input_source.h
 * Virtual interface for input sources.
 */

#pragma once

#include "mg/core/input/mg_keyboard.h"
#include "mg/core/input/mg_mouse.h"
#include "mg/utils/mg_macros.h"

#include <cstdint>

namespace Mg::input {

enum class InputEvent {
    Press,
    Release,
};

/** InputTarget decides which button/mouse event handlers receive input events.
 *  Each input-poll has an associated target and only receivers with target equal to the current
 *  target receive events.
 *
 *  This can be used to let GUI overlays temporarily take over input from the main application.
 */
using InputTargetId = uint32_t;

/** Special case InputTargetId. Receivers with this target id receive all inputs. */
static constexpr InputTargetId InputTargetId_receive_all = 0;

static constexpr InputTargetId InputTargetId_game = 1;
static constexpr InputTargetId InputTargetId_editor = 2;
static constexpr InputTargetId InputTargetId_gui_overlay = 3;

class IButtonEventHandler {
public:
    MG_INTERFACE_BOILERPLATE(IButtonEventHandler);

    virtual void handle_key_event(InputTargetId target, Key key, InputEvent event) = 0;
    virtual void
    handle_mouse_button_event(InputTargetId target, MouseButton button, InputEvent event) = 0;
};

class IMouseMovementEventHandler {
public:
    MG_INTERFACE_BOILERPLATE(IMouseMovementEventHandler);

    virtual void handle_mouse_move_event(InputTargetId target,
                                         bool is_cursor_locked_to_window,
                                         float x,
                                         float y) = 0;
};

class IScrollEventHandler {
public:
    MG_INTERFACE_BOILERPLATE(IScrollEventHandler);

    virtual void handle_scroll_event(InputTargetId target, float xoffset, float yoffset) = 0;
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
