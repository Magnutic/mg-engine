//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_audio_context.h
 * Initialises and owns audio system context.
 */

#pragma once

#include "mg/audio/mg_sound_buffer_handle.h"
#include "mg/utils/mg_gsl.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>

namespace Mg {
class SoundResource;
}

/** Audio-related functionality. */
namespace Mg::audio {

struct ListenerState {
    glm::vec3 position = {};
    glm::vec3 velocity = {};
    glm::vec3 forward = {};
    glm::vec3 up = {};
};

struct AudioContextData;

/** Initialises and owns audio system context. */
class AudioContext : PImplMixin<AudioContextData> {
    friend class SoundBufferHandle;
    friend class SoundSource;
    struct ConstructKey {};

public:
    AudioContext(ConstructKey);
    ~AudioContext();

    MG_MAKE_NON_COPYABLE(AudioContext);
    MG_MAKE_NON_MOVABLE(AudioContext);

    /** Get audio context singleton. */
    static AudioContext& get();

    struct GenerateSoundBufferResult {
        SoundBufferHandle handle;
        std::string error_reason;
    };

    /** Generate new sound buffer from sound file data.
     * @param sound_file_data Raw binary data from sound file (e.g. .wav, .ogg).
     */
    GenerateSoundBufferResult generate_sound_buffer(span<const std::byte> sound_file_data);

    /** Set the master gain (volume) for the listener. */
    void set_gain(float gain);
    float get_gain() const;

    void set_listener_state(const ListenerState& state);

    void check_for_errors() const;

private:
    // Used by SoundSource
    void link_buffer_to_source(uintptr_t sound_source_id, const SoundBufferHandle& buffer);

    // Used by SoundBufferHandle
    void increment_ref_count(SoundBuffer* ptr);
    void decrement_ref_count(SoundBuffer* ptr);
};

} // namespace Mg::audio
