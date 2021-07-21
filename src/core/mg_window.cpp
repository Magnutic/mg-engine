//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

#include "mg/core/mg_window.h"

#include "mg/containers/mg_small_vector.h"
#include "mg/core/mg_log.h"
#include "mg/core/mg_runtime_error.h"
#include "mg/gfx/mg_render_target.h"
#include "mg/mg_defs.h"
#include "mg/utils/mg_gsl.h"

#include <fmt/core.h>

#define GLFW_INCLUDE_NONE // Do not let GLFW include OpenGL headers.
#include <GLFW/glfw3.h>
#undef GLFW_INCLUDE_NONE

#include <mutex>

namespace Mg {

// Implementation details with internal linkage.
namespace {

//--------------------------------------------------------------------------------------------------
// GLFW Callbacks
//--------------------------------------------------------------------------------------------------

constexpr auto opengl_create_fail_msg =
    "Failed to create OpenGL context.\n"
    "This application requires an OpenGL 3.3 capable system. "
    "Please make sure your system has an up-to-date graphics driver.\n"
    "Error message: '{}'";

void glfw_error_callback(int error, const char* reason)
{
    if (error == GLFW_VERSION_UNAVAILABLE || error == GLFW_API_UNAVAILABLE) {
        log.error(opengl_create_fail_msg, reason);
    }
    else {
        log.error("GLFW error {}:\n%s", error, reason);
    }

    throw RuntimeError();
}

// GLFW callbacks provide the GLFW window handle, but we must forward to the appropriate instance
// of Mg::Window. For simplicity, we only support one window for now.
Window* s_window;

Window& window_from_glfw_handle(const GLFWwindow* handle) noexcept
{
    MG_ASSERT(s_window != nullptr);
    MG_ASSERT(s_window->glfw_window() == handle);
    return *s_window;
}

// Creates window with given video mode settings
GLFWwindow* create_window() noexcept
{
    log.verbose("Creating window");

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
void set_vsync(GLFWwindow* window, bool enable)
{
    // Update vsync settings, which is part of OpenGL context
    // Make context current for this thread and then switch back
    GLFWwindow* context = glfwGetCurrentContext();
    if (context != window) {
        glfwMakeContextCurrent(window);
    }

    log.message("{} vsync.", enable ? "Enabling" : "Disabling");
    glfwSwapInterval(enable ? 1 : 0);

    if (context != window) {
        glfwMakeContextCurrent(context);
    }
}

// Make sure settings are reasonable (fall back to defaults for invalid settings).
WindowSettings sanitise_settings(WindowSettings s)
{
    if (s.video_mode.width <= 0 || s.video_mode.height <= 0) {
        if (s.fullscreen && defs::k_default_to_desktop_res_in_fullscreen) {
            log.verbose("Mg::Window: no resolution specified, using desktop resolution.");
            s.video_mode = current_monitor_video_mode();
        }
        else {
            log.verbose("Mg::Window: no resolution specified, using defaults.");
            s.video_mode.width = defs::k_default_res_x;
            s.video_mode.height = defs::k_default_res_y;
        }
    }

    return s;
}

} // namespace

//--------------------------------------------------------------------------------------------------

/** Get video mode of primary monitor. */
VideoMode current_monitor_video_mode() noexcept
{
    glfwInit();
    const GLFWvidmode* gvm = glfwGetVideoMode(glfwGetPrimaryMonitor());
    return VideoMode{ gvm->width, gvm->height };
}

Array<VideoMode> find_available_video_modes()
{
    small_vector<VideoMode, 20> res;

    const auto vid_modes = []() -> span<const GLFWvidmode> {
        int n_modes;
        auto p_modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &n_modes);
        return { p_modes, narrow<size_t>(n_modes) };
    };

    // Iterate through modes and add relevant ones (24bpp) to list
    for (const auto& mode : vid_modes()) {
        // Ignore resolutions with weird bit-depths
        if (mode.redBits != 8 || mode.blueBits != 8 || mode.greenBits != 8) {
            continue;
        }

        VideoMode vm;
        vm.width = mode.width;
        vm.height = mode.height;

        res.push_back(vm);
    }

