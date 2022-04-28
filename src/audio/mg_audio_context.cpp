//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/audio/mg_audio_context.h"

#include "mg/containers/mg_array.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_assert.h"
#include "mg/utils/mg_math_utils.h"
#include "mg/utils/mg_optional.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <sndfile.h>

#include <fmt/core.h>

#include <plf_colony.h>

#include <atomic>
#include <memory>
#include <mutex>

namespace Mg::audio {

namespace {

//--------------------------------------------------------------------------------------------------
// OpenAL context management
//--------------------------------------------------------------------------------------------------

// Initialize OpenAL
void init_OpenAL()
{
    // Open and initialize default OpenAL device.
    // TODO: should this be configurable / overridable, or is default always right?
    // TODO: error handling: allow running without sound if OpenAL fails to initialize?
    ALCdevice* device = alcOpenDevice(nullptr);
    if (!device) {
        log.error("Audio system error: Failed to open OpenAL device.");
        throw RuntimeError();
    }

    ALCcontext* context = alcCreateContext(device, nullptr);
    if (!context || alcMakeContextCurrent(context) == ALC_FALSE) {
        if (context) {
            alcDestroyContext(context);
        }

        alcCloseDevice(device);
        log.error("Audio system error: Failed to set OpenAL context.");
        throw RuntimeError();
    }

    const char* device_name = nullptr;
    if (alcIsExtensionPresent(device, "ALC_ENUMERATE_ALL_EXT") != 0) {
        device_name = alcGetString(device, ALC_ALL_DEVICES_SPECIFIER);
    }
    if (!device_name || alcGetError(device) != AL_NO_ERROR) {
        device_name = alcGetString(device, ALC_DEVICE_SPECIFIER);
    }

    log.message("Successfully initialized OpenAL audio context [device: {}].", device_name);
}

// Destroy OpenAL context and close its device.
void close_OpenAL()
{
    ALCcontext* context = alcGetCurrentContext();
    if (context) {
        ALCdevice* device = alcGetContextsDevice(context);

        log.message("Destroying OpenAL context.");
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);

        log.message("Closing OpenAL device.");
        alcCloseDevice(device);
    }
}

//--------------------------------------------------------------------------------------------------
// Functions for creating sound buffers using libsndfile and OpenAL
//--------------------------------------------------------------------------------------------------

// 'Virtual file' interface for reading from span of bytes.
namespace {

struct State {
    span<const std::byte> data;
    sf_count_t pos = 0;
};

sf_count_t read(void* ptr, sf_count_t count, void* user_data)
{
    auto* state = static_cast<State*>(user_data);
    const auto file_size = as<sf_count_t>(state->data.size_bytes());
    const sf_count_t real_count = min(file_size - state->pos, count);
    if (real_count <= 0) {
        return 0;
    }

    std::memcpy(ptr, &state->data[as<size_t>(state->pos)], as<size_t>(real_count));
    state->pos += real_count;
    return real_count;
}

sf_count_t get_filelen(void* user_data)
{
    auto* state = static_cast<State*>(user_data);
    return as<sf_count_t>(state->data.size_bytes());
}

sf_count_t seek(sf_count_t offset, int whence, void* user_data)
{
    auto* state = static_cast<State*>(user_data);
    const auto file_size = as<sf_count_t>(state->data.size_bytes());

    switch (whence) {
    case SEEK_CUR:
        state->pos += offset;
        break;
    case SEEK_SET:
        state->pos = offset;
        break;
    case SEEK_END:
        state->pos = file_size + offset;
        break;
    default:
        MG_ASSERT(false && "Unexpected libsndfile seek mode");
    }

    state->pos = clamp<sf_count_t>(state->pos, 0, file_size);
    return state->pos;
}

sf_count_t tell(void* user_data)
{
    auto* state = static_cast<State*>(user_data);
    return state->pos;
}

sf_count_t write(const void* /*ptr*/, sf_count_t /*count*/, void* /*user_data*/)
{
    MG_ASSERT(false);
    return 0;
}

// Auto-destructing SNDFILE*
using UniqueSndFile = std::unique_ptr<SNDFILE, decltype(&sf_close)>;

class SndFileReader {
public:
    SF_INFO sf_info{};
    UniqueSndFile sndfile = { nullptr, &sf_close };
    std::string error_reason;

