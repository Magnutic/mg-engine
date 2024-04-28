//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/mg_config.h"

#include "mg/containers/mg_flat_map.h"
#include "mg/core/mg_identifier.h"
#include "mg/core/mg_log.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_file_io.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_optional.h"
#include "mg/utils/mg_stl_helpers.h"
#include "mg/utils/mg_string_utils.h"

#include <fmt/core.h>

#include <string>
#include <vector>

namespace Mg {

//--------------------------------------------------------------------------------------------------
// ConfigVariable
//--------------------------------------------------------------------------------------------------

namespace {

double numeric_from_string(std::string_view string)
{
    const auto [success, value] = string_to<double>(string);
    if (success) {
        return value;
    }
    return (to_lower(string) == "true") ? 1.0 : 0.0;
}

/** Internal storage type for configuration values. */
class ConfigVariable {
public:
    struct Value {
        std::string string;
        double numeric = 0.0;
    };

    void set(std::string_view value)
    {
        m_value.string = std::string(value);
        m_value.numeric = numeric_from_string(value);
    }

    void set(double value)
    {
        m_value.string = fmt::format("{}", value);
        m_value.numeric = value;
    }

    const Value& value() const noexcept { return m_value; }

private:
    Value m_value;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// Config implementation
//--------------------------------------------------------------------------------------------------

using ConfigVariablesMap = FlatMap<Identifier, ConfigVariable, Identifier::HashCompare>;

Config::Config() = default;
Config::~Config() = default;

Config::Config(std::string_view filepath)
{
    read_from_file(filepath);
}

struct Config::Impl {
    ConfigVariable* at(Identifier key)
    {
        auto it = m_values.find(key);
        return (it == m_values.end()) ? nullptr : std::addressof(it->second);
    }

    const ConfigVariable* at(Identifier key) const
    {
        auto it = m_values.find(key);
        return (it == m_values.end()) ? nullptr : std::addressof(it->second);
    }

    ConfigVariablesMap m_values;
};

// Generic implementation of Config::set_default_value
template<typename T> void _set_default_value(Config::Impl& impl, Identifier key, const T& value)
{
    auto p_cvar = impl.at(key);
    if (p_cvar == nullptr) impl.m_values[key].set(value);
}

void Config::set_default_value(Identifier key, std::string_view value)
{
    _set_default_value(*m_impl, key, value);
}

void Config::_set_default_value_numeric(Identifier key, double value)
{
    _set_default_value(*m_impl, key, value);
}

// Generic implementation of Config::set_value
template<typename T> void _set_value(Config::Impl& impl, Identifier key, const T& value)
{
    auto p_cvar = impl.at(key);
    if (p_cvar != nullptr) {
        p_cvar->set(value);
    }
    else {
        impl.m_values[key].set(value);
    }
}

void Config::set_value(Identifier key, std::string_view value)
{
    _set_value(*m_impl, key, value);
}

void Config::_set_value_numeric(Identifier key, double value)
{
    _set_value(*m_impl, key, value);
}

template<typename NumT> NumT Config::as(Identifier key) const
{
    const auto& v = m_impl->at(key);
    MG_ASSERT(v != nullptr);

    if constexpr (std::is_integral_v<NumT>) {
        return round<NumT>(v->value().numeric);
    }

    if constexpr (std::is_floating_point_v<NumT>) {
        return static_cast<NumT>(v->value().numeric);
    }
}

// Explicit instantiations for numeric types
template bool Config::as<bool>(Identifier) const;
template int8_t Config::as<int8_t>(Identifier) const;
template int16_t Config::as<int16_t>(Identifier) const;
template int32_t Config::as<int32_t>(Identifier) const;
template int64_t Config::as<int64_t>(Identifier) const;
template uint8_t Config::as<uint8_t>(Identifier) const;
template uint16_t Config::as<uint16_t>(Identifier) const;
template uint32_t Config::as<uint32_t>(Identifier) const;
template uint64_t Config::as<uint64_t>(Identifier) const;
template float Config::as<float>(Identifier) const;
template double Config::as<double>(Identifier) const;

std::string_view Config::as_string(Identifier key) const
{
    auto p_cvar = m_impl->at(key);
    MG_ASSERT(p_cvar != nullptr);
    return p_cvar->value().string;
}

std::string Config::format_assignment_line(Identifier key) const
{
    auto p_cvar = m_impl->at(key);
    MG_ASSERT(p_cvar != nullptr);

    std::string value = p_cvar->value().string;

    // If value is not numeric, add quotation marks
    if (!string_to<double>(value).first) {
        value = "\"" + value + "\"";
    }

    // Format output line
    return fmt::format("{} = {}", key.str_view(), value);
}

bool Config::evaluate_line(std::string_view input)
{
    const auto error = [&](std::string_view reason) {
        log.warning("Failed to parse config assignment:\n\t'{}'\n\t{}\n\t{}",
                    input,
                    reason,
                    "Assignments must be of the form 'key = value'");
    };

    // Ignore comment
    const auto size = Mg::as<size_t>(find(input, '#') - input.begin());
    input = trim(input.substr(0, size));

    if (input.empty()) {
        return true;
    }

    auto [key, rhs] = split_string_on_char(input, '=');

    key = trim(key);
    rhs = trim(rhs);

    if (key.empty() || find_any_of(key, k_white_space) != std::string::npos) {
        error("Malformed key.");
        return false;
    }

    std::string value;

    // If value is "-enclosed, parse it as string literal
    if (rhs.size() > 1 && rhs[0] == '"' && rhs[rhs.size() - 1] == '"') {
        bool escape = false;
        for (const char c : rhs.substr(1, rhs.size() - 2)) {
            if (c == '\\' && !escape) {
                escape = true;
                continue;
            }
            if (c == '"' && !escape) {
                return false;
            }

            value += c;
            escape = false;
        }
    }
    else {
        if (rhs.empty() || find_any_of(value, " \t\r\n\v\f\"") != std::string::npos) {
            error("Malformed value");
            return false;
        }

        value = std::string(rhs);
    }

    set_value(Identifier::from_runtime_string(key), value);

    return true;
}

void Config::read_from_file(std::string_view filepath)
{
    log.verbose("Reading config file '{}'", filepath);

    auto [reader, error_msg] = io::make_input_filestream(filepath, io::Mode::text);

    if (!reader) {
        log.warning("Could not read config file '{}': {}", filepath, error_msg);
        return;
    }

    while (reader->good()) {
        auto line = io::get_line(*reader);
        evaluate_line(line);
    }
}

namespace {

// Helper class for write_to_file, models a configuration-value assignment line.
struct Line {
    static auto init_key(std::string_view line) noexcept
    {
        const auto split = split_string_on_char(line, '#');
        const auto assignment = split_string_on_char(split.first, '=');
        return trim(assignment.first);
    }

