//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/input/mg_input.h"

#include <cmath>

#include <fmt/core.h>

#include "mg/core/mg_log.h"

namespace Mg::input {

std::string InputSource::description() const
{
    return m_device->description(m_id);
}

float InputSource::state() const
{
    return m_device->state(m_id);
}

void InputMap::bind(Identifier command, InputSource input_id)
{
    m_commands.insert_or_assign(command, input_id);
    m_command_states.insert_or_assign(command, State{ 0.0f, 0.0f });
}

void InputMap::unbind(Identifier command)
{
    if (auto it = m_commands.find(command); it != m_commands.end()) {
        m_commands.erase(it);
        return;
    }

    g_log.write_warning(
        fmt::format("InputMap::unbind(): "
                    "Attempting to clear binding for non-existing command '{}'",
                    command.c_str()));
}

InputSource InputMap::binding(Identifier command) const
{
    return m_commands.at(command);
}

std::vector<Identifier> InputMap::commands() const
{
    std::vector<Identifier> ret_val;
    ret_val.reserve(m_commands.size());

    for (auto&& p : m_commands) {
        ret_val.push_back(p.first);
    }

    return ret_val;
}

void InputMap::refresh()
{
    for (auto& [id, state] : m_command_states) {
        state.prev_state = state.current_state;
        state.current_state = m_commands.at(id).state();
    }
}

float InputMap::state(Identifier command) const
{
    return m_command_states.at(command).current_state;
}

float InputMap::prev_state(Identifier command) const
{
    return m_command_states.at(command).prev_state;
}

bool InputMap::is_held(Identifier command) const
{
    return std::abs(state(command)) >= k_is_pressed_threshold;
}

bool InputMap::was_pressed(Identifier command) const
{
    auto& state = m_command_states.at(command);
    return std::abs(state.prev_state) < k_is_pressed_threshold &&
           std::abs(state.current_state) >= k_is_pressed_threshold;
}

bool InputMap::was_released(Identifier command) const
{
    auto& state = m_command_states.at(command);
    return std::abs(state.prev_state) >= k_is_pressed_threshold &&
           std::abs(state.current_state) < k_is_pressed_threshold;
}

} // namespace Mg::input
