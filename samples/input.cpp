#include <sstream>

#include <fmt/core.h>

#include <mg/core/mg_log.h>
#include <mg/core/mg_window.h>
#include <mg/input/mg_input.h>

#include <mg/input/mg_joy.h>
#include <mg/input/mg_keyboard.h>
#include <mg/input/mg_mouse.h>

void inputTest()
{
    using namespace Mg;

    std::unique_ptr window = Window::make({}, "Input test");

    input::InputMap  kb_map;
    input::InputMap  mouse_map;
    input::Keyboard& kb = window->keyboard;
    input::Mouse& mouse = window->mouse;

    // Funny jump-loop to bind all keys
    auto bindId = [&](size_t i) {
        std::ostringstream ss;
        ss << i;
        auto command = Identifier::from_runtime_string(ss.str());

        auto key = input::Keyboard::Key(i);
        kb_map.bind(command, kb.key(key));
    };

    for (size_t i = 0; i < input::Keyboard::k_num_keys; ++i) { bindId(i); }

    kb_map.bind("quit", kb.key(input::Keyboard::Key::Esc));

    kb_map.bind("mouse1", mouse.button(input::Mouse::Button::left));
    kb_map.bind("mouse2", mouse.button(input::Mouse::Button::right));
    kb_map.bind("mouse3", mouse.button(input::Mouse::Button::middle));
    kb_map.bind("mouse4", mouse.button(input::Mouse::Button::button4));
    kb_map.bind("mouse5", mouse.button(input::Mouse::Button::button5));
    kb_map.bind("mouse6", mouse.button(input::Mouse::Button::button6));
    kb_map.bind("mouse7", mouse.button(input::Mouse::Button::button7));

    mouse_map.bind("xPos", mouse.axis(input::Mouse::Axis::pos_x));
    mouse_map.bind("yPos", mouse.axis(input::Mouse::Axis::pos_y));
    mouse_map.bind("xDelta", mouse.axis(input::Mouse::Axis::delta_x));
    mouse_map.bind("yDelta", mouse.axis(input::Mouse::Axis::delta_y));

    /*
    auto joyInfoSpan = input::getConnectedJoyInfo();

    for ( const input::JoyInfo& ji : joyInfoSpan ) {
        for ( size_t i = 0; i < ji.numAxes; ++i ) {
            input::JoyAxis axis{ji.joyId, i};
            Identifier     command{axis.description()};
            kb_map.bind( command, axis );
        }

        for ( size_t i = 0; i < ji.numButtons; ++i ) {
            input::JoyButton button{ji.joyId, i};
            Identifier       command{button.description()};
            kb_map.bind( command, button );
        }
    }
    */

    while (true) {
        window->refresh();
        window->poll_input_events();
        kb_map.refresh();
        mouse_map.refresh();

        bool xMoved     = mouse_map.was_released("xDelta");
        bool yMoved     = mouse_map.was_released("yDelta");
        bool isMoving   = mouse_map.is_held("xDelta") || mouse_map.is_held("yDelta");
        bool mouseMoved = (xMoved || yMoved) && !isMoving;

        if (mouseMoved) {
            g_log.write_message(fmt::format(
                "Mouse pos: ({}, {})", mouse_map.state("xPos"), mouse_map.state("yPos")));
        }

        for (Identifier& command : kb_map.commands()) {
            if (kb_map.was_pressed(command)) {
                g_log.write_message(
                    fmt::format("Pressed: {}", kb_map.binding(command).description()));
            }
            else if (kb_map.was_released(command)) {
                g_log.write_message(
                    fmt::format("Released: {}", kb_map.binding(command).description()));
            }
        }

        if (kb_map.is_held("quit")) break;
    }
}

int main()
{
    inputTest();
}