    Line(std::string_view line) : key{ init_key(line) }, text{ std::string(line) }
    {
        key_hash = hash_fnv1a(key);
    }

    friend bool operator==(const Line& l, const Line& r) noexcept { return l.key == r.key; }
    friend bool operator<(const Line& l, const Line& r) noexcept { return l.text < r.text; }

    uint32_t key_hash;
    std::string key;
    std::string text;
};

} // namespace

void Config::write_to_file(std::string_view filepath) const
{
    log.verbose("Writing config file '{}'", filepath);

    std::vector<Line> lines;

    {
        // Read in old lines from config file if it exists
        auto [reader, error_msg] = io::make_input_filestream(filepath, io::Mode::text);

        while (reader && reader->good()) {
            lines.emplace_back(io::get_line(*reader));
        }
    }

    // Remove blank lines at end of file
    while (!lines.empty() && lines.back().text.empty()) {
        lines.erase(lines.end() - 1);
    }

    std::vector<Line> new_lines;

    // Format new config lines
    for (const auto& [key, value] : m_impl->m_values) {
        new_lines.emplace_back(format_assignment_line(key));
    }

    std::sort(new_lines.begin(), new_lines.end());

    for (auto& line : new_lines) {
        auto equivalent = find(lines, line);

        // Replace old lines that have equivalent keys
        if (equivalent != lines.end()) {
            const std::string& old_line = equivalent->text;

            // While preserving comments
            const auto comment_begin = old_line.find('#');
            if (comment_begin != std::string::npos) {
                line.text.append(" ").append(old_line.substr(comment_begin));
            }

            *equivalent = std::move(line);
        }
        else {
            lines.push_back(line);
        }
    }

    // Write result to file
    auto [writer, error_msg] = io::make_output_filestream(filepath, true, io::Mode::text);

    if (!writer) {
        log.error("Error writing config file '{}': {}", filepath, error_msg);
        return;
    }

    for (auto& l : lines) {
        io::write_line(*writer, l.text);
    }
}

} // namespace Mg
