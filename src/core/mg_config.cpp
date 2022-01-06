//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/mg_config.h"

#include "mg/core/mg_identifier.h"
#include "mg/core/mg_log.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_file_io.h"
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

    explicit ConfigVariable(std::string_view key, double value) : ConfigVariable(key)
    {
        set(value);
    }

    explicit ConfigVariable(std::string_view key, std::string_view value) : ConfigVariable(key)
    {
        set(value);
    }

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

    std::string_view key() const noexcept { return m_key; }
    uint32_t key_hash() const noexcept { return m_key_hash; }
    const Value& value() const noexcept { return m_value; }

private:
    ConfigVariable(std::string_view key) : m_key(key), m_key_hash(hash_fnv1a(key)) {}

    std::string m_key;
    uint32_t m_key_hash;
    Value m_value;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// Config implementation
//--------------------------------------------------------------------------------------------------

Config::Config() = default;
Config::~Config() = default;

Config::Config(std::string_view filepath)
{
    read_from_file(filepath);
}

// Shared const and non-const implementation of Config::at.
template<typename CVarVector>
auto at_impl(std::string_view key, CVarVector& values) -> decltype(values.data())
{
    const auto key_hash = hash_fnv1a(key);

    auto it = find_if(values, [&](const ConfigVariable& cvar) {
        return cvar.key_hash() == key_hash && cvar.key() == key;
    });

    if (it == values.end()) {
        return nullptr;
    }

    return std::addressof(*it);
}

struct ConfigData {
    ConfigVariable* at(std::string_view key) { return at_impl(key, m_values); }
    const ConfigVariable* at(std::string_view key) const { return at_impl(key, m_values); }

    std::vector<ConfigVariable> m_values;
};

// Generic implementation of Config::set_default_value
template<typename T> void _set_default_value(ConfigData& impl, std::string_view key, const T& value)
{
    auto p_cvar = impl.at(key);
    if (p_cvar == nullptr) impl.m_values.emplace_back(key, value);
}

void Config::set_default_value(std::string_view key, std::string_view value)
{
    _set_default_value(impl(), key, value);
}

void Config::_set_default_value_numeric(std::string_view key, double value)
{
    _set_default_value(impl(), key, value);
}

// Generic implementation of Config::set_value
template<typename T> void _set_value(ConfigData& impl, std::string_view key, const T& value)
{
    auto p_cvar = impl.at(key);
    if (p_cvar != nullptr) {
        p_cvar->set(value);
    }
    else {
        impl.m_values.emplace_back(key, value);
    }
}

void Config::set_value(std::string_view key, std::string_view value)
{
    _set_value(impl(), key, value);
}

void Config::_set_value_numeric(std::string_view key, double value)
{
    _set_value(impl(), key, value);
}

template<typename NumT> NumT Config::as(std::string_view key) const
{
    const auto& v = impl().at(key);
    MG_ASSERT(v != nullptr);

    if constexpr (std::is_integral_v<NumT>) {
        return round<NumT>(v->value().numeric);
    }

    if constexpr (std::is_floating_point_v<NumT>) {
        return static_cast<NumT>(v->value().numeric);
    }
}

// Explicit instantiations for numeric types
template bool Config::as<bool>(std::string_view) const;
template int8_t Config::as<int8_t>(std::string_view) const;
template int16_t Config::as<int16_t>(std::string_view) const;
template int32_t Config::as<int32_t>(std::string_view) const;
template int64_t Config::as<int64_t>(std::string_view) const;
template uint8_t Config::as<uint8_t>(std::string_view) const;
template uint16_t Config::as<uint16_t>(std::string_view) const;
template uint32_t Config::as<uint32_t>(std::string_view) const;
template uint64_t Config::as<uint64_t>(std::string_view) const;
template float Config::as<float>(std::string_view) const;
template double Config::as<double>(std::string_view) const;

std::string_view Config::as_string(std::string_view key) const
{
    auto p_cvar = impl().at(key);
    MG_ASSERT(p_cvar != nullptr);
    return p_cvar->value().string;
}

std::string Config::assignment_line(std::string_view key) const
{
    auto p_cvar = impl().at(key);
    MG_ASSERT(p_cvar != nullptr);

    std::string value = p_cvar->value().string;

    // If value is not numeric, add quotation marks
    if (!string_to<double>(value).first) {
        value = "\"" + value + "\"";
    }

    // Format output line
    return fmt::format("{} = {}", key, value);
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
    const auto size = narrow<size_t>(find(input, '#') - input.begin());
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

    set_value(key, value);

    return true;
}

void Config::read_from_file(std::string_view filepath)
{
    log.verbose("Reading config file '{}'", filepath);

    Opt<std::ifstream> reader = io::make_input_filestream(filepath, io::Mode::text);

    if (!reader) {
        log.warning("Could not read config file '{}'.", filepath);
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
        Opt<std::ifstream> reader = io::make_input_filestream(filepath, io::Mode::text);

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
    for (auto& cvar : impl().m_values) {
        new_lines.emplace_back(assignment_line(cvar.key()));
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
    Opt<std::ofstream> writer = io::make_output_filestream(filepath, true, io::Mode::text);

    if (!writer) {
        log.error("Error writing config file {}", filepath);
        return;
    }

    for (auto& l : lines) {
        io::write_line(*writer, l.text);
    }
}

} // namespace Mg
