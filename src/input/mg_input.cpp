//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
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

struct InputMapData {
    std::unordered_map<Identifier, InputSource> commands;
    std::unordered_map<Identifier, State> command_states;
};

InputMap::InputMap() = default;

InputMap::~InputMap() = default;

void InputMap::bind(Identifier command, InputSource input_id)
{
    impl().commands.insert_or_assign(command, input_id);
    impl().command_states.insert_or_assign(command, State{ 0.0f, 0.0f });
}

void InputMap::unbind(Identifier command)
{
    if (auto it = impl().commands.find(command); it != impl().commands.end()) {
        impl().commands.erase(it);
        return;
    }

    log.warning(
        "InputMap::unbind(): "
        "Attempting to clear binding for non-existing command '{}'",
        command.str_view());
}

InputSource InputMap::binding(Identifier command) const
{
    return impl().commands.at(command);
}

std::vector<Identifier> InputMap::commands() const
{
    std::vector<Identifier> ret_val;
    ret_val.reserve(impl().commands.size());

    for (auto&& p : impl().commands) {
        ret_val.push_back(p.first);
    }

    return ret_val;
}

void InputMap::refresh()
{
    for (auto& [id, state] : impl().command_states) {
        state.prev_state = state.current_state;
        state.current_state = impl().commands.at(id).state();
    }
}

float InputMap::state(Identifier command) const
{
    return impl().command_states.at(command).current_state;
}

float InputMap::prev_state(Identifier command) const
{
    return impl().command_states.at(command).prev_state;
}

bool InputMap::is_held(Identifier command) const
{
    return std::abs(state(command)) >= k_is_pressed_threshold;
}

bool InputMap::was_pressed(Identifier command) const
{
    auto& state = impl().command_states.at(command);
    return std::abs(state.prev_state) < k_is_pressed_threshold &&
           std::abs(state.current_state) >= k_is_pressed_threshold;
}

bool InputMap::was_released(Identifier command) const
{
    auto& state = impl().command_states.at(command);
    return std::abs(state.prev_state) >= k_is_pressed_threshold &&
           std::abs(state.current_state) < k_is_pressed_threshold;
}

} // namespace Mg::input
