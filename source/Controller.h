#pragma once
#define DIRECTINPUT_VERSION 0x0500
#include <dinput.h>

namespace Controller
{
    constexpr uint16_t CameraLeft = 0x100, CameraRight = 0x200;

    uint16_t* actions; // actions pressed this frame
    uint16_t camera; // right stick state, kept between frames for hysteresis

    // Replaces the joystick GetDeviceState call: adds right stick camera control
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
        return result;
    }
}
