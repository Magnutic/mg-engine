//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2024, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_input.h
 * Generic input handling via binding input sources to command identifiers.
 */

#pragma once

#include "mg/containers/mg_flat_map.h"
#include "mg/core/mg_identifier.h"
#include "mg/input/mg_input_source.h"
#include "mg/input/mg_keyboard.h"
#include "mg/input/mg_mouse.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_optional.h"

#include <glm/vec2.hpp>

namespace Mg::input {

struct ButtonState {
    bool was_pressed = false;
    bool was_released = false;
    bool is_held = false;
};

class ButtonTracker : public IButtonEventHandler {
public:
    using ButtonStates = FlatMap<Identifier, ButtonState, Identifier::HashCompare>;

    explicit ButtonTracker(IInputSource& input_source);
    ~ButtonTracker() override;

    MG_MAKE_NON_MOVABLE(ButtonTracker);
    MG_MAKE_NON_COPYABLE(ButtonTracker);

    // Implementation of IButtonEventHandler.
    // The input source calls these functions to notify ButtonTracker of button events.
    void handle_key_event(Key key, InputEvent event) override;
    void handle_mouse_button_event(MouseButton button, InputEvent event) override;

    void bind(Identifier button_action_id, Key key, bool overwrite = true);
    void bind(Identifier button_action_id, MouseButton button, bool overwrite = true);

    // Get button events for each binding since last call to this function.
    [[nodiscard]] ButtonStates get_button_events()
    {
        ButtonStates result = m_states;

        for (auto& [id, state] : m_states) {
            state.was_pressed = false;
            state.was_released = false;
        }

        return result;
    }

private:
    IInputSource& m_input_source;
    ButtonStates m_states;
    FlatMap<MouseButton, Identifier> m_mouse_button_bindings;
    FlatMap<Key, Identifier> m_key_bindings;
};

class MouseMovementTracker : public IMouseMovementEventHandler {
public:
    explicit MouseMovementTracker(IInputSource& input_source);
    ~MouseMovementTracker() override;

    MG_MAKE_NON_MOVABLE(MouseMovementTracker);
    MG_MAKE_NON_COPYABLE(MouseMovementTracker);

    // Implementation of IMouseMovementEventHandler.
    // The input source calls these functions to notify MouseMovementTracker of input events.
    void handle_mouse_move_event(float x, float y, bool is_cursor_locked_to_window) override;

    /** Get mouse cursor position in screen coordinates, relative to upper left corner of the
     * window.
     */
    glm::vec2 mouse_cursor_position() const { return m_cursor_position; }

    /** Get difference in cursor position since last call to this function.
     * Returns delta in units of screen coordinates.
     */
    glm::vec2 mouse_delta() { return std::exchange(m_cursor_delta, { 0.0f, 0.0f }); }

private:
    IInputSource& m_input_source;
    glm::vec2 m_cursor_position = { 0.0f, 0.0f };
    glm::vec2 m_cursor_delta = { 0.0f, 0.0f };
};

} // namespace Mg::input
