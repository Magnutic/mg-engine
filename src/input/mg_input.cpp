//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/input/mg_input.h"

#include "mg/core/mg_log.h"

#include <cmath>
#include <unordered_map>

namespace Mg::input {

namespace {

struct State {
    float largest_diff = 0.0f;
    float current_state = 0.0f;
    float prev_state = 0.0f;
};

} // namespace

std::string InputSource::description() const
{
    return m_device->description(m_id);
}

float InputSource::state() const
{
    return m_device->state(m_id);
}

struct InputMap::Impl {
    std::unordered_map<Identifier, InputSource> commands;
    std::unordered_map<Identifier, State> command_states;
};

InputMap::InputMap() = default;

InputMap::~InputMap() = default;

void InputMap::bind(Identifier command, InputSource input_id)
{
    m_impl->commands.insert_or_assign(command, input_id);
    m_impl->command_states.insert_or_assign(command, State{ 0.0f, 0.0f });
}

void InputMap::unbind(Identifier command)
{
    if (auto it = m_impl->commands.find(command); it != m_impl->commands.end()) {
        m_impl->commands.erase(it);
        return;
    }

    log.warning(
        "InputMap::unbind(): "
        "Attempting to clear binding for non-existing command '{}'",
        command.str_view());
}

InputSource InputMap::binding(Identifier command) const
{
    return m_impl->commands.at(command);
}

std::vector<Identifier> InputMap::commands() const
{
    std::vector<Identifier> ret_val;
    ret_val.reserve(m_impl->commands.size());

    for (auto&& p : m_impl->commands) {
        ret_val.push_back(p.first);
    }

    return ret_val;
}

void InputMap::update()
{
    refresh();
    for (auto& [id, state] : m_impl->command_states) {
        state.prev_state = state.current_state;
        state.current_state = m_impl->commands.at(id).state();

        // Check for missed states B when going from A -> B -> A in one update.
        // (This can only happen if `refresh` was called between `update`s).
        const bool missed_state_change = state.prev_state == state.current_state &&
                                         state.largest_diff != 0.0f;
        if (missed_state_change) {
            state.current_state = state.largest_diff;
        }

        state.largest_diff = 0.0f;
    }
}

void InputMap::refresh()
{
    for (auto& [id, state] : m_impl->command_states) {
        const float new_state = m_impl->commands.at(id).state();
        if (std::abs(new_state - state.current_state) > state.largest_diff) {
            state.largest_diff = new_state;
        }
    }
}

float InputMap::state(Identifier command) const
{
    return m_impl->command_states.at(command).current_state;
}

float InputMap::prev_state(Identifier command) const
{
    return m_impl->command_states.at(command).prev_state;
}

bool InputMap::is_held(Identifier command) const
{
    return std::abs(state(command)) >= k_is_pressed_threshold;
}

bool InputMap::was_pressed(Identifier command) const
{
    auto& state = m_impl->command_states.at(command);
    return std::abs(state.prev_state) < k_is_pressed_threshold &&
           std::abs(state.current_state) >= k_is_pressed_threshold;
}

bool InputMap::was_released(Identifier command) const
{
    auto& state = m_impl->command_states.at(command);
    return std::abs(state.prev_state) >= k_is_pressed_threshold &&
           std::abs(state.current_state) < k_is_pressed_threshold;
}

} // namespace Mg::input