    explicit SndFileReader(span<const std::byte> data)
    {
        state.data = data;

        // Use virtual file interface to read input data.
        sndfile.reset(sf_open_virtual(&sf_io, SFM_READ, &sf_info, &state));

        if (!sndfile) {
            error_reason = fmt::format("Failed to read sound file data: '{}'",
                                       sf_strerror(sndfile.get()));
        }
    }

private:
    SF_VIRTUAL_IO sf_io = { &get_filelen, &seek, &read, &write, &tell };
    State state = {};
};

} // namespace

//--------------------------------------------------------------------------------------------------

struct GenerateOpenALBufferResult {
    Opt<ALuint> al_buffer_id = 0u;
    std::string error_reason;
};

GenerateOpenALBufferResult generate_OpenAL_buffer(SNDFILE* sndfile, SF_INFO& sf_info)
{
    const sf_count_t max_num_frames = std::numeric_limits<int>::max() /
                                      (as<int>(sizeof(short)) * sf_info.channels);

    if (sf_info.frames < 1 || sf_info.frames > max_num_frames) {
        return { nullopt, fmt::format("Bad sample count (was {})", sf_info.frames) };
    }

    ALenum format = {};
    if (sf_info.channels == 1) {
        format = AL_FORMAT_MONO16;
    }
    else if (sf_info.channels == 2) {
        format = AL_FORMAT_STEREO16;
    }
    else {
        return { nullopt, fmt::format("Unsupported channel count (was {})", sf_info.channels) };
    }

    ALuint al_buffer_id = 0;

    { // Use libsndfile to decode the audio file to an OpenAL buffer.
        auto buffer =
            Array<short>::make_for_overwrite(as<size_t>(sf_info.frames * sf_info.channels));
        short* dst = buffer.data();

        const sf_count_t num_frames = sf_readf_short(sndfile, dst, sf_info.frames);
        if (num_frames < 1) {
            return { nullopt, "Failed to read samples." };
        }

        const auto num_bytes = as<ALsizei>(buffer.size() * sizeof(short));

        alGenBuffers(1, &al_buffer_id);
        alBufferData(al_buffer_id, format, buffer.data(), num_bytes, sf_info.samplerate);
    }

    ALenum al_error = alGetError();
    if (al_error != AL_NO_ERROR) {
        if (alIsBuffer(al_buffer_id) != 0) {
            alDeleteBuffers(1, &al_buffer_id);
        }
        return { nullopt, fmt::format("OpenAL Error: {}", alGetString(al_error)) };
    }

    return { al_buffer_id, {} };
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Internal storage for sound buffers.
//--------------------------------------------------------------------------------------------------

class SoundBuffer {
public:
    explicit SoundBuffer(ALuint al_buffer_id_) : al_buffer_id(al_buffer_id_) {}
    ~SoundBuffer()
    {
        MG_LOG_DEBUG("Deleting OpenAL buffer {}.", al_buffer_id);
        alDeleteBuffers(1, &al_buffer_id);
    }

    MG_MAKE_NON_MOVABLE(SoundBuffer);
    MG_MAKE_NON_COPYABLE(SoundBuffer);

    ALuint al_buffer_id = 0;
    std::atomic_int ref_count = 0;
};

//--------------------------------------------------------------------------------------------------
// AudioContext implementation
//--------------------------------------------------------------------------------------------------

struct AudioContextData {
    plf::colony<SoundBuffer> sound_buffers;

    std::mutex sound_buffers_mutex;
    std::mutex libsndfile_mutex;
};

AudioContext::AudioContext(ConstructKey)
{
    init_OpenAL();
}

AudioContext::~AudioContext()
{
    close_OpenAL();
}

AudioContext& AudioContext::get()
{
    static auto context = std::make_unique<AudioContext>(ConstructKey{});
    return *context;
}

AudioContext::GenerateSoundBufferResult
AudioContext::generate_sound_buffer(span<const std::byte> sound_file_data)
{
    ALuint al_buffer_id = 0;

    { // Read sound data with mutex locked, since libsndfile is not thread-safe.
        std::scoped_lock sndfile_lock{ impl().libsndfile_mutex };

        SndFileReader reader(sound_file_data);

        if (!reader.sndfile) {
            return { {}, reader.error_reason };
        }

        const auto [opt_al_buffer_id, openal_error_reason] =
            generate_OpenAL_buffer(reader.sndfile.get(), reader.sf_info);
        if (!opt_al_buffer_id) {
            return { {}, openal_error_reason };
        }

        al_buffer_id = opt_al_buffer_id.value();
    }

    std::scoped_lock sound_buffers_lock{ impl().sound_buffers_mutex };

    SoundBuffer& buffer = *impl().sound_buffers.emplace(al_buffer_id);
    return { SoundBufferHandle{ &buffer }, {} };
}

namespace {
std::string_view al_error_to_str(const ALenum error)
{
    switch (error) {
    case AL_NO_ERROR:
        return "AL_NO_ERROR";
    case AL_INVALID_NAME:
        return "AL_INVALID_NAME";
    case AL_INVALID_ENUM:
        return "AL_INVALID_ENUM";
    case AL_INVALID_VALUE:
        return "AL_INVALID_VALUE";
    case AL_INVALID_OPERATION:
        return "AL_INVALID_OPERATION";
    case AL_OUT_OF_MEMORY:
        return "AL_OUT_OF_MEMORY";
    default:
        return "UNKNOWN";
    }
}
} // namespace

void AudioContext::set_listener_state(const ListenerState& state)
{
    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);

    alListener3f(AL_VELOCITY, state.velocity.x, state.velocity.y, state.velocity.z);
    alListener3f(AL_POSITION, state.position.x, state.position.y, state.position.z);
    std::array<float, 6> orientation = { state.forward.x, state.forward.y, state.forward.z,
                                         state.up.x,      state.up.y,      state.up.z };
    alListenerfv(AL_ORIENTATION, orientation.data());

    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);
}

void AudioContext::check_for_errors() const
{
    const ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        log.error("Audio context: {}", al_error_to_str(error));
        throw RuntimeError{};
    }
}

void AudioContext::link_buffer_to_source(uintptr_t sound_source_id, const SoundBufferHandle& buffer)
{
    const ALuint al_buffer_id = buffer.m_ptr->al_buffer_id;
    alSourcei(as<ALuint>(sound_source_id), AL_BUFFER, as<ALint>(al_buffer_id));
    MG_ASSERT_DEBUG(alGetError() == AL_NO_ERROR);
}

void AudioContext::increment_ref_count(SoundBuffer* ptr)
{
    ++(ptr->ref_count);
}

void AudioContext::decrement_ref_count(SoundBuffer* ptr)
{
    const auto count = ptr->ref_count.fetch_sub(1);

    if (count == 1) {
        auto it = impl().sound_buffers.get_iterator_from_pointer(ptr);
        std::scoped_lock l{ impl().sound_buffers_mutex };
        impl().sound_buffers.erase(it);
    }
}

} // namespace Mg::audio
