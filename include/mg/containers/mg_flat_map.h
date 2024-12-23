//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_flat_map.h
 * Sorted map data structure backed by std::vector.
 */

#pragma once

#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_optional.h"

#include <algorithm>
#include <vector>

namespace Mg {

/** Sorted map data structure backed by std::vector. Mostly follows std::map interface, with
simplifications. Does NOT provide std::map's iterator / pointer invalidation guarantees, since it
is backed by std::vector.
*/
template<typename KeyT, typename ValueT, typename Compare = std::less<KeyT>> class FlatMap {
public:
    using key_type = KeyT;
    using mapped_type = ValueT;
    using value_type = std::pair<KeyT, ValueT>;
    using key_compare = Compare;
    using iterator = typename std::vector<value_type>::iterator;
    using const_iterator = typename std::vector<value_type>::const_iterator;

    FlatMap() = default;
    FlatMap(std::initializer_list<value_type> ilist)
    {
        for (auto&& elem : ilist) {
            insert(elem);
        }
    }

    std::pair<iterator, bool> insert(const value_type& value)
    {
        return insert(value_type{ value });
    }
    std::pair<iterator, bool> insert(value_type&& value)
    {
        auto it = lower_bound(value.first);
        if (it != end() && it->first == value.first) {
            return { it, false };
        }
        it = m_data.insert(it, std::move(value));
        return { it, true };
    }

    iterator find(const key_type& key) noexcept
    {
        const auto it = lower_bound(key);
        return (it != end() && it->first == key) ? it : end();
    }
    const_iterator find(const key_type& key) const noexcept
    {
        const auto it = lower_bound(key);
        return (it != end() && it->first == key) ? it : end();
    }

    mapped_type& operator[](const key_type& key)
    {
        auto it = lower_bound(key);
        if (it == end() || it->first != key) {
            it = m_data.insert(it, { key, mapped_type{} });
        }
        return it->second;
    }
    mapped_type& operator[](key_type&& key)
    {
        auto it = lower_bound(key);
        if (it == end() || it->first != key) {
            it = m_data.insert(it, { std::move(key), mapped_type{} });
        }
        return it->second;
    }

    Opt<mapped_type&> try_get(const key_type& key)
    {
        auto it = find(key);
        if (it == end()) {
            return nullopt;
        }
        return it->second;
    }
    Opt<const mapped_type&> try_get(const key_type& key) const
    {
        auto it = find(key);
        if (it == end()) {
            return nullopt;
        }
        return it->second;
    }

    iterator erase(const_iterator pos) noexcept
    {
        MG_ASSERT_DEBUG(pos >= cbegin() && pos < end());
        return m_data.erase(pos);
    }
    iterator erase(const_iterator begin, const_iterator end) noexcept
    {
        MG_ASSERT_DEBUG(begin >= cbegin() && begin < cend());
        MG_ASSERT_DEBUG(end >= cbegin() && end < cend());
        MG_ASSERT_DEBUG(begin <= end);
        return m_data.erase(begin, end);
    }
    size_t erase(const key_type& key) noexcept
    {
        const auto it = lower_bound(key);
        if (it != end() && it->first == key) {
            m_data.erase(it);
            return 1;
        }
        return 0;
    }

    iterator lower_bound(const key_type& key) noexcept
    {
        return std::lower_bound(m_data.begin(), m_data.end(), key, ValueKeyCompare{});
    }
    const_iterator lower_bound(const key_type& key) const noexcept
    {
        return std::lower_bound(m_data.begin(), m_data.end(), key, ValueKeyCompare{});
    }

    iterator begin() noexcept { return m_data.begin(); }
    const_iterator begin() const noexcept { return m_data.begin(); }
    const_iterator cbegin() const noexcept { return m_data.begin(); }

    iterator end() noexcept { return m_data.end(); }
    const_iterator end() const noexcept { return m_data.end(); }
    const_iterator cend() const noexcept { return m_data.end(); }

    [[nodiscard]] bool empty() const noexcept { return m_data.empty(); }
    void clear() { m_data.clear(); }
    size_t size() const noexcept { return m_data.size(); }

    void swap(FlatMap& other) noexcept { std::swap(m_data, other.m_data); }

private:
    struct ValueKeyCompare {
        bool operator()(const value_type& value, const key_type& key) const
        {
            return key_compare{}(value.first, key);
        }
    };

    std::vector<value_type> m_data;
};

} // namespace Mg
