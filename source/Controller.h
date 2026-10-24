#pragma once
#define DIRECTINPUT_VERSION 0x0500
#include <dinput.h>

namespace Controller
{
    constexpr uint16_t Up = 0x10, Right = 0x20, Down = 0x40, Left = 0x80, CameraLeft = 0x100, CameraRight = 0x200;
    constexpr int DpadUp = 0x422, DpadRight = 0x423, DpadDown = 0x424, DpadLeft = 0x425; // pseudo-keys after the 32 joystick buttons

    struct Binding { int key; uint32_t actions; };

    Binding* bindings; // 38 entries, key -1 = empty
    DIJOYSTATE* joystick;
    uint16_t* actions; // actions pressed this frame
    int32_t* movement; // X, Y walking input; the game writes it only while the left stick is deflected
    uint16_t camera; // right stick state, kept between frames for hysteresis
    int (__cdecl* capture)();
    const char* (__cdecl* bindingName)(int key);

    unsigned Dpad(DWORD pov) // 1 = up, 2 = right, 4 = down, 8 = left
    {
        constexpr unsigned masks[] = {1, 3, 2, 6, 4, 12, 8, 9};
        return pov < 36000 ? masks[((pov + 2250) / 4500) & 7] : 0;
    }

    // Replaces the joystick GetDeviceState call: adds right stick camera control and D-pad bindings
    HRESULT WINAPI ReadJoystick(IDirectInputDevice2A* device, DWORD size, DIJOYSTATE* state)
    {
        HRESULT result = device->GetDeviceState(size, state);
        if (FAILED(result))
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

    // Replaces the controls menu's key capture: D-pad directions can be bound
    int __cdecl Capture()
    {
        int key = capture();
        if (key == -1 || key == 0x800) // nothing, or an analog stick direction
            for (int i = 0; i < 4; i++)
                if (Dpad(joystick->rgdwPOV[0]) & (1 << i)) return DpadUp + i;
        return key;
    }

    const char* __cdecl BindingName(int key)
    {
        static const char* names[] = {"dpad up", "dpad right", "dpad down", "dpad left"};
        return key >= DpadUp && key <= DpadLeft ? names[key - DpadUp] : bindingName(key);
    }
}
