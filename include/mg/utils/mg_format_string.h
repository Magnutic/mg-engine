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

/** @file mg_format_string.h
 * Type-safe printf-style formatting.
 */

#pragma once

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace Mg {

/** Constructs a string from a printf() style call in a type safe manner.
 * Handles most common printf() formatting.
 * Differences from printf:
 * 1. Type specifiers are equivalent: output is handled by argument type.
 *    Consider using the appropriate specifier as a form of documentation.
 * 2. Length specifiers are ignored in favour of argument types.
 * 3. Works with any type T for which the std::ostream output operator << is
 *    defined.
 * 4. Unsupported: g, G, *width, *precision, n
 */
template<size_t N, typename T1, typename... Ts>
std::string format_string(const char (&str)[N], T1&& arg1, Ts&&... args);

/** Fast track for strings without formatting specifiers. */
template<size_t N> std::string format_string(const char (&str)[N]);

//--------------------------------------------------------------------------------------------------

// Implementation details
namespace detail {

/** Counts the number of formatting specifiers in str. */
template<size_t N> size_t num_specifiers(const char (&str)[N])
{
    size_t n = 0;

    for (size_t i = 0; i < N; ++i) {
        if (str[i] == '%') {
            if (str[i + 1] == '%') {
                ++i;
                continue;
            }

            ++n;
        }
    }

    return n;
}

template<typename T, typename FormatT, bool> struct FormatAsType {
    static void invoke(std::ostringstream& /* output */, const T& /* value */)
    {
        throw std::logic_error("Mg::format_string(): T is not convertible to FormatT");
    }
};

// Output value as type, implementation
template<typename T, typename FormatT> struct FormatAsType<T, FormatT, true> {
    static void invoke(std::ostringstream& output, const T& value)
    {
        output << static_cast<FormatT>(value);
    }
};

// Output value as type
template<typename T, typename FormatT>
void format_as_type(std::ostringstream& output, const T& value)
{
    FormatAsType<T, FormatT, std::is_convertible<T, FormatT>::value>::invoke(output, value);
}

// format_string helper base case
inline void format_helper(std::ostringstream& out, const char* str)
{
    bool skip_percent = true;

    while (char c = *str++) {
        if (c == '%') {
            skip_percent = !skip_percent;
            if (!skip_percent) {
                continue;
            }
        }

        out << c;
    }
}

// format_string helper
template<typename T, typename... Ts>
void format_helper(std::ostringstream& out, const char* s, const T& val, Ts&&... args)
{
    auto init_flags = out.flags();
    auto init_width = out.width();
    auto init_prec  = out.precision();
    auto init_fill  = out.fill();

    auto restore_state = [&] {
        out.flags(init_flags);
        out.width(init_width);
        out.precision(init_prec);
        out.fill(init_fill);
    };

    while (*s) {
        if (*s == '%' && *++s != '%') {
            // Format specifier
            bool c = false, p = false, check_next = true;

            // Flags
            while (check_next) {
                switch (*s) {
                case '-':
                    out << std::left;
                    ++s;
                    break;
                case '+':
                    out << std::showpos;
                    ++s;
                    break;
                case '0':
                    out << std::setfill('0');
                    ++s;
                    break;
                case '#':
                    out << std::showbase;
                    out << std::showpoint;
                    ++s;
                    break;
                default: check_next = false;
                }
            }

            auto read_number = [&]() {
                int32_t num = 0;

                while (*s >= '0' && *s <= '9') {
                    num = num * 10 + static_cast<int>(*s++ - '0');
                }

                return num;
            };

            // Width
            auto width = read_number();
            if (width > 0) {
                out << std::setw(width);
            }

            // Precision
            if (*s == '.') {
                ++s;

                auto precision = read_number();
                out << std::setprecision(precision);
            }

            // Length (ignore)
            while (*s == 'l' || *s == 'h' || *s == 'j' || *s == 'z' || *s == 't' || *s == 'L') ++s;

            if (std::is_floating_point<T>::value) {
                out << std::fixed;
            }

            // Specifier
            switch (*s) {
            case 'f':
            case 'F':
            case 'd':
            case 'i':
            case 's':
            case 'u': break;
            case 'c': c = true; break;
            case 'p': p = true; break;
            case 'o': out << std::oct; break;
            case 'X':
                out << std::uppercase; // fallthrough
            case 'x': out << std::hex; break;
            case 'E':
                out << std::uppercase; // fallthrough
            case 'e': out << std::scientific; break;
            case 'A':
                out << std::uppercase; // fallthrough
            case 'a': out << std::hexfloat; break;
            default:
                std::ostringstream oss;
                oss << "\n_unsupported specifier: " << *s << "\n";
                throw std::logic_error(oss.str());
            }

            // Convert chars and pointers for c and p specifiers
            if (c && std::is_convertible<T, char>::value) {
                format_as_type<T, char>(out, val);
            }
            else if (p && std::is_convertible<T, const void*>::value) {
                format_as_type<T, const void*>(out, val);
            }
            else {
                out << val;
            }

            // Restore stream format state before moving on to next specifier.
            restore_state();
            format_helper(out, ++s, args...);
            return;
        }

        out << *s++;
    }
}

} // namespace detail

template<size_t N, typename T1, typename... Ts>
std::string format_string(const char (&str)[N], T1&& arg1, Ts&&... args)
{
    if (detail::num_specifiers(str) != sizeof...(args) + 1) {
        throw std::logic_error(
            "The number of formatting specifiers does not match the number of "
            "arguments provided.");
    }

    std::ostringstream output;

    detail::format_helper(output, str, std::forward<T1>(arg1), std::forward<Ts>(args)...);

    return { output.str() };
}

// Fast track for calls without arguments and specifiers
template<size_t N> std::string format_string(const char (&str)[N])
{
    if (detail::num_specifiers(str) != 0) {
        throw std::logic_error(
            "String contains formatting specifiers but no arguments were provided.");
    }

    std::string out_str;
    out_str.reserve(N);

    // We still need to deal with '%%'
    bool was_percent = false;
    for (char c : str) {
        if (c == '\0') {
            break;
        }
        if (c == '%' && !was_percent) {
            was_percent = true;
            continue;
        }
        out_str += c;
        was_percent = false;
    }

    return out_str;
}

} // namespace Mg
