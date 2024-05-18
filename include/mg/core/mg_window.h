//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_window.h
 * Window handling.
 */

#pragma once

#include "mg/containers/mg_array.h"
#include "mg/core/mg_window_settings.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_observer.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

struct GLFWwindow;

namespace Mg::input {
class IButtonEventHandler;
class IMouseMovementEventHandler;
} // namespace Mg::input

namespace Mg {

/* Find all available screen resolutions.
 * This could be useful for e.g. listing choices in a display options menu.
 */
Array<VideoMode> find_available_video_modes();

/** Get video mode of primary monitor. */
VideoMode current_monitor_video_mode() noexcept;

/** Whether or not cursor is locked into the window when it is focused.
 * Locked results in cursor being invisible and forced to stay within the window.
 */
enum class CursorLockMode { UNLOCKED, LOCKED };

/** Window handling class. Presently, there is no support for multiple windows. */
class Window {
    struct ConstructKey {}; // Limits access to Window constructor

public:
    /** Callback to invoke when window gains or loses focus.
     * Parameters: bool is_focused, whether the window is focused.
     */
    using FocusCallbackT = std::function<void(bool)>;

    /** Create new window. */
    static std::unique_ptr<Window> make(WindowSettings settings, std::string title);

    /** Restricted constructor (see Window::make()). */
    explicit Window(ConstructKey, GLFWwindow* handle, WindowSettings settings);

    ~Window();

    // Do not allow copying or moving: Window must be findable by address.
    MG_MAKE_NON_COPYABLE(Window);
    MG_MAKE_NON_MOVABLE(Window);

    /** Call at end of frame to display image to window. */
    void swap_buffers() noexcept;

    /** Set window title. */
    void set_title(std::string title) noexcept;

    /** Get whether window is currently fullscreen. */
    bool is_fullscreen() const noexcept { return m_settings.fullscreen; }

    /** Get the size of the current window's frame buffer in pixels.
     * N.B. frame buffer size is not necessarily the same as window size.
     */
    VideoMode frame_buffer_size() const noexcept;

    /** Set callback function to invoke when window focus is gained or lost.
     * @see Mg::Window::FocusCallbackT
     */
    void set_focus_callback(FocusCallbackT func) noexcept { m_focus_callback = std::move(func); }

    /** Get the callback function which is invoked when window focus is gained or lost.
     * @see Mg::Window::FocusCallbackT
     */
    FocusCallbackT get_focus_callback() const noexcept { return m_focus_callback; }

    /** Get the value should-close flag, which is true when the user e.g. presses alt-f4 or the
     * window close button.
     */
    bool should_close_flag() const noexcept;

    /** Unset the should-close flag if it was set. */
    void clear_should_close_flag() noexcept;

    //----------------------------------------------------------------------------------------------
    // Cursor state

    bool is_cursor_locked_to_window() const noexcept { return m_is_cursor_locked; }

    /** Locks cursor to window if it was not locked and CursorLockMode is set to LOCKED. */
    void grab_cursor();

    /** Releases cursor if it was locked to this window. */
    void release_cursor();

    void set_cursor_lock_mode(CursorLockMode mode) noexcept;

    CursorLockMode get_cursor_lock_mode() const noexcept { return m_cursor_lock_mode; }

    //----------------------------------------------------------------------------------------------
    // Window settings

    /** Get current aspect ratio (width / height) of the window. */
    float aspect_ratio() const noexcept
    {
        return narrow_cast<float>(m_settings.video_mode.width) /
               narrow_cast<float>(m_settings.video_mode.height);
    }

    /** Get (const reference to) settings struct for this Window. */
    const WindowSettings& settings() const noexcept { return m_settings; }

    /** Observe changes to window settings. */
    void observe_settings(Observer<WindowSettings>& observer)
    {
        m_window_settings_subject.add_observer(observer);
    }

    /** Set the settings for this Window. Takes immediate effect. */
    void apply_settings(WindowSettings s);

    //----------------------------------------------------------------------------------------------

    /** Polls input events for this window. Should be done every frame. */
    void poll_input_events();

    void register_button_event_handler(input::IButtonEventHandler& handler);
    void deregister_button_event_handler(input::IButtonEventHandler& handler);

    void register_mouse_movement_event_handler(input::IMouseMovementEventHandler& handler);
    void deregister_mouse_movement_event_handler(input::IMouseMovementEventHandler& handler);

    /** Render target for this window. */
    gfx::WindowRenderTarget render_target{};

    /** Get underlying GLFW window handle. */
    GLFWwindow* glfw_window() const noexcept { return m_window; }

private:
    void mouse_button_callback(int button, int action, int mods);
    void cursor_position_callback(float x, float y);
    void key_callback(int key, int scancode, int action, int mods);
    void focus_callback(bool focused);
    void frame_buffer_size_callback(int width, int height);
    void window_size_callback(int width, int height);

    /** Resets window, applying current window settings. */
    void reset();
    void lock_cursor_to_window();

private:
    FocusCallbackT m_focus_callback{};
    Subject<WindowSettings> m_window_settings_subject;
    WindowSettings m_settings;
    std::string m_title;

    GLFWwindow* m_window = nullptr;

    std::vector<input::IButtonEventHandler*> m_button_event_handlers;
    std::vector<input::IMouseMovementEventHandler*> m_mouse_movement_event_handlers;

    CursorLockMode m_cursor_lock_mode = CursorLockMode::UNLOCKED;
    bool m_is_cursor_locked = false;
};

} // namespace Mg
