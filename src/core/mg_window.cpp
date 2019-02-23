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

#include "mg/core/mg_window.h"

#include <mutex>
#include <stdexcept>

#define GLFW_INCLUDE_NONE // Do not let GLFW include OpenGL headers.
#include <GLFW/glfw3.h>
#undef GLFW_INCLUDE_NONE

#include <fmt/core.h>

#include "mg/core/mg_log.h"
#include "mg/core/mg_root.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/mg_defs.h"
#include "mg/utils/mg_gsl.h"

namespace Mg {

static void glfw_error_callback(int error, const char* msg);

/** Get video mode of primary monitor. */
VideoMode current_monitor_video_mode()
{
    glfwInit();
    const GLFWvidmode* gvm = glfwGetVideoMode(glfwGetPrimaryMonitor());
    return VideoMode{ gvm->width, gvm->height };
}

std::vector<VideoMode> find_available_video_modes()
{
    std::vector<VideoMode> res;

    auto vid_modes = []() -> span<const GLFWvidmode> {
        int  n_modes;
        auto p_modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &n_modes);
        return { p_modes, narrow<size_t>(n_modes) };
    };

    // Iterate through modes and add relevant ones (24bpp) to list
    for (const auto& mode : vid_modes()) {
        // Ignore resolutions with weird bit-depths
        if (mode.redBits != 8 || mode.blueBits != 8 || mode.greenBits != 8) { continue; }

        VideoMode vm;
        vm.width  = mode.width;
        vm.height = mode.height;

        res.push_back(vm);
    }

    return res;
}

// GLFW callbacks provide the GLFW window handle, but we must forward to the appropriate instance
// of Mg::Window. For simplicity, we only support one window for now.

static Window* s_window;

static Window& window_from_glfw_handle(GLFWwindow* handle)
{
    MG_ASSERT(s_window != nullptr);
    MG_ASSERT(s_window->glfw_window() == handle);
    return *s_window;
}

// Creates window with given video mode settings
static GLFWwindow* create_window()
{
    g_log.write_verbose("Creating window");

#ifndef NDEBUG
    // Enabling OpenGL debug context enables the application to receive debug /
    // error messages from the OpenGL implementation.
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
#endif

    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_RESIZABLE, 0);
    // Supposedly necessary for OSX:
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE); // 'Don't care' -> use highest available

    // These settings are all temporary and will be overridden
    return glfwCreateWindow(1024, 768, "", nullptr, nullptr);
}

// Set whether to use VSync for the graphics context associated with this Window.
static void set_vsync(GLFWwindow* window, bool enable)
{
    // Update vsync settings, which is part of OpenGL context
    // Make context current for this thread and then switch back
    GLFWwindow* context = glfwGetCurrentContext();
    if (context != window) { glfwMakeContextCurrent(window); }

    g_log.write_message(fmt::format("{} vsync.", enable ? "Enabling" : "Disabling"));
    glfwSwapInterval(enable ? 1 : 0);

    if (context != window) { glfwMakeContextCurrent(context); }
}

// Make sure settings are reasonable (fall back to defaults for invalid settings).
static WindowSettings sanitise_settings(WindowSettings s)
{
    if (s.video_mode.width <= 0 || s.video_mode.height <= 0) {
        if (s.fullscreen && defs::k_default_to_desktop_res_in_fullscreen) {
            g_log.write_verbose("Mg::Window: no resolution specified, using desktop resolution.");
            s.video_mode = current_monitor_video_mode();
        }
        else {
            g_log.write_verbose("Mg::Window: no resolution specified, using defaults.");
            s.video_mode.width  = defs::k_default_res_x;
            s.video_mode.height = defs::k_default_res_y;
        }
    }

    return s;
}

//--------------------------------------------------------------------------------------------------
// Window implementation
//--------------------------------------------------------------------------------------------------

Ptr<Window> Window::make(WindowSettings settings, const std::string& title)
{
    // glfwInit() may be called multiple times
    if (glfwInit() == GLFW_FALSE) {
        g_log.write_error("Window::make(): failed to initialise GLFW.");
        return {};
    }

    settings = sanitise_settings(settings);

    glfwSetErrorCallback(glfw_error_callback);

    GLFWwindow* window = create_window();

    if (window == nullptr) { return {}; }

    set_vsync(window, settings.vsync);

    // Set up callbacks

    glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int act, int) {
        window_from_glfw_handle(w).mouse_button_callback(button, act == GLFW_PRESS);
    });

    glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) {
        window_from_glfw_handle(w).focus_callback(focused != 0);
    });

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
        window_from_glfw_handle(w).frame_buffer_size_callback(width, height);
    });

    glfwSetWindowSizeCallback(window, [](GLFWwindow* w, int width, int height) {
        window_from_glfw_handle(w).window_size_callback(width, height);
    });

    auto window_handle = Ptr<Window>::make(ConstructKey{}, window, settings);
    window_handle->apply_settings(settings);
    window_handle->set_title(title);

    return window_handle;
}

