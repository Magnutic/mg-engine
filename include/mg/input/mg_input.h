//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_input.h
 * Generic input handling via binding input sources to command identifiers.
 */

#pragma once

#include <vector>

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

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

    explicit InputSource(const IInputDevice& dev, InputSource::Id id) noexcept
        : m_device(&dev), m_id(id)
    {}

    std::string description() const;
    float state() const;

private:
    const IInputDevice* m_device;
    InputSource::Id m_id;
};

//--------------------------------------------------------------------------------------------------

/** Generic interface for all kinds of input devices: keyboard, mouse, joystick / gamepad, etc. */
class IInputDevice {
    friend class InputSource;

public:
    MG_INTERFACE_BOILERPLATE(IInputDevice);

protected:
    virtual float state(InputSource::Id) const = 0;
    virtual std::string description(InputSource::Id) const = 0;
};

//--------------------------------------------------------------------------------------------------

struct InputMapData;

/** InputMap provides a mapping from a set of Identifiers to InputSources. */
class InputMap : PImplMixin<InputMapData> {
public:
    explicit InputMap();
    ~InputMap();

    MG_MAKE_NON_COPYABLE(InputMap);
    MG_MAKE_DEFAULT_MOVABLE(InputMap);

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
};

} // namespace Mg::input
