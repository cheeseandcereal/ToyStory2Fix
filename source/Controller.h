#pragma once
#define DIRECTINPUT_VERSION 0x0500
#include <dinput.h>

namespace Controller
{
    constexpr uint16_t Menu = 0x08, Up = 0x10, Right = 0x20, Down = 0x40, Left = 0x80, CameraLeft = 0x100, CameraRight = 0x200, Cancel = 0x1000, Jump = 0x4000;
    constexpr int DpadUp = 0x422, DpadRight = 0x423, DpadDown = 0x424, DpadLeft = 0x425, LeftTrigger = 0x426, RightTrigger = 0x427; // pseudo-keys after the 32 joystick buttons

    struct Binding { int key; uint32_t actions; };

    Binding* bindings; // 38 entries, key -1 = empty
    uint64_t buttons; // buttons in bits 0-31, D-pad and triggers in bits 32-37
    bool valid; // buttons holds a fresh joystick sample
    uint16_t* actions; // actions pressed this frame
    int32_t* movement; // X, Y walking input; the game writes it only while the left stick is deflected
    uint32_t* directions; // Up/Down/Left/Right from the left stick
    uint16_t camera, triggers; // right stick and trigger states, kept between frames for hysteresis
    int (__cdecl* capture)();
    const char* (__cdecl* bindingName)(int key);
    int (__cdecl* configure)();
    uint8_t (__cdecl* keyPressed)(uint32_t key);

    struct
    {
        bool active;
        uint64_t held; // inputs still held from the press that opened the capture step
        uint16_t previous, pressed; // menu actions held last frame and pressed this frame
        DWORD repeatAt;
    } menu;

    // Replaces the joystick GetDeviceState call: adds right stick camera control, D-pad and trigger bindings
    HRESULT WINAPI ReadJoystick(IDirectInputDevice2A* device, DWORD size, DIJOYSTATE* state)
    {
        HRESULT result = device->GetDeviceState(size, state);
        valid = SUCCEEDED(result);
        if (!valid)
            return result;
        // The game uses one joystick interface for the process lifetime.
        DIDEVICEOBJECTINSTANCEA object = {sizeof(object)};
        static const bool hasZ = SUCCEEDED(device->GetObjectInfo(&object, DIJOFS_Z, DIPH_BYOFFSET));
        int rx = state->lRx - 0x8000, engage = 0x8000 * 35 / 100, release = 0x8000 * 25 / 100; // 0-65535 axis range, as the game assumes for X/Y
        if (rx <= -engage) camera = CameraLeft;
        else if (rx >= engage) camera = CameraRight;
        else if ((camera == CameraLeft && rx > -release) || (camera == CameraRight && rx < release)) camera = 0;
        // ponytail: Xbox-style combined Z; independent simultaneous triggers require XInput.
        int z = state->lZ - 0x8000;
        if (!hasZ) triggers = 0;
        else if (z >= engage) triggers = 16;
        else if (z <= -engage) triggers = 32;
        else if ((triggers == 16 && z < release) || (triggers == 32 && z > -release)) triggers = 0;
        DWORD pov = state->rgdwPOV[0];
        constexpr unsigned masks[] = {1, 3, 2, 6, 4, 12, 8, 9};
        unsigned extra = (pov < 36000 ? masks[((pov + 2250) / 4500) & 7] : 0) | triggers;
        buttons = (uint64_t)extra << 32;
        for (int i = 0; i < 32; i++)
            if (state->rgbButtons[i]) buttons |= 1ull << i;
        uint16_t bound = camera;
        for (int i = 0; i < 38; i++)
            if (bindings[i].key >= DpadUp && bindings[i].key <= RightTrigger && (extra & (1 << (bindings[i].key - DpadUp))))
                bound |= bindings[i].actions;
        *actions |= bound;
        if (bound & (Up | Right | Down | Left)) { // walk at full speed like the arrow keys; a deflected stick still overrides this
            movement[0] = ((bound & Right) ? 0x8000 : 0) - ((bound & Left) ? 0x8000 : 0);
            movement[1] = ((bound & Up) ? 0x8000 : 0) - ((bound & Down) ? 0x8000 : 0);
        }
        return result;
    }

    // Wraps the controls menu update: A changes, Start accepts, Cancel cancels, stick or D-pad up/down navigate
    int __cdecl Configure()
    {
        uint64_t sources = valid ? buttons : 0;
        DWORD now = GetTickCount();
        uint16_t navigation = valid ? (*directions & (Up | Down)) | ((sources >> 32 & 1) ? Up : 0) | ((sources >> 32 & 4) ? Down : 0) : 0;
        uint16_t down = navigation | (*actions & Cancel) |
            ((sources & 1) ? Jump : 0) | ((sources & 0x80) ? Menu : 0);
        menu.pressed = menu.active ? down & ~menu.previous : 0;
        if (!menu.active || navigation != (menu.previous & (Up | Down))) {
            menu.repeatAt = now + 350;
        } else if (navigation && (int)(now - menu.repeatAt) >= 0) {
            menu.pressed |= navigation;
            menu.repeatAt = now + 120;
        }
        menu.active = true;
        menu.previous = down;
        return configure();
    }

    // Wraps the controls menu's key checks (Up, Down, Esc, Return, Space): adds the synthesized controller presses
    uint8_t __cdecl KeyPressed(uint32_t key)
    {
        uint8_t pressed = keyPressed(key);
        if (!menu.active)
            return 0;
        uint16_t action = key == 0xC8 ? Up : key == 0xD0 ? Down : key == 0x01 ? Cancel : key == 0x1C ? Menu : key == 0x39 ? Jump : 0;
        if (menu.pressed & action)
            pressed = 0x80;
        if (pressed && key == 0x39)
            menu.held = valid ? buttons : 0;
        if (pressed && (key == 0x01 || key == 0x1C))
            menu.active = false; // Esc or Return ends the menu; ignore the later checks this frame
        return pressed;
    }

    // Replaces the controls menu's key capture: waits for held inputs to release, and allows D-pad and trigger bindings
    int __cdecl Capture()
    {
        int key = capture();
        if (valid)
            menu.held &= buttons;
        if (menu.held)
            return key == 0x01 ? key : -1;
        if (valid && (key == -1 || key == 0x800)) // nothing, or an analog stick direction
            for (int i = 0; i < 6; i++)
                if (buttons & (1ull << (32 + i))) return DpadUp + i;
        return key;
    }

    const char* __cdecl BindingName(int key)
    {
        static const char* names[] = {"dpad up", "dpad right", "dpad down", "dpad left", "l trigger", "r trigger"};
        return key >= DpadUp && key <= RightTrigger ? names[key - DpadUp] : bindingName(key);
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
