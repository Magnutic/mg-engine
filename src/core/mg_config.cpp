//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
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

#include "mg/core/mg_config.h"

#include <sstream>

#include <fmt/core.h>

#include "mg/core/mg_log.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_stl_helpers.h"
#include "mg/utils/mg_string_utils.h"
#include "mg/utils/mg_text_file_io.h"

namespace Mg {

static double numeric_from_string(std::string_view string)
{
    auto [success, value] = string_to<double>(string);

    if (success) { return value; }

    auto low_str = to_lower(string);

    if (low_str == "true") { return 1.0; }

    return 0.0;
}

void detail::ConfigVariable::set(std::string_view value)
{
    m_value.string  = std::string(value);
    m_value.numeric = numeric_from_string(value);
}

void detail::ConfigVariable::set(double value)
{
    std::stringstream ss;
    ss << value;

    m_value.string  = ss.str();
    m_value.numeric = value;
}

//--------------------------------------------------------------------------------------------------

template<typename T> T Config::as(std::string_view key) const
{
    const auto& v = at(key);
    MG_ASSERT(v != nullptr);

    if constexpr (std::is_integral_v<T>) { return round<T>(v->value().numeric); }

    if constexpr (std::is_floating_point_v<T>) { return static_cast<T>(v->value().numeric); }
}

// Explicit instantiations for numeric types
template bool     Config::as<bool>(std::string_view) const;
template int8_t   Config::as<int8_t>(std::string_view) const;
template int16_t  Config::as<int16_t>(std::string_view) const;
template int32_t  Config::as<int32_t>(std::string_view) const;
template int64_t  Config::as<int64_t>(std::string_view) const;
template uint8_t  Config::as<uint8_t>(std::string_view) const;
template uint16_t Config::as<uint16_t>(std::string_view) const;
template uint32_t Config::as<uint32_t>(std::string_view) const;
template uint64_t Config::as<uint64_t>(std::string_view) const;
template float    Config::as<float>(std::string_view) const;
template double   Config::as<double>(std::string_view) const;

std::string Config::assignment_line(std::string_view key) const
{
    auto p_cvar = at(key);
    MG_ASSERT(p_cvar != nullptr);

    std::string value = p_cvar->value().string;

    // If value is not numeric, add quotation marks
    if (!string_to<double>(value).first) {
        std::ostringstream ss;
        ss << "\"" << value << "\"";
        value = ss.str();
    }

    // Format output line
    return fmt::format("{} = {}", key, value);
}

bool Config::evaluate_line(std::string_view input)
{
    auto error = [&](std::string_view reason) {
        g_log.write_message(fmt::format("Failed to parse config assignment:\n\t'{}'\n\t{}\n\t{}",
                                        input,
                                        reason,
                                        "Assignments must be of the form 'key = value'"));
    };

    // Ignore comment
    auto size = size_t(find(input, '#') - input.begin());
    input     = trim(input.substr(0, size));

    if (input.empty()) { return true; }

    auto [key, rhs] = split_string_on_char(input, '=');

    key = trim(key);
    rhs = trim(rhs);

    if (find_any_of(key, k_white_space) != std::string::npos) {
        error("Malformed key.");
        return false;
    }

    std::string value;

    // If value is "-enclosed, parse it as string literal
    if (rhs.size() > 1 && rhs[0] == '"' && rhs[rhs.size() - 1] == '"') {
        bool escape = false;
        for (char c : rhs.substr(1, rhs.size() - 2)) {
            if (c == '\\' && !escape) {
                escape = true;
                continue;
            }
            if (c == '"' && !escape) { return false; }

            value += c;
            escape = false;
        }
    }
    else {
        if (find_any_of(value, " \t\r\n\v\f\"") != std::string::npos) {
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
    g_log.write_verbose(fmt::format("Reading config file '{}'", filepath));

    std::optional<std::ifstream> reader = io::make_input_filestream(filepath);

    if (!reader) {
        g_log.write_warning(fmt::format("Could not read config file '{}'.", filepath));
        return;
    }

    while (reader->good()) {
        auto line = io::get_line(*reader);
        evaluate_line(line);
    }
}

// Helper class for write_to_file, models a configuration value assignment line.
struct Line {
    static auto init_key(std::string_view line)
    {
        auto split      = split_string_on_char(line, '#');
        auto assignment = split_string_on_char(split.first, '=');
        return trim(assignment.first);
    }

    Line(std::string_view line) : key{ init_key(line) }, text{ std::string(line) }
    {
        key_hash = hash_fnv1a(key);
    }

    uint32_t    key_hash;
    std::string key;
    std::string text;
};

bool operator==(const Line& l, const Line& r)
{
    return l.key == r.key;
}
bool operator<(const Line& l, const Line& r)
{
    return l.text < r.text;
}

void Config::write_to_file(std::string_view filepath) const
{
    g_log.write_verbose(fmt::format("Writing config file '{}'", filepath));

    std::vector<Line> lines;

    {
        // Read in old lines from config file if it exists
        std::optional<std::ifstream> reader = io::make_input_filestream(filepath);

        while (reader && reader->good()) { lines.emplace_back(io::get_line(*reader)); }
    }

    // Remove blank lines at end of file
    while (!lines.empty() && lines.back().text.empty()) { lines.erase(lines.end() - 1); }

    std::vector<Line> new_lines;

    // Format new config lines
    for (auto& cvar : m_values) { new_lines.emplace_back(assignment_line(cvar.key())); }

    std::sort(new_lines.begin(), new_lines.end());

    for (auto& line : new_lines) {
        auto equivalent = find(lines, line);

        // Replace old lines that have equivalent keys
        if (equivalent != lines.end()) {
            std::string& old_line = equivalent->text;

            // While preserving comments
            auto comment_begin = old_line.find('#');
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
    std::optional<std::ofstream> writer = io::make_output_filestream(filepath, true);

    if (!writer) {
        g_log.write_error(fmt::format("Error writing config file {}", filepath));
        return;
    }

    for (auto& l : lines) { io::write_line(*writer, l.text); }
}

// Shared const and non-const implementation of Config::at.
template<typename CVarVector>
auto at_impl(std::string_view key, CVarVector& values) -> decltype(values.data())
{
    const auto key_hash = hash_fnv1a(key);

    auto it = find_if(values, [&](const detail::ConfigVariable& cvar) {
        return cvar.key_hash() == key_hash && cvar.key() == key;
    });

    if (it == values.end()) { return nullptr; }

    return std::addressof(*it);
}

detail::ConfigVariable* Config::at(std::string_view key)
{
    return at_impl(key, m_values);
}

const detail::ConfigVariable* Config::at(std::string_view key) const
{
    return at_impl(key, m_values);
}

} // namespace Mg
