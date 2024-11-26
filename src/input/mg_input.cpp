//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/input/mg_input.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/core/mg_window.h"

namespace Mg::input {

namespace {

void update_button_state(ButtonState& state, const InputEvent event)
{
    if (event == InputEvent::Press) {
        state.is_held = true;
        state.was_pressed = true;
    }
    else {
        state.is_held = false;
        state.was_released = true;
    }
}

} // namespace


ButtonTracker::ButtonTracker(IInputSource& input_source) : m_input_source{ input_source }
{
    m_input_source.register_button_event_handler(*this);
}

ButtonTracker::~ButtonTracker()
{
    m_input_source.deregister_button_event_handler(*this);
}

void ButtonTracker::handle_key_event(const Key key, const InputEvent event)
{
    if (auto it = m_key_bindings.find(key); it != m_key_bindings.end()) {
        for (const auto& action_id : it->second) {
            update_button_state(m_states[action_id], event);
        }
    }
}

void ButtonTracker::handle_mouse_button_event(const MouseButton button, const InputEvent event)
{
    if (auto it = m_mouse_button_bindings.find(button); it != m_mouse_button_bindings.end()) {
        for (const auto& action_id : it->second) {
            update_button_state(m_states[action_id], event);
        }
    }
}

void ButtonTracker::bind(Identifier button_action_id, Key key)
{
    m_key_bindings[key].push_back(button_action_id);
    m_states[button_action_id] = {};
}

void ButtonTracker::bind(Identifier button_action_id, MouseButton button)
{
    m_mouse_button_bindings[button].push_back(button_action_id);
    m_states[button_action_id] = {};
}

MouseMovementTracker::MouseMovementTracker(IInputSource& input_source)
    : m_input_source{ input_source }
{
    m_input_source.register_mouse_movement_event_handler(*this);
}

MouseMovementTracker::~MouseMovementTracker()
{
    m_input_source.deregister_mouse_movement_event_handler(*this);
}

void MouseMovementTracker::handle_mouse_move_event(const float x,
                                                   const float y,
                                                   const bool is_cursor_locked_to_window)
{
    m_cursor_delta = { m_cursor_delta.x + x - m_cursor_position.x,
                       m_cursor_delta.y + y - m_cursor_position.y };
    m_cursor_position = { x, y };

    if (!is_cursor_locked_to_window) {
        m_cursor_delta = { 0.0f, 0.0f };
    }
}

ScrollTracker::ScrollTracker(IInputSource& input_source) : m_input_source{ input_source }
{
    m_input_source.register_scroll_event_handler(*this);
}

ScrollTracker::~ScrollTracker()
{
    m_input_source.deregister_scroll_event_handler(*this);
}

void ScrollTracker::handle_scroll_event(const float xoffset, const float yoffset)
{
    m_scroll_delta = { xoffset, yoffset };
}

} // namespace Mg::input
