//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Magnus Bergsten
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

/** @file mg_slot_map.h
 * Slot map, array-like data structure with O(1) insertion and deletion.
 */

// TODO: split into storage (no handles) and Slot_map using the storage type? I.e. storage type
// (name pending) acts as Slot_map without handles (the -map part of Slot_map), hence no Key.

#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>

#include "mg/containers/mg_array.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_max.h"

#if __cplusplus >= 201703L
#define HAVE_OVERALIGNED_NEW 1
#else
#define HAVE_OVERALIGNED_NEW 0
#endif

namespace Mg {

namespace detail {
// Reserved index for uninitialised handles, Slot_map size is thus limited to `k_invalid_index - 1`.
static constexpr auto k_invalid_index = std::numeric_limits<uint32_t>::max();

static constexpr auto k_slot_map_growth_factor = 1.5f;
} // namespace detail

/** Slot_map_handle offers safe indexing to elements in a Slot_map. Handles are are only invalidated
 * when the element they refer to has been deleted.
 */
class Slot_map_handle {
public:
    using size_type    = uint32_t;
    using counter_type = uint32_t;

    Slot_map_handle() = default;

    explicit Slot_map_handle(size_type index, counter_type counter) noexcept
        : m_index(index), m_counter(counter)
    {}

    /** Get whether this handle is initialised. */
    operator bool() const noexcept { return m_counter < detail::k_invalid_index; }

    auto index() const noexcept { return m_index; }
    auto counter() const noexcept { return m_counter; }

    friend bool operator==(Slot_map_handle l, Slot_map_handle r) noexcept
    {
        return l.m_index == r.m_index && l.m_counter == r.m_counter;
    }

