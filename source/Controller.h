#pragma once
#define DIRECTINPUT_VERSION 0x0500
#include <dinput.h>

namespace Controller
{
    constexpr uint16_t Menu = 0x08, Up = 0x10, Right = 0x20, Down = 0x40, Left = 0x80, CameraLeft = 0x100, CameraRight = 0x200, Cancel = 0x1000, Jump = 0x4000;
    constexpr int DpadUp = 0x422, DpadRight = 0x423, DpadDown = 0x424, DpadLeft = 0x425; // pseudo-keys after the 32 joystick buttons

    struct Binding { int key; uint32_t actions; };

    Binding* bindings; // 38 entries, key -1 = empty
    DIJOYSTATE* joystick;
    bool valid; // the joystick buffer holds a fresh sample
    uint16_t* actions; // actions pressed this frame
    int32_t* movement; // X, Y walking input; the game writes it only while the left stick is deflected
    uint32_t* directions; // Up/Down/Left/Right from the left stick
    uint32_t* capturing; // controls menu is waiting for a key
    uint16_t camera; // right stick state, kept between frames for hysteresis
    int (__cdecl* capture)();
    const char* (__cdecl* bindingName)(int key);
    int (__cdecl* configure)();
    uint8_t (__cdecl* keyPressed)(uint32_t key);

    struct
    {
        bool active, cancel, closing;
        uint64_t sources, held; // buttons and D-pad down; inputs still held from the press that opened the capture step
        uint16_t directions, pressed; // actions synthesized for the menu this frame
        DWORD repeatAt;
    } menu;

    unsigned Dpad(DWORD pov) // 1 = up, 2 = right, 4 = down, 8 = left
    {
        constexpr unsigned masks[] = {1, 3, 2, 6, 4, 12, 8, 9};
        return pov < 36000 ? masks[((pov + 2250) / 4500) & 7] : 0;
    }

    uint64_t Sources() // buttons in bits 0-31, D-pad in bits 32-35
    {
        if (!valid)
            return 0;
        uint64_t sources = (uint64_t)Dpad(joystick->rgdwPOV[0]) << 32;
        for (int i = 0; i < 32; i++)
            if (joystick->rgbButtons[i]) sources |= 1ull << i;
        return sources;
    }

    // Replaces the joystick GetDeviceState call: adds right stick camera control and D-pad bindings
    HRESULT WINAPI ReadJoystick(IDirectInputDevice2A* device, DWORD size, DIJOYSTATE* state)
    {
        HRESULT result = device->GetDeviceState(size, state);
        valid = SUCCEEDED(result);
        if (!valid)
            return result;
        int rx = state->lRx - 0x8000, engage = 0x8000 * 35 / 100, release = 0x8000 * 25 / 100; // 0-65535 axis range, as the game assumes for X/Y
        if (rx <= -engage) camera = CameraLeft;
        else if (rx >= engage) camera = CameraRight;
        else if (rx > -release && rx < release) camera = 0;
        *actions |= camera;
        unsigned hat = Dpad(state->rgdwPOV[0]);
        uint16_t dpad = 0;
        for (int i = 0; i < 38; i++)
            if (bindings[i].key >= DpadUp && bindings[i].key <= DpadLeft && (hat & (1 << (bindings[i].key - DpadUp))))
                dpad |= bindings[i].actions;
        *actions |= dpad;
        if (dpad & (Up | Right | Down | Left)) { // walk at full speed like the arrow keys; a deflected stick still overrides this
            movement[0] = ((dpad & Right) ? 0x8000 : 0) - ((dpad & Left) ? 0x8000 : 0);
            movement[1] = ((dpad & Up) ? 0x8000 : 0) - ((dpad & Down) ? 0x8000 : 0);
        }
        return result;
    }

    // Wraps the controls menu update: A changes, Start accepts, Cancel cancels, stick or D-pad up/down navigate
    int __cdecl Configure()
    {
        uint64_t sources = Sources(), pressed = sources & ~menu.sources;
        DWORD now = GetTickCount();
        uint16_t navigation = valid ? (*directions & (Up | Down)) | ((sources >> 32 & 1) ? Up : 0) | ((sources >> 32 & 4) ? Down : 0) : 0;
        menu.pressed = ((pressed & 1) ? Jump : 0) | ((pressed & 0x80) ? Menu : 0); // A, Start
        if (navigation != menu.directions) {
            menu.pressed |= navigation & ~menu.directions;
            menu.repeatAt = now + 350;
        } else if (navigation && (int)(now - menu.repeatAt) >= 0) {
            menu.pressed |= navigation;
            menu.repeatAt = now + 120;
        }
        bool cancel = (*actions & Cancel) != 0;
        if (cancel && !menu.cancel)
            menu.pressed |= Cancel;
        if (!menu.active) // first frame: inputs held from the previous menu are not presses
            menu.pressed = 0;
        menu.active = true;
        menu.cancel = cancel;
        menu.closing = false;
        menu.sources = sources;
        menu.directions = navigation;
        bool wasCapturing = *capturing != 0;
        int result = configure();
        if (!wasCapturing && *capturing)
            menu.held = sources;
        if (result) // menu closed
            menu = {};
        return result;
    }

    // Wraps the controls menu's key checks (Up, Down, Esc, Return, Space): adds the synthesized controller presses
    uint8_t __cdecl KeyPressed(uint32_t key)
    {
        uint8_t pressed = keyPressed(key);
        if (menu.closing)
            return 0;
        uint16_t action = key == 0xC8 ? Up : key == 0xD0 ? Down : key == 0x01 ? Cancel : key == 0x1C ? Menu : key == 0x39 ? Jump : 0;
        if (menu.pressed & action)
            pressed = 0x80;
        if (pressed && (key == 0x01 || key == 0x1C))
            menu.closing = true; // Esc or Return ends the menu; ignore the later checks this frame
        return pressed;
    }

    // Replaces the controls menu's key capture: waits for the input that opened it to be released, and D-pad directions can be bound
    int __cdecl Capture()
    {
        menu.held &= Sources();
        if (menu.held)
            return -1;
        int key = capture();
        if (valid && (key == -1 || key == 0x800)) // nothing, or an analog stick direction
            for (int i = 0; i < 4; i++)
                if (Dpad(joystick->rgdwPOV[0]) & (1 << i)) return DpadUp + i;
        return key;
    }

    const char* __cdecl BindingName(int key)
    {
        static const char* names[] = {"dpad up", "dpad right", "dpad down", "dpad left"};
        return key >= DpadUp && key <= DpadLeft ? names[key - DpadUp] : bindingName(key);
    }

    uint16_t* pauseSelection; // highlighted item of the pause menu
    void (__cdecl* updateGameInput)(); // derives the frame's gameplay input from the actions; presses are edges against the previous frame

    // Cancel on the pause menu's main page selects continue, without firing the actions that share its button when play resumes
    struct PauseCancel
    {
        void operator()(injector::reg_pack& regs)
        {
            if (regs.ecx) { // cancel was pressed
                regs.edx &= ~0xFFFFu; // dx: the item to act on
                *pauseSelection = 0;
                updateGameInput();
            }
        }
    };
}
