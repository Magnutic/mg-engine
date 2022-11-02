//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_enum_map.h
 * Map from enum to value.
 */

#pragma once

#include "mg/containers/mg_small_vector.h"

#include <type_traits>
#include <utility>

#define MG_DEFINE_ENUM(enum_type_name, ...) enum class enum_type_name { __VA_ARGS__, _NumValues };

namespace Mg {

template<typename EnumT> struct NumEnumElements {
    static constexpr size_t value = static_cast<size_t>(EnumT::_NumValues);
};

/** Map from enumeration value to value of type T. Requires that the enumeration type has a value
 * named _NumValues whose value corresponds to the number of enumeration values in the type
 * (excluding _NumValues itself). This will be handled automatically if MG_DEFINE_ENUM is used to
 * define the enumeration type.
 */
template<typename EnumT, typename T> class EnumMap {
public:
    static_assert(std::is_enum_v<EnumT>);

    using value_type = std::pair<EnumT, T>;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = value_type*;
    using const_iterator = const value_type*;

    T& operator[](EnumT key) // requires(std::default_initializable<T>)
    {
        pointer element = find(key);
        if (!element) {
            m_map.emplace_back(std::piecewise_construct, std::tuple{ key }, std::tuple{});
            element = &m_map.back();
        }
        assert(element != nullptr);
        return element->second;
    }

    template<typename... Args> T& set(EnumT key, Args&&... args)
    {
        pointer element = find(key);
        if (element) {
            element->second = T(std::forward<Args>(args)...);
        }
        else {
            m_map.emplace_back(std::piecewise_construct,
                               std::tuple{ key },
                               std::tuple{ std::forward<Args>(args)... });
            element = &m_map.back();
        }
        assert(element != nullptr);
        return element->second;
    }

    T* get(EnumT key)
    {
        pointer element = find(key);
        return element ? &element->second : nullptr;
    }

    const T* get(EnumT key) const
    {
        const_pointer element = find(key);
        return element ? &element->second : nullptr;
    }

    iterator begin() { return m_map.begin(); }
    iterator end() { return m_map.end(); }

    const_iterator begin() const { return m_map.begin(); }
    const_iterator end() const { return m_map.end(); }

    const_iterator cbegin() const { return m_map.cbegin(); }
    const_iterator cend() const { return m_map.cend(); }

private:
    template<typename Self> auto* find_impl(Self& self, EnumT key)
    {
        auto hasSoughtKey = [key](const value_type& v) { return v.first == key; };
        auto it = std::find_if(self.m_map.begin(), self.m_map.end(), hasSoughtKey);
        return it != self.m_map.end() ? &*it : nullptr;
    }

    const value_type* find(EnumT key) const { return find_impl(*this, key); }
    value_type* find(EnumT key) { return find_impl(*this, key); }

    small_vector<value_type, NumEnumElements<EnumT>::value> m_map;
};

} // namespace Mg
