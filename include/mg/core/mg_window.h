//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
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

/** @file mg_window.h
 * Window handling.
 */

#pragma once

#include "mg/core/mg_window_settings.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/input/mg_keyboard.h"
#include "mg/input/mg_mouse.h"
#include "mg/utils/mg_pointer.h"

#include <cstdint>
#include <string>
#include <vector>

struct GLFWwindow;

//--------------------------------------------------------------------------------------------------

namespace Mg {

/* Find all available screen resolutions.
 * This could be useful for e.g. listing choices in a display options menu.
 */
std::vector<VideoMode> find_available_video_modes();

/** Get video mode of primary monitor. */
VideoMode current_monitor_video_mode();

//--------------------------------------------------------------------------------------------------

/** Whether or not cursor is locked into the window when it is focused.
 * Locked results in cursor being invisible and forced to stay within the window.
 */
enum class CursorLockMode { UNLOCKED, LOCKED };

/** Window handling class. Presently, there is no support for multiple windows. */
class Window {
    struct ConstructKey {}; // Limits access to Window constructor

public:
    /** Owning handle for Window. */
    using Handle = Ptr<Window>;

    /** Callback to invoke when window gains or loses focus.
     * Parameters: bool is_focused, whether the window is focused.
     */
    using FocusCallbackT = void (*)(bool);

    /** Create new window. */
    static std::optional<Handle> make(WindowSettings settings, const std::string& title);

    /** Restricted constructor (see Window::make()). */
    explicit Window(ConstructKey, GLFWwindow* handle, WindowSettings settings);

    ~Window();

    // Do not allow copying or moving: Window must be findable by address.
    Window(const Window&) = delete;
    Window(Window&&)      = delete;

    /** Call at end of frame to display image to window. */
    void refresh();

    /** Polls input events for this window, refreshing `keyboard` and `mouse` members. */
    void poll_input_events();

    /** Set window title. */
    void set_title(const std::string& title);

    /** Get whether window is currently fullscreen. */
    bool is_fullscreen() const { return m_settings.fullscreen; }

    /** Get the size of the current window's frame buffer in pixels.
     * N.B. frame buffer size is not necessarily the same as window size.
     */
    VideoMode frame_buffer_size() const;

    /** Set callback function to invoke when window focus is gained or lost.
     * @see Mg::Window::FocusCallbackT
     */
    void set_focus_callback(FocusCallbackT func) { m_focus_callback = func; }

    /** Get the callback function which is invoked when window focus is gained or lost.
     * @see Mg::Window::FocusCallbackT
     */
    FocusCallbackT get_focus_callback() const { return m_focus_callback; }

    //----------------------------------------------------------------------------------------------
    // Cursor state

    bool is_cursor_locked_to_window() const { return m_is_cursor_locked; }

    /** Releases cursor if it was locked to this window. */
    void release_cursor();

    void set_cursor_lock_mode(CursorLockMode mode) { m_cursor_lock_mode = mode; }

    CursorLockMode get_cursor_lock_mode() const { return m_cursor_lock_mode; }

    //----------------------------------------------------------------------------------------------
    // Window settings

    /** Get current aspect ratio (width / height) of the window. */
    float aspect_ratio() const
    {
        return float(m_settings.video_mode.width) / float(m_settings.video_mode.height);
    }

    /** Get (const reference to) settings struct for this Window. */
    const WindowSettings& settings() const { return m_settings; }

    /** Set the settings for this Window. Takes immediate effect. */
    void apply_settings(WindowSettings s);

    /** Get underlying GLFW window handle. Used by input system. */
    GLFWwindow* glfw_window() const { return m_window; } // TODO: fix abstraction leak somehow

    /** Keyboard input device associated with this Window. */
    input::Keyboard keyboard;

    /** Mouse input device associated with this Window. */
    input::Mouse mouse;

    gfx::WindowRenderTarget render_target{};

private:
    void mouse_button_callback(int button, bool pressed);
    void focus_callback(bool focused);
    void frame_buffer_size_callback(int width, int height);
    void window_size_callback(int width, int height);

    /** Resets window, applying current window settings. */
    void reset();

    void lock_cursor_to_window();

private:
    FocusCallbackT m_focus_callback;
    WindowSettings m_settings;

    GLFWwindow* m_window;

    CursorLockMode m_cursor_lock_mode = CursorLockMode::UNLOCKED;
    bool           m_is_cursor_locked = false;
};

} // namespace Mg
