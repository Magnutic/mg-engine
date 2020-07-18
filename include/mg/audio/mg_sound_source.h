//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_sound_source.h
 * A source of sound, which may play a given sound buffer.
 */

#pragma once

#include "mg/audio/mg_sound_buffer_handle.h"

#include <glm/vec3.hpp>

namespace Mg::audio {

/** Parameters struct which may be used to set all SoundSource settings at once. */
struct SoundSourceSettings {
    float gain = 1.0f;
    float pitch = 1.0f;
    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 velocity = { 0.0f, 0.0f, 0.0f };
    bool enable_attenuation = true;
    bool enable_listener_relative_position = false;
};

/* A source of sound, which may play a given sound buffer.
 * Note that positional sound effects (e.g. attenuation) only work with mono sound buffers.
 */
class SoundSource {
public:
    SoundSource();

    explicit SoundSource(SoundBufferHandle buffer) : SoundSource()
    {
        set_buffer(std::move(buffer));
    }

    explicit SoundSource(SoundBufferHandle buffer, const SoundSourceSettings& settings)
        : SoundSource()
    {
        set_buffer(std::move(buffer));
        apply_settings(settings);
    }

    ~SoundSource();

    SoundSource(const SoundSource& rhs) : SoundSource() { set_buffer(rhs.m_buffer); }

    SoundSource(SoundSource&& rhs) noexcept
        : m_source_id(rhs.m_source_id), m_buffer(std::move(rhs.m_buffer))
    {
        rhs.m_source_id = 0;
        rhs.m_buffer = {};
    }

    SoundSource& operator=(const SoundSource& rhs)
    {
        SoundSource tmp(rhs);
        swap(tmp);
        return *this;
    }

    SoundSource& operator=(SoundSource&& rhs) noexcept
    {
        SoundSource tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    void swap(SoundSource& rhs) noexcept
    {
        std::swap(m_source_id, rhs.m_source_id);
        std::swap(m_buffer, rhs.m_buffer);
    }

    /** Set the sound buffer of the sound source.
     * If the sound source was playing, it will be stopped.
     */
    void set_buffer(SoundBufferHandle buffer);

    void play();
    void loop();
    void pause();
    void stop();

    enum class State { playing, paused, stopped };
    State get_state();

    /** Unit of time offset within the sample: seconds from start or as fraction of time (e.g. 0.5
     * means halfway through the sound sample.
     */
    enum class OffsetUnit { seconds, fraction };

    /** Get current offset in the sound in the requested unit. */
    float current_offset(OffsetUnit unit) const;

    /** Set current offset in the sound in the requested unit. */
    void set_offset(OffsetUnit unit, float value);

    /** Get the position of the sound source.
     * If listener-relative position is enabled, the position is interpreted as relative to the
     * listener. Otherwise, the position is interpreted as world-space (default).
     */
    glm::vec3 get_position() const;

    /** Set the position of the sound source.
     * If listener-relative position is enabled, the position is interpreted as relative to the
     * listener. Otherwise, the position is interpreted as world-space (default).
     */
    void set_position(glm::vec3 position);

    /** Get the current velocity of the sound source. */
    glm::vec3 get_velocity() const;

    /** Set the current velocity of the sound source. */
    void set_velocity(glm::vec3 velocity);

    /** Set source gain (volume). Range: [0.0f, infinity). Default 1.0f. */
    void set_gain(float gain);

    /** Get source gain (volume). */
    float get_gain() const;

    /* Set source pitch. Range: (0.0f, infinity). Default: 1.0f. */
    void set_pitch(float pitch);

    /** Get source pitch. */
    float get_pitch() const;

    /** Whether to enable attenuation for this sound source.
     * Default true.
     */
    void enable_attenuation(bool enable);

    /** Whether to interpret source position as relative to the listener instead of world-space.
     * Default false.
     */
    void enable_listener_relative_position(bool enable);

    /** Apply the settings specified in the argument to this sound source. */
    void apply_settings(const SoundSourceSettings& settings)
    {
        set_gain(settings.gain);
        set_pitch(settings.pitch);
        set_position(settings.position);
        set_velocity(settings.velocity);
        enable_attenuation(settings.enable_attenuation);
        enable_listener_relative_position(settings.enable_listener_relative_position);
    }

    /** Returns the length of the sample (in seconds). */
    float length() const;

    /** Returns whether the buffer attached to this sound source is stereo or mono.
     * Note: positional audio only works with mono sound buffers.
     */
    bool is_stereo() const;

private:
    uintptr_t m_source_id = 0;
    SoundBufferHandle m_buffer;
};

} // namespace Mg::audio
