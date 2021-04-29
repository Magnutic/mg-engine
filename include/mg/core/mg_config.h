//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_config.h
 * Configuration system, handles strings and numeric values.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "mg/core/mg_identifier.h"
#include "mg/utils/mg_assert.h"

namespace Mg {

namespace detail {

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

    void set(std::string_view value);
    void set(double value);

    std::string_view key() const noexcept { return m_key; }
    uint32_t key_hash() const noexcept { return m_key_hash; }
    const Value& value() const noexcept { return m_value; }

private:
    ConfigVariable(std::string_view key) : m_key(key), m_key_hash(hash_fnv1a(key)) {}

    std::string m_key;
    uint32_t m_key_hash;
    Value m_value;
};

} // namespace detail

/** Config holds a collection of dynamically-typed configuration variables.
 * Configuration variables can be used as strings or numeric types.
 *
 * Config attempts to automatically convert between string and numeric values, but if conversion
 * from string to numeric fails, reading the value as numeric results in 0.
 */
class Config {
public:
    Config() = default;

    explicit Config(std::string_view filepath) { read_from_file(filepath); }

    ~Config() = default;

    // Disable copying, avoids risk of bugs caused by accidental copies.
    MG_MAKE_NON_COPYABLE(Config);
    MG_MAKE_NON_MOVABLE(Config);

    /** Set the default value for the given key.
     * This only sets the value if a value for the given key does not already exist.
     */
    void set_default_value(std::string_view key, std::string_view value)
    {
        const auto* p_cvar = at(key);
        if (p_cvar == nullptr) m_values.emplace_back(key, value);
    }

    /** Set the default value for the given key.
     * This only sets the value if a value for the given key does not already exist.
     */
    template<typename NumT, typename = std::enable_if_t<std::is_arithmetic_v<NumT>>>
    void set_default_value(std::string_view key, NumT value)
    {
        const auto* p_cvar = at(key);
        if (p_cvar == nullptr) m_values.emplace_back(key, static_cast<double>(value));
    }

    /** Set the current value for the given key to string value. */
    void set_value(std::string_view key, std::string_view value) { _set_value(key, value); }

    /** Set the current value for the given key to numeric value. */
    template<typename NumT, typename = std::enable_if_t<std::is_arithmetic_v<NumT>>>
    void set_value(std::string_view key, NumT value)
    {
        _set_value(key, static_cast<double>(value));
    }

    /** Get configuration variable as numeric, e.g. `as<int32_t>("key_name")`
     * @return Numeric value of the variable (rounded to nearest, if integral).
     */
    template<typename T> T as(std::string_view key) const;

    /** Get configuration variable as string. */
    std::string_view as_string(std::string_view key) const
    {
        auto p_cvar = at(key);
        MG_ASSERT(p_cvar != nullptr);
        return p_cvar->value().string;
    }

    /** Get configuration value assignment line, as used by `evaluate_line`. */
    std::string assignment_line(std::string_view key) const;

    /** Evaluates a config assignment line, taken from file or console.
     * The syntax is as follows:
     *
     * \s*(key)\s*=\s*"?(value)"?\s*
     *
     * Key and value may be anything, but key cannot contain any whitespace, and value must be
     * "-enclosed if it contains whitespace.
     *
     * The '#' character marks the beginning of a comment.
     *
     * @return Whether assignment was successful (i.e. line was well-formed, CVar with name
     * matching the key was found and set).
     */
    bool evaluate_line(std::string_view input);

    /** Reads from the given config file. */
    void read_from_file(std::string_view filepath);

    /** Writes to the given config file. */
    void write_to_file(std::string_view filepath) const;

private:
    // Generic implementation of set_value
    template<typename T> void _set_value(std::string_view key, const T& value)
    {
        auto p_cvar = at(key);
        if (p_cvar != nullptr) {
            p_cvar->set(value);
        }
        else {
            m_values.emplace_back(key, value);
        }
    }

    detail::ConfigVariable* at(std::string_view key);
    const detail::ConfigVariable* at(std::string_view key) const;

    std::vector<detail::ConfigVariable> m_values;
};

} // namespace Mg
