//**************************************************************************************************
// Mg Engine
//-------------------------------------------------------------------------------
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

/** @file mg_input.h
 * Generic input handling via binding input sources to command identifiers.
 */

#pragma once

#include <unordered_map>
#include <vector>

#include <mg/core/mg_identifier.h>
#include <mg/utils/mg_macros.h>

namespace Mg::input {

/** Threshold value above which (the absolute value of) InputSources' state is to be considered as
 * being pressed.
 */
constexpr float k_is_pressed_threshold = 0.5f;

class IInputDevice;

/** InputSource represents a particular source of input, e.g. a keyboard key, a mouse button or
 * axis, joystick axis, etc.
 */
class InputSource {
public:
    /** Identifier for an individual input source within an input device. How the value for
     * InputSource::Id is mapped to a specific such input source is internal to each IInputDevice
     * implementation.
     */
    using Id = uint32_t;

    InputSource(const IInputDevice& dev, InputSource::Id id) : m_device(&dev), m_id(id) {}

    std::string description() const;
    float       state() const;

private:
    const IInputDevice* m_device;
    InputSource::Id     m_id;
};

//--------------------------------------------------------------------------------------------------

/** Generic interface for all kinds of input devices: keyboard, mouse, joystick / gamepad, etc. */
class IInputDevice {
    friend class InputSource;

public:
    MG_INTERFACE_BOILERPLATE(IInputDevice);

protected:
    virtual float       state(InputSource::Id) const       = 0;
    virtual std::string description(InputSource::Id) const = 0;
};

//--------------------------------------------------------------------------------------------------

/** InputMap provides a mapping from a set of Identifiers to InputSources. */
class InputMap {
public:
    /** Bind command to the given input source. */
    void bind(Identifier command, InputSource input_id);

    void unbind(Identifier command);

    InputSource binding(Identifier command) const;

    /** Get a list of all commands. */
    std::vector<Identifier> commands() const;

    /** Checks input devices and updates state. Call this once per time step. */
    void refresh();

    /** Get current state of input command.
     * @return State of command as float. Keys and buttons return 1.0f if pressed, 0.0f otherwise.
     * Joystick axes return in the range [-1.0f, 1.0f].
     */
    float state(Identifier command) const;

    /** Get previous state of input command as float. Previous state refers to the state before the
     * last `InputSystem::refresh()` call.
     * @return Previous state of command as float. Keys and buttons return 1.0f if pressed, 0.0f
     * otherwise. Joystick axes return in the range [-1.0f, 1.0f].
     */
    float prev_state(Identifier command) const;

    /** Returns whether button assigned to identifier was just pressed. */
    bool was_pressed(Identifier command) const;

    /** Returns whether button assigned to identifier is currently held. */
    bool is_held(Identifier command) const;

    /** Returns whether button assigned to identifier was just released. */
    bool was_released(Identifier command) const;

private:
    struct State {
        float current_state = 0.0f;
        float prev_state    = 0.0f;
    };

    std::unordered_map<Identifier, InputSource> m_commands;
    std::unordered_map<Identifier, State>       m_command_states;
};

} // namespace Mg::input
