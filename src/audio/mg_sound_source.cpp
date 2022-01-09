//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/audio/mg_sound_source.h"
#include "mg/audio/mg_audio_context.h"
#include "mg/utils/mg_gsl.h"

#include <AL/al.h>

namespace Mg::audio {

SoundSource::SoundSource()
{
    AudioContext::get(); // Ensures OpenAL is initialized.

    ALuint source_id = 0;
    alGenSources(1, &source_id);
    alSource3f(source_id, AL_POSITION, 0.0f, 0.0f, 0.0f);
    m_source_id = source_id;
}

SoundSource::~SoundSource()
{
    if (m_source_id != 0) {
        auto source_id = narrow_cast<ALuint>(m_source_id);
        alSourceStop(source_id);
        alDeleteSources(1, &source_id);
    }
}

void SoundSource::set_buffer(SoundBufferHandle buffer)
{
    alSourceStop(narrow_cast<ALuint>(m_source_id));
    m_buffer = std::move(buffer);
    AudioContext::get().link_buffer_to_source(m_source_id, m_buffer);
}

void SoundSource::play()
{
    alSourcei(narrow_cast<ALuint>(m_source_id), AL_LOOPING, 0);
    alSourcePlay(narrow_cast<ALuint>(m_source_id));
}

void SoundSource::loop()
{
    alSourcei(narrow_cast<ALuint>(m_source_id), AL_LOOPING, 1);
    alSourcePlay(narrow_cast<ALuint>(m_source_id));
}

void SoundSource::pause()
{
    alSourcePause(narrow_cast<ALuint>(m_source_id));
}

void SoundSource::stop()
{
    alSourceStop(narrow_cast<ALuint>(m_source_id));
}

SoundSource::State SoundSource::get_state()
{
    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);

    ALenum state = {};
    alGetSourcei(narrow_cast<ALuint>(m_source_id), AL_SOURCE_STATE, &state);

    switch (state) {
    case AL_INITIAL:
        [[fallthrough]];
    case AL_STOPPED:
        return State::stopped;
    case AL_PLAYING:
        return State::playing;
    case AL_PAUSED:
        return State::paused;
    default:
        MG_ASSERT(false && "Unexpected SoundSource state");
    }

    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);
}

float SoundSource::current_offset(OffsetUnit unit) const
{
    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);

    float result = 0.0f;
    alGetSourcef(narrow_cast<ALuint>(m_source_id), AL_SEC_OFFSET, &result);

    switch (unit) {
    case OffsetUnit::seconds:
        return result;
    case OffsetUnit::fraction:
        return result / length();
    default:
        MG_ASSERT(false);
    }

    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);
}

void SoundSource::set_offset(OffsetUnit unit, const float value)
{
    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);

    float actualValue = 0.0f;

    switch (unit) {
    case OffsetUnit::seconds:
        actualValue = value;
        break;
    case OffsetUnit::fraction:
        actualValue = value * length();
        break;
    }

    alSourcef(narrow_cast<ALuint>(m_source_id), AL_SEC_OFFSET, actualValue);

    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);
}

glm::vec3 SoundSource::get_position() const
{
    glm::vec3 result;
    alGetSource3f(narrow_cast<ALuint>(m_source_id), AL_POSITION, &result.x, &result.y, &result.z);
    return result;
}

void SoundSource::set_position(glm::vec3 position)
{
    alSource3f(narrow_cast<ALuint>(m_source_id), AL_POSITION, position.x, position.y, position.z);
}

glm::vec3 SoundSource::get_velocity() const
{
    glm::vec3 result;
    alGetSource3f(narrow_cast<ALuint>(m_source_id), AL_VELOCITY, &result.x, &result.y, &result.z);
    return result;
}

void SoundSource::set_velocity(glm::vec3 velocity)
{
    alSource3f(narrow_cast<ALuint>(m_source_id), AL_VELOCITY, velocity.x, velocity.y, velocity.z);
}

void SoundSource::set_gain(const float gain)
{
    alSourcef(narrow_cast<ALuint>(m_source_id), AL_GAIN, gain);
}

float SoundSource::get_gain() const
{
    float result = 0.0f;
    alGetSourcef(narrow_cast<ALuint>(m_source_id), AL_GAIN, &result);
    return result;
}

void SoundSource::set_pitch(const float pitch)
{
    alSourcef(narrow_cast<ALuint>(m_source_id), AL_PITCH, pitch);
}

float SoundSource::get_pitch() const
{
    float result = 0.0f;
    alGetSourcef(narrow_cast<ALuint>(m_source_id), AL_PITCH, &result);
    return result;
}

void SoundSource::enable_attenuation(const bool enable)
{
    alSourcei(narrow_cast<ALuint>(m_source_id), AL_ROLLOFF_FACTOR, enable ? 1.0f : 0.0f);
}

void SoundSource::enable_listener_relative_position(const bool enable)
{
    alSourcei(narrow_cast<ALuint>(m_source_id), AL_SOURCE_RELATIVE, enable ? AL_TRUE : AL_FALSE);
}

float SoundSource::length() const
{
    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);

    ALint al_buffer_id = 0;
    ALint buffer_size = 0;
    ALint frequency = 0;
    ALint bits_per_sample = 0;
    ALint num_channels = 0;

    alGetSourcei(narrow_cast<ALuint>(m_source_id), AL_BUFFER, &al_buffer_id);
    alGetBufferi(narrow_cast<ALuint>(al_buffer_id), AL_SIZE, &buffer_size);
    alGetBufferi(narrow_cast<ALuint>(al_buffer_id), AL_FREQUENCY, &frequency);
    alGetBufferi(narrow_cast<ALuint>(al_buffer_id), AL_BITS, &bits_per_sample);
    alGetBufferi(narrow_cast<ALuint>(al_buffer_id), AL_CHANNELS, &num_channels);

    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);

    const auto bytes_per_sample = bits_per_sample / 8;
    const auto bytes_per_second = num_channels * frequency * bytes_per_sample;

    return static_cast<float>(buffer_size) / static_cast<float>(bytes_per_second);
}

bool SoundSource::is_stereo() const
{
    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);

    ALint al_buffer_id = 0;
    ALint num_channels = 0;
    alGetSourcei(narrow_cast<ALuint>(m_source_id), AL_BUFFER, &al_buffer_id);
    alGetBufferi(narrow_cast<ALuint>(al_buffer_id), AL_CHANNELS, &num_channels);

    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);

    return num_channels == 2;
}

} // namespace Mg::audio