Window::Window(ConstructKey /*unused*/, GLFWwindow* handle, WindowSettings settings)
    : keyboard{ *this }, mouse{ *this }, m_settings{ settings }, m_window{ handle }
{
    MG_ASSERT(s_window == nullptr && "Only one Mg::Window may exist at a time.");
    s_window = this;
    render_target.set_size(m_settings.video_mode.width, m_settings.video_mode.height);
}

Window::~Window()
{
    g_log.write_message(fmt::format("Closing window at {}", static_cast<void*>(this)));
    glfwDestroyWindow(m_window);
}

VideoMode Window::frame_buffer_size() const
{
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    return { width, height };
}

void Window::refresh()
{
    // Update GLFW window
    glfwSwapBuffers(m_window);
}

void Window::poll_input_events()
{
    glfwPollEvents();
    mouse.refresh();
    keyboard.refresh();
}

void Window::apply_settings(WindowSettings s)
{
    m_settings = sanitise_settings(s);
    reset();
}

// Reset window, applying new settings
void Window::reset()
{
    g_log.write_message(fmt::format("Setting video mode: {}x{}, {}",
                                    m_settings.video_mode.width,
                                    m_settings.video_mode.height,
                                    m_settings.fullscreen ? "fullscreen" : "windowed"));

    GLFWmonitor* monitor = m_settings.fullscreen ? glfwGetPrimaryMonitor() : nullptr;

    // Put window in centre of screen
    int window_pos_x;
    int window_pos_y;

    {
        VideoMode vm = current_monitor_video_mode();
        window_pos_x = vm.width / 2 - m_settings.video_mode.width / 2;
        window_pos_y = vm.height / 2 - m_settings.video_mode.height / 2;
    }

    glfwSetWindowMonitor(m_window,
                         monitor,
                         window_pos_x,
                         window_pos_y,
                         m_settings.video_mode.width,
                         m_settings.video_mode.height,
                         GLFW_DONT_CARE); // 'don't care' -> use highest available

    set_vsync(m_window, m_settings.vsync);
}

void Window::set_title(const std::string& title)
{
    glfwSetWindowTitle(m_window, title.c_str());
}

//--------------------------------------------------------------------------------------------------
// GLFW Callbacks
//--------------------------------------------------------------------------------------------------

static void glfw_error_callback(int error, const char* msg)
{
    if (error == GLFW_VERSION_UNAVAILABLE || error == GLFW_API_UNAVAILABLE) {
        throw std::runtime_error(
            fmt::format("Failed to create OpenGL context.\n"
                        "This application requires an OpenGL 3.3 capable system. "
                        "Please make sure your system has an up-to-date graphics driver.\n"
                        "Error message: '{}'",
                        msg));
    }

    throw std::runtime_error(fmt::format("GLFW error {}:\n%s", error, msg));
}

void Window::lock_cursor_to_window()
{
    g_log.write_verbose(fmt::format("Window {} caught cursor.", static_cast<void*>(this)));

    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    m_is_cursor_locked = true;
}

void Window::release_cursor()
{
    g_log.write_verbose(fmt::format("Window {} let go of cursor.", static_cast<void*>(this)));

    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    m_is_cursor_locked = false;
}

// Mark window as focused when user clicks within window
void Window::mouse_button_callback(int button, bool pressed)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || !pressed) { return; }

    if (!is_cursor_locked_to_window() && get_cursor_lock_mode() == CursorLockMode::LOCKED) {
        lock_cursor_to_window();
    }
}

void Window::focus_callback(bool focused)
{
    g_log.write_verbose(fmt::format("Window {} {} focus.",
                                    static_cast<void*>(this),
                                    focused ? "received" : "lost"));

    // Release cursor if it was locked to this window
    if (is_cursor_locked_to_window() && !focused) { release_cursor(); }

    // Forward to user-specified focus callback function.
    if (m_focus_callback) { m_focus_callback(focused); }
}

// Keep render target's size equal to framebuffer size.
// This has to be done in a callback from GLFW, because we cannot get the correct framebuffer size
// until _after_ video mode switch, which could, depending on system, execute asynchronously.
void Window::frame_buffer_size_callback(int width, int height)
{
    g_log.write_verbose(
        fmt::format("Setting window render target framebuffer size to: {}x{}", width, height));
    render_target.set_size(width, height);
    render_target.update_viewport();
}

// Check whether we got the requested video mode.
void Window::window_size_callback(int width, int height)
{
    VideoMode& conf = m_settings.video_mode;

    if (width != conf.width || height != conf.height) {
        g_log.write_warning(
            fmt::format("Failed to set requested video mode: {}x{}. Actual video mode: {}x{}.",
                        conf.width,
                        conf.height,
                        width,
                        height));

        conf.width  = width;
        conf.height = height;
    }
    else {
        g_log.write_verbose(fmt::format("Video mode successfully set to: {}x{}", width, height));
    }
}

} // namespace Mg
