//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2021, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_application_context.h
 * The ApplicationContext is a framework for running an Mg Engine application with its own window,
 * configuration, and update loops. It handles timing for logical time step updates ("game ticks")
 * and rendering events (frames).
 */

#pragma once

#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_simple_pimpl.h"

#include <string_view>

namespace Mg {
class Config;
class Window;
} // namespace Mg

namespace Mg::gfx {
class GfxDevice;
}

namespace Mg {

/** Settings controlling main loop updates. */
struct UpdateTimerSettings {
    /** Number of invocations to `IApplication::simulation_step` per second. */
    int simulation_steps_per_second = 60;

    /** Maximum number of invocations to `IApplication::render` per second.
     * 0 means no limit.
     */
    int max_frames_per_second = 0;

    /** When `decouple_rendering_from_time_step` is true, the `IApplication::render` is called as
     * often as possible (unless limited by `max_frames_per_second`), whereas
     * `IApplication::simulation_step` will only be called `simulation_steps_per_second`. When
     * `decouple_rendering_from_time_step` is false, an invocation to
     * `IApplication::simulation_step` is always immediately followed by an invocation to
     * `IApplication::render`, meaning that rendering is fully synchronized to simulation time
     * steps.
     */
    bool decouple_rendering_from_time_step = true;

    /** Maximum number of invocations to `IApplication::simulation_step` that may be performed at
     * once. If so much time has passed since last update that the simulation should advance by more
     * steps than this, `max_time_steps_at_once` limits the number of updates. This is useful for
     * two reasons:
     *  1. If the application freezes for some reason, the simulation should not race too far ahead
     *     when execution resumes.
     *  2. If performing the simulation updates takes too much time, so that each update takes more
     *     time than `1.0 / simulation_steprper_second`, then without this limit, the application
     *     would be stuck in a simulation-step loop forever.
     */
    int max_time_steps_at_once = 10;
};

/** Interface for Mg Engine applications. Implementing this interface allows the use of
 * `ApplicationContext` to run the application, invoking the `simulation_step` and `render` member
 * functions according to the `update_timer_settings`.
 * @seealso ApplicationContext
 */
class IApplication {
public:
    MG_INTERFACE_BOILERPLATE(IApplication);

    /** Simulation step function. Advances the simulation by one time-step. */
    virtual void simulation_step() = 0;

    /** Rendering update function. The interpolation_factor is the proportion of a time-step
     * duration that has passed since the last invocation to simulation_step, which is useful for
     * interpolating the world state to create a smooth visualization.
     */
    virtual void render(double interpolation_factor) = 0;

    /** If this returns true, the simulation loop ends. */
    virtual bool should_exit() const = 0;

    /** Settings controlling update timing settings. Changes to this value will take effect
     * immediately.
     */
    virtual UpdateTimerSettings update_timer_settings() const = 0;
};

struct PerformanceInfo {
    double frames_per_second = 0.0;
    double last_frame_time_seconds = 0.0;
};

struct ApplicationContextData;

/* The `ApplicationContext` is a framework for running an Mg Engine application, holding a window,
 * configuration, and handling the application update loops. It handles timing for logical time step
 * updates ("game ticks") and rendering events (frames).
 * @seealso IApplication
 */
class ApplicationContext : PImplMixin<ApplicationContextData> {
public:
    explicit ApplicationContext(std::string_view config_file_path);
    ~ApplicationContext();

    MG_MAKE_NON_COPYABLE(ApplicationContext);
    MG_MAKE_NON_MOVABLE(ApplicationContext);

    Window& window();
    const Window& window() const;

    Config& config();
    const Config& config() const;

    gfx::GfxDevice& gfx_device();
    const gfx::GfxDevice& gfx_device() const;

    double time_since_init() noexcept;

    PerformanceInfo performance_info() const;

    /** Run main loop. Must be called from main thread. */
    void run_main_loop(IApplication& application);

    /** May be called from another thread to stop a running loop. */
    void stop_main_loop();
};

} // namespace Mg
