//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_sound_buffer_handle.h
 * Reference-counted handle to a sound buffer.
 */

#pragma once

#include <cstdint>
#include <utility>

namespace Mg::audio {

/** Internal buffer type. */
class SoundBuffer;

/** Reference-counted handle to a sound buffer. */
class SoundBufferHandle {
    friend class AudioContext;

public:
    SoundBufferHandle() = default;

    ~SoundBufferHandle()
    {
        decrement_ref_count();
        reset();
    }

    SoundBufferHandle(const SoundBufferHandle& rhs) : m_ptr(rhs.m_ptr) { increment_ref_count(); }

    SoundBufferHandle(SoundBufferHandle&& rhs) noexcept : m_ptr(rhs.m_ptr) { rhs.reset(); }

    SoundBufferHandle& operator=(const SoundBufferHandle& rhs)
    {
        SoundBufferHandle tmp(rhs);
        swap(tmp);
        return *this;
    }

    SoundBufferHandle& operator=(SoundBufferHandle&& rhs) noexcept
    {
        SoundBufferHandle tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    void swap(SoundBufferHandle& other) noexcept { std::swap(other.m_ptr, m_ptr); }

    void reset() { m_ptr = nullptr; }

    operator bool() const { return m_ptr != nullptr; }

private:
    explicit SoundBufferHandle(SoundBuffer* ptr) : m_ptr(ptr) { increment_ref_count(); }

    void increment_ref_count();
    void decrement_ref_count();

    SoundBuffer* m_ptr = nullptr;
};

} // namespace Mg::audio
