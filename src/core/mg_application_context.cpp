//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2022, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/mg_application_context.h"

#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/utils/mg_math_utils.h"
#include "../core/gfx/mg_opengl_loader_glad.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <numeric>
#include <thread>

namespace Mg {

using namespace std::chrono;
using namespace std::literals;
using Clock = std::chrono::high_resolution_clock;
using SecondsDouble = duration<double, seconds::period>;

struct ApplicationContext::Impl {
    time_point<Clock> start_time{};

    std::thread::id starting_thread_id = std::this_thread::get_id();

    std::atomic_bool main_loop_is_running = false;
    std::atomic_bool main_loop_should_stop = false;

    mutable std::mutex time_info_mutex;
    ApplicationTimeInfo time_info = {};
};

ApplicationContext::ApplicationContext() = default;
ApplicationContext::~ApplicationContext() = default;

ApplicationTimeInfo ApplicationContext::time_info() const
{
    std::lock_guard g{ m_impl->time_info_mutex };
    return m_impl->time_info;
};

void ApplicationContext::run_main_loop(IApplication& application)
{
    if (m_impl->main_loop_is_running.exchange(true)) {
        throw RuntimeError{ "ApplicationContext::run_main_loop: main loop already running." };
    }

    m_impl->start_time = Clock::now();

    log.message("Starting main loop.");

    double step_accumulator = 0.0;
    double render_accumulator = 0.0;

    // Track frame time for the most recent frames. We calculate the frame-rate using the mean of
    // these samples.
    std::array<double, 60> frame_time_samples = {};
    frame_time_samples.fill(0.0);
    size_t frame_time_sample_index = 0;

    double last_loop_time = 0.0f;
    double last_render_time = 0.0f;

    // Run two simulation steps before rendering. One for initializing before starting to render,
    // and a second to have two valid results to interpolate between.
    application.simulation_step(m_impl->time_info);
    application.simulation_step(m_impl->time_info);

    for (;;) {
        // Check if it is time to stop.
        {
            // If stopped by m_impl->main_loop_should_stop, we also reset that variable.
            bool value_if_stopped = true;
            const bool was_stopped_from_outside =
                m_impl->main_loop_should_stop.compare_exchange_weak(value_if_stopped, false);

            if (was_stopped_from_outside || application.should_exit()) {
                break;
            }
        }

        {
            std::lock_guard g{ m_impl->time_info_mutex };
            m_impl->time_info.time_since_init =
                SecondsDouble(Clock::now() - m_impl->start_time).count();
        }

        const double time = m_impl->time_info.time_since_init;
        const double loop_time_delta = time - last_loop_time;
        last_loop_time = time;

        // Get update settings. Note that we do this every iteration, in case the application
        // changes its settings.
        const auto settings = application.update_timer_config();

        // Minimum time between rendering updates.
        const double min_frame_time = settings.max_frames_per_second > 0
                                          ? 1.0 / settings.max_frames_per_second
                                          : 0.0;

        // Time between simulation updates.
        const double simulation_time_step = 1.0 / settings.simulation_steps_per_second;

        step_accumulator += loop_time_delta;
        render_accumulator += loop_time_delta;
        step_accumulator = min(step_accumulator,
                               settings.max_time_steps_at_once * simulation_time_step);

        // Make one or more simulation updates, if enough time has passed.
        const bool should_run_simulation_step = step_accumulator >= simulation_time_step;
        if (should_run_simulation_step) {
            while (step_accumulator >= simulation_time_step) {
                application.simulation_step(m_impl->time_info);
                step_accumulator -= simulation_time_step;
            }
        }

        // Render the scene, unless limited by framerate cap.
        const bool should_render = render_accumulator > min_frame_time &&
                                   (settings.decouple_rendering_from_time_step ||
                                    should_run_simulation_step);
        if (should_render) {
            const auto interpolation_factor = settings.decouple_rendering_from_time_step
                                                  ? step_accumulator / simulation_time_step
                                                  : 0.0;
            application.render(interpolation_factor, m_impl->time_info);
            render_accumulator = 0.0;

            ++frame_time_sample_index;
            frame_time_sample_index %= frame_time_samples.size();

            const double frame_time_delta = time - last_render_time;
            last_render_time = time;
            frame_time_samples[frame_time_sample_index] = frame_time_delta;

            // Update performance info.
            {
                std::lock_guard g{ m_impl->time_info_mutex };
                m_impl->time_info.last_frame_time_seconds = frame_time_delta;
                m_impl->time_info.frames_per_second =
                    1.0 /
                    (std::accumulate(frame_time_samples.begin(), frame_time_samples.end(), 0.0) /
                     frame_time_samples.size());
            }
        }
        else {
            // If we are limited by framerate cap, let the thread sleep for a while to not saturate
            // the CPU by spinning this loop.
            const double time_until_next_frame_us =
                max(0.0, (min_frame_time - render_accumulator) * 1'000'000.0);
            const double time_until_next_step_us =
                max(0.0, (simulation_time_step - step_accumulator) * 1'000'000.0);
            const auto wait_time = Mg::min(Mg::min(microseconds(uint64_t(time_until_next_frame_us)),
                                                   microseconds(uint64_t(time_until_next_step_us))),
                                           50us);
            std::this_thread::sleep_for(wait_time);
        }
    }

    // Main loop is over. Revert status boolean.
    bool expected_main_loop_is_running = true;
    const bool reverted =
        m_impl->main_loop_is_running.compare_exchange_weak(expected_main_loop_is_running, false);
    MG_ASSERT(reverted);

    log.message("Exiting main loop.");
}

void ApplicationContext::stop_main_loop()
{
    m_impl->main_loop_should_stop = true;
}

} // namespace Mg