    friend bool operator!=(Slot_map_handle a, Slot_map_handle b) noexcept { return !(a == b); }

private:
    size_type    m_index   = 0;
    counter_type m_counter = detail::k_invalid_index;
};

/** The Slot_map is a compact, memory-contiguous data structure that supports O(1) insertion and
 * deletion without sacrificing efficient iteration and dereferencing. This makes it suitable for
 * storing many objects which are created and destroyed regularly, and over which efficient
 * iteration is required. A typical example would be entities in a simulation, where some
 * computation needs to be done for each entity during each simulation step.
 *
 * Internally, Slot_map pre-allocates a chunk of memory and then constructs elements of contained
 * type T in this chunk (similar to std::vector). Whenever an element is erased, the last element
 * in the memory chunk is moved onto the erased one's location - this makes sure that the elements
 * remain contiguous in memory, which allows efficient iteration.
 *
 * The Slot_map also offers a persistent Slot_map_handle type to index elements. This handle remains
 * valid even when types move around inside the Slot_map; it is only invalidated if the element it
 * refers to is destroyed. This works via internal metadata in the Slot_map: it keeps an auxiliary
 * array (called the Key) of indices into the main data array, which is updated when elements
 * move.
 *
 * Limitations: not allocator-aware.
 *
 * @tparam T Element type.
 */
template<typename T> class Slot_map {
public:
    using value_type = T;
    using size_type  = uint32_t;

    using difference_type = ptrdiff_t;

    using reference       = T&;
    using const_reference = const T&;

    using pointer       = T*;
    using const_pointer = const T*;

    using iterator       = T*;
    using const_iterator = const T*;

    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /** Construct empty Slot_map. */
    Slot_map() : Slot_map(0) {}

    /** Construct Slot_map.
     * @param max_elems The capacity of the Slot_map.
     */
    explicit Slot_map(size_type max_elems) { init(max_elems); }

    /** Copy construct Slot_map. SlotMapHandles into rhs will be valid into this copy as well. */
    Slot_map(const Slot_map& rhs);

    /** Move construct Slot_map. N.B. moving invalidates SlotMapHandles into rhs. */
    Slot_map(Slot_map&& rhs) noexcept;

    Slot_map& operator=(Slot_map rhs) noexcept
    {
        swap(rhs);
        return *this;
    }

    ~Slot_map() { clear(); }

    /** Insert element into this Slot_map.
     * @param rhs Object to insert.
     * @return A Slot_map_handle pointing to the object in the Slot_map.
     */
    Slot_map_handle insert(const T& rhs) { return emplace(rhs); }

    /** Insert element into this Slot_map.
     * @param rhs Object to insert.
     * @return A Slot_map_handle pointing to the object in the Slot_map.
     */
    Slot_map_handle insert(T&& rhs) { return emplace(std::move(rhs)); }

    /** Emplace element into this Slot_map.
     * @param args... Constructor parameters.
     * @return A Slot_map_handle pointing to the object in the Slot_map.
     */
    template<typename... Ts> Slot_map_handle emplace(Ts&&... args);

    /** Destroy the element pointed to by handle. */
    void erase(Slot_map_handle handle);

    /** Destroy the element pointed to by iterator.
     * @return iterator to next element
     */
    iterator erase(iterator it);

    /** Erase a range of elements. */
    iterator erase(iterator first, iterator last);

    /** Clear the Slot_map, destroying all elements. */
    void clear()
    {
        for (size_type i = 0; i < size(); ++i) { destroy_element_at(i); }
        m_num_elems = 0;
    }

    /** Get reference to the element pointed to by handle. */
    reference operator[](Slot_map_handle handle);

    /** Get const reference to the element pointed to by handle. */
    const_reference operator[](Slot_map_handle handle) const;

    /** Get whether handle is valid, i.e. refers to an existing element. */
    bool is_handle_valid(Slot_map_handle handle) const;

    /** Get persistent handle to element pointed to by iterator. */
    Slot_map_handle make_handle(const_iterator it) const;

    /** Get number of elements currently in this Slot_map. */
    size_type size() const { return m_num_elems; }

    /** Get pointer to first element in this Slot_map. */
    pointer data() { return empty() ? nullptr : &element_at(0); }

    /** Get pointer to first element in this Slot_map. */
    const_pointer data() const { return empty() ? nullptr : &element_at(0); }

    /** Get the current capacity in number of elements of this Slot_map. */
    size_type capacity() const { return m_max_elems; }

    /** Maximum size that this container can hold. */
    constexpr size_type max_size() const { return detail::k_invalid_index - 1; }

    /** Resize the Slot_map, allocating memory for the new capacity and moving contained elements.
     */
    void resize(size_type new_size);

    /** Get whether the Slot_map is empty. */
    bool empty() const { return m_num_elems == 0; }

    void swap(Slot_map& rhs) noexcept;

    iterator       begin() { return data(); }
    const_iterator begin() const { return cbegin(); }
    const_iterator cbegin() const { return data(); }

    iterator       end() { return data() + size(); }
    const_iterator end() const { return cend(); }
    const_iterator cend() const { return data() + size(); }

    reverse_iterator       rbegin() { return reverse_iterator{ end() }; }
    const_reverse_iterator rbegin() const { return crbegin(); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator{ end() }; }

    reverse_iterator       rend() { return reverse_iterator{ begin() }; }
    const_reverse_iterator rend() const { return crend(); }
    const_reverse_iterator crend() const { return const_reverse_iterator{ begin() }; }

private:
    using counter_type = uint32_t;

    /** Key is used in an auxiliary array to support element look-ups. There is one key element per
     * slot in the Slot_map. Slot_map_handles are actually indices into the key, which holds the
     * actual offset of the contained element (see `position` below).
     */
    struct Key {
        /* position is used for two purposes:
         * - if the Key corresponds to an existing element, position is the offset at which the
         *   element is stored.
         * - if the Key corresponds to an unused element, position acts as a linked-list of free Key
         *   indices with m_first_free_key_index as head.
         */
        size_type position;

        /** counter is used to validate that Slot_map_handles have not been invalidated. Incremented
         * when the element for this Key is destroyed.
         */
        counter_type counter;

        /* inverse_index is used to find the appropriate Key when only knowing the element's
         * position, e.g. when calling erase() with an iterator:
         * m_key[m_key[elem_pos].inverse_index]
         */
        size_type inverse_index;
    };

    void allocate_arrays(size_type num_elems);

    void init(size_type max_elems);

    void erase_at(size_type index);

    // Helper for element insertion: find position, create handle, and update auxiliary Key array.
    std::pair<size_type, Slot_map_handle> insert_helper();

    // Find element corresponding to handle; or nullptr, if handle is invalid.
    pointer       find_element(const Slot_map_handle& handle);
    const_pointer find_element(const Slot_map_handle& handle) const;

    // Get reference to data element
    reference element_at(size_type position)
    {
        MG_ASSERT(position < size());
        return *reinterpret_cast<pointer>(&m_data[position]);
    }
    const_reference element_at(size_type position) const
    {
        MG_ASSERT(position < size());
        return *reinterpret_cast<const_pointer>(&m_data[position]);
    }

    template<typename... Ts> void construct_element_at(size_type position, Ts&&... args)
    {
        MG_ASSERT(position < capacity());
        new (&m_data[position]) T(std::forward<Ts>(args)...);
    }

    void destroy_element_at(size_type position) { element_at(position).~T(); }

    // Move element at position `from` to (unused) position `to`.
    void move_element_to(size_type from, size_type to);

private:
    /** Storage for element data. */
    struct alignas(T) Elem_data {
        char data[sizeof(T)];
    };

    /** Data buffer, stores both elements. */
    ArrayUnknownSize<Elem_data> m_data;

    ArrayUnknownSize<Key> m_key;

    size_type m_max_elems = 0;
    size_type m_num_elems = 0;

    // Head of free-key linked-list
    size_type m_first_free_key_index;
};

/** Insertion iterator (similar to std::insert_iterator)
 * This is included -- rather than using std::back_insert_iterator, or similar -- because the
 * standard insertion iterators expect interfaces that do not quite make sense for Slot_map, e.g.
 * push_back(); or insert() with a position parameter: these would be misleading since Slot_map is
 * unordered (in the sense that order may change as elements move around).
 */
template<typename T> class Slot_map_insert_iterator {
public:
    using value_type        = void;
    using difference_type   = void;
    using pointer           = void;
    using reference         = void;
    using iterator_category = std::output_iterator_tag;

    explicit Slot_map_insert_iterator(Slot_map<T>& sm) : m_slot_map(&sm) {}

    Slot_map_insert_iterator(const Slot_map_insert_iterator&) = default;
    Slot_map_insert_iterator& operator=(const Slot_map_insert_iterator&) = default;

    // No-ops for OutputIterator interface conformance
    Slot_map_insert_iterator& operator++() { return *this; }
    Slot_map_insert_iterator& operator++(int) { return *this; }
    Slot_map_insert_iterator& operator*() { return *this; }

    Slot_map_insert_iterator& operator=(const T& rhs)
    {
        m_slot_map->insert(rhs);
        return *this;
    }

    Slot_map_insert_iterator& operator=(T&& rhs)
    {
        m_slot_map->insert(std::move(rhs));
        return *this;
    }

private:
    Slot_map<T>* m_slot_map;
};

template<typename T> Slot_map_insert_iterator<T> slot_map_inserter(Slot_map<T>& sm)
{
    return Slot_map_insert_iterator<T>(sm);
}

//--------------------------------------------------------------------------------------------------
// Mg::Slot_map implementation
//--------------------------------------------------------------------------------------------------

template<typename T> void Slot_map<T>::allocate_arrays(size_type num_elems)
{
    ArrayUnknownSize<Elem_data> data(nullptr);
    ArrayUnknownSize<Key>       key(nullptr);

#if !HAVE_OVERALIGNED_NEW
    static_assert(alignof(T) <= alignof(std::max_align_t),
                  "Slot_map only supports over-aligned element types for C++17 and onwards.");
#endif

    if (num_elems > 0) {
        data = ArrayUnknownSize<Elem_data>::make(num_elems);
        key  = ArrayUnknownSize<Key>::make(num_elems);
    }

    // Allocate first, assign later, for exception safety.
    // If allocating either array throws, then m_data and m_max_elems should still be valid.
    m_data      = std::move(data);
    m_key       = std::move(key);
    m_max_elems = num_elems;
}

// Initialisation
template<typename T> void Slot_map<T>::init(size_type max_elems)
{
    m_num_elems = 0;
    allocate_arrays(max_elems);

    m_first_free_key_index = 0;

    // Set initial index data
    for (size_type i = 0; i < max_elems; ++i) {
        Key& k          = m_key[i];
        k.position      = i + 1;
        k.counter       = 0;
        k.inverse_index = 0;
    }
}

// Copy constructor
template<typename T> Slot_map<T>::Slot_map(const Slot_map& rhs)
{
    try {
        allocate_arrays(rhs.capacity());

        // Copy elements and copy key.
        // This way allows handles into rhs to be valid also into this copy.
        for (size_type i = 0; i < rhs.size(); ++i) {
            construct_element_at(i, rhs.element_at(i));
            m_key[i] = rhs.m_key[i];
        }

        m_num_elems            = rhs.m_num_elems;
        m_first_free_key_index = rhs.m_first_free_key_index;
    }
    catch (...) {
        init(0);
        throw;
    }
}

// Resize Slot_map
template<typename T> void Slot_map<T>::resize(size_type new_size)
{
    MG_ASSERT(size() <= new_size);
    Slot_map<T> tmp(new_size);

    // Move elements and copy key
    for (size_type i = 0; i < size(); ++i) {
        tmp.construct_element_at(i, std::move_if_noexcept(element_at(i)));
        tmp.m_key[i] = m_key[i];
    }

    tmp.m_num_elems            = m_num_elems;
    tmp.m_first_free_key_index = m_first_free_key_index;

    swap(tmp);
}

// Move constructor
// The default-generated one would work, but would also leave rhs in an invalid state.
template<typename T> Slot_map<T>::Slot_map(Slot_map&& rhs) noexcept
{
    m_data                 = std::move(rhs.m_data);
    m_key                  = std::move(rhs.m_key);
    m_max_elems            = rhs.m_max_elems;
    m_num_elems            = rhs.m_num_elems;
    m_first_free_key_index = rhs.m_first_free_key_index;
    rhs.init(0);
}

// Emplace
template<typename T>
template<typename... Ts>
auto Slot_map<T>::emplace(Ts&&... args) -> Slot_map_handle
{
    // This copy is usually redundant, but guards against the case where args refers to elements
    // within this Slot_map and insert_helper has to resize the storage.
    // TODO: The copy could be avoided by refactoring such that resizing the storage first allocates
    // new buffers, constructs the new element there, and only then moves the old elements over.
    T tmp(std::forward<Ts>(args)...);

    auto [pos, handle] = insert_helper();
    construct_element_at(pos, std::move(tmp));
    return handle;
}

template<typename T> void Slot_map<T>::swap(Slot_map& rhs) noexcept
{
    using std::swap;
    swap(m_data, rhs.m_data);
    swap(m_key, rhs.m_key);
    swap(m_max_elems, rhs.m_max_elems);
    swap(m_num_elems, rhs.m_num_elems);
    swap(m_first_free_key_index, rhs.m_first_free_key_index);
}

// Destroy element by position and decouple element's Key
template<typename T> void Slot_map<T>::erase_at(size_type position)
{
    // Increment counter to invalidate SlotMapHandles to destroyed element.
    const size_type old_key_index = m_key[position].inverse_index;
    Key&            old_key       = m_key[old_key_index];
    ++old_key.counter;

    destroy_element_at(position);

    // Move last elem to deleted's position
    const auto last_position = m_num_elems - 1;
    move_element_to(last_position, position);

    // Decouple destroyed element's Key
    old_key.position       = m_first_free_key_index;
    m_first_free_key_index = old_key_index;

    --m_num_elems;
}

// Erase element by handle
template<typename T> void Slot_map<T>::erase(Slot_map_handle handle)
{
    Key& k = m_key[handle.index()];

    MG_ASSERT(k.counter == handle.counter() && k.position < m_num_elems &&
              "Slot_map::erase() called with invalid handle.");

    erase_at(k.position);
}

// Erase element by iterator
template<typename T> auto Slot_map<T>::erase(iterator it) -> iterator
{
    const ptrdiff_t position_signed = std::distance(begin(), it);
    const auto      position        = static_cast<size_type>(position_signed);

    if (position_signed < 0 || position >= size()) { return end(); }

    erase_at(position);
    return iterator{ begin() + position };
}

// Erase range
template<typename T> auto Slot_map<T>::erase(iterator first, iterator last) -> iterator
{
    MG_ASSERT((first - begin()) >= 0 && first < last);

    const ptrdiff_t index_first          = std::distance(begin(), first);
    const auto      erase_count          = last - first;
    const auto      index_last           = index_first + erase_count;
    const auto      num_subsequent_elems = std::distance(end(), last);

    // Erase elements in range
    for (auto i = index_first; i < index_last; ++i) { destroy_element_at(i); }

    // Move subsequent elements to start of erased range
    for (auto i = index_last; i < index_last + num_subsequent_elems; ++i) {
        move_element_to(i, i - erase_count);
    }

    m_num_elems -= erase_count;

    return first;
};

// Insert helper: find position for insertion, create handle, and update auxiliary Key array.
template<typename T> auto Slot_map<T>::insert_helper() -> std::pair<size_type, Slot_map_handle>
{
    auto pos = m_num_elems;

    if (pos == m_max_elems) {
        resize(max(2u, static_cast<uint32_t>(size() * detail::k_slot_map_growth_factor)));
    }

    ++m_num_elems;

    auto index             = m_first_free_key_index;
    Key& key               = m_key[index];
    m_first_free_key_index = key.position;
    key.position           = pos;

    m_key[pos].inverse_index = index;

    return std::make_pair(pos, Slot_map_handle(index, key.counter));
}

// Element finding helper
template<typename T> auto Slot_map<T>::find_element(const Slot_map_handle& handle) -> pointer
{
    return const_cast<pointer>(static_cast<const Slot_map<T>&>(*this).find_element(handle));
}

template<typename T>
auto Slot_map<T>::find_element(const Slot_map_handle& handle) const -> const_pointer
{
    MG_ASSERT(handle.index() < m_max_elems);
    const Key& k = m_key[handle.index()];

    if (k.position >= m_num_elems || k.counter != handle.counter()) { return nullptr; }

    return &element_at(k.position);
}

// Get element by handle
template<typename T> auto Slot_map<T>::operator[](Slot_map_handle handle) -> reference
{
    MG_ASSERT(is_handle_valid(handle));
    return *(find_element(handle));
}

// Get element by handle, const
template<typename T> auto Slot_map<T>::operator[](Slot_map_handle handle) const -> const_reference
{
    MG_ASSERT(is_handle_valid(handle));
    return *(find_element(handle));
}

// Check whether handle refers to a value that (still) exists in the Slot_map
template<typename T> bool Slot_map<T>::is_handle_valid(Slot_map_handle handle) const
{
    return find_element(handle) != nullptr;
}

// Get the corresponding handle for an iterator
template<typename T> Slot_map_handle Slot_map<T>::make_handle(const_iterator it) const
{
    auto position = std::distance(begin(), it);

    if (position < 0 || size_type(position) >= size()) { return Slot_map_handle(); }

    const Key& k = m_key[m_key[size_type(position)].inverse_index];
    return Slot_map_handle(k.position, k.counter);
};

template<typename T> void Slot_map<T>::move_element_to(size_type from, size_type to)
{
    if (from == to) { return; }
    construct_element_at(to, std::move_if_noexcept(element_at(from)));
    destroy_element_at(from);

    // Update index of moved elem to match new position
    auto from_key_index            = m_key[from].inverse_index;
    m_key[from_key_index].position = to;
}

//--------------------------------------------------------------------------------------------------
// Non-member functions on Slot_map
//--------------------------------------------------------------------------------------------------

template<typename T> void swap(Slot_map<T>& lhs, Slot_map<T>& rhs) noexcept
{
    lhs.swap(rhs);
}

} // namespace Mg