    return Array<VideoMode>::make_copy(res);
}

//--------------------------------------------------------------------------------------------------

std::unique_ptr<Window> Window::make(WindowSettings settings, std::string title)
{
    // glfwInit() may be called multiple times
    if (glfwInit() == GLFW_FALSE) {
        log.error("Window::make(): failed to initialise GLFW.");
        return {};
    }

    settings = sanitise_settings(settings);

    glfwSetErrorCallback(glfw_error_callback);

    GLFWwindow* window = create_window();

    if (window == nullptr) {
        return {};
    }

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

    // Sticky keys means that key state remains 'pressed' until their state has been polled, so
    // that key-presses in between polls do not go unnoticed.
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

    if (glfwRawMouseMotionSupported() != GLFW_FALSE) {
        log.message("Raw mouse-motion input enabled.");
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    else {
        log.message("Raw mouse-motion input unavailable.");
    }

    auto window_handle = std::make_unique<Window>(ConstructKey{}, window, settings);
    window_handle->apply_settings(settings);
    window_handle->set_title(std::move(title));

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
    log.message("Closing window '{}'.", m_title);

    glfwDestroyWindow(m_window);
    glfwTerminate();

    log.message("GLFW terminated.");
}

VideoMode Window::frame_buffer_size() const noexcept
{
    int width{};
    int height{};
    glfwGetFramebufferSize(m_window, &width, &height);
    return { width, height };
}

bool Window::should_close_flag() const noexcept
{
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::clear_should_close_flag() noexcept
{
    glfwSetWindowShouldClose(m_window, 0);
}

void Window::refresh() noexcept
{
    // Update GLFW window
    // TODO: this is OpenGL-specific
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
    log.message("Setting video mode: {}x{}, {}",
                m_settings.video_mode.width,
                m_settings.video_mode.height,
                m_settings.fullscreen ? "fullscreen" : "windowed");

    GLFWmonitor* monitor = m_settings.fullscreen ? glfwGetPrimaryMonitor() : nullptr;

    // Put window in centre of screen
    int window_pos_x{};
    int window_pos_y{};

    {
        const VideoMode vm = current_monitor_video_mode();
        window_pos_x = vm.width / 2 - m_settings.video_mode.width / 2;
        window_pos_y = vm.height / 2 - m_settings.video_mode.height / 2;
    }

    glfwSetWindowMonitor(m_window,
                         monitor,
                         window_pos_x,
                         window_pos_y,
                         m_settings.video_mode.width,
                         m_settings.video_mode.height,
                         GLFW_DONT_CARE); // Refresh rate: 'don't care' -> use highest available

    set_vsync(m_window, m_settings.vsync);
}

void Window::set_title(std::string title) noexcept
{
    m_title = std::move(title);
    glfwSetWindowTitle(m_window, m_title.c_str());
}

void Window::lock_cursor_to_window()
{
    if (!m_is_cursor_locked) {
        log.verbose("Window {} caught cursor.", static_cast<void*>(this));

        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        m_is_cursor_locked = true;
    }
}

void Window::grab_cursor()
{
    if (!is_cursor_locked_to_window() && get_cursor_lock_mode() == CursorLockMode::LOCKED) {
        lock_cursor_to_window();
    }
}

void Window::release_cursor()
{
    if (m_is_cursor_locked) {
        log.verbose("Window {} let go of cursor.", static_cast<void*>(this));

        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_is_cursor_locked = false;
    }
}

void Window::set_cursor_lock_mode(CursorLockMode mode) noexcept
{
    m_cursor_lock_mode = mode;
    if (m_cursor_lock_mode == CursorLockMode::LOCKED) {
        grab_cursor();
    }
    else {
        release_cursor();
    }
}

// Mark window as focused when user clicks within window
void Window::mouse_button_callback(int button, bool pressed)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || !pressed) {
        return;
    }

    grab_cursor();
}

void Window::focus_callback(bool focused)
{
    log.verbose("Window {} {} focus.", static_cast<void*>(this), focused ? "received" : "lost");

    // Release cursor if it was locked to this window
    if (is_cursor_locked_to_window() && !focused) {
        release_cursor();
    }

    // Forward to user-specified focus callback function.
    if (m_focus_callback) {
        m_focus_callback(focused);
    }
}

// Keep render target's size equal to framebuffer size.
// This has to be done in a callback from GLFW, because we cannot get the correct framebuffer size
// until _after_ video mode switch, which could, depending on system, execute asynchronously.
void Window::frame_buffer_size_callback(int width, int height)
{
    log.verbose(
        fmt::format("Setting window render target framebuffer size to: {}x{}", width, height));
    render_target.set_size(width, height);
    render_target.update_viewport();
}

// Check whether we got the requested video mode.
void Window::window_size_callback(int width, int height)
{
    VideoMode& conf = m_settings.video_mode;

    if (width != conf.width || height != conf.height) {
        log.warning(
            fmt::format("Failed to set requested video mode: {}x{}. Actual video mode: {}x{}.",
                        conf.width,
                        conf.height,
                        width,
                        height));

        conf.width = width;
        conf.height = height;
    }
    else {
        log.verbose("Video mode successfully set to: {}x{}", width, height);
    }
}

} // namespace Mg
