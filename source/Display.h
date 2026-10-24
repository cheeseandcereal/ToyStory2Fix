#pragma once
#include <ddraw.h>

namespace Display
{
    // Enumeration records built by the game, as consumed by its graphics picker
    struct Device
    {
        uint8_t reserved[0xD8];
        uint32_t renderBitDepths; // DDBD_*
        uint8_t reserved2[0x5C];
        int32_t hardware;
        int32_t supportsCurrentMode;
        int32_t windowed;
        void* selectedMode;
        void* modes;
        Device* next;
    };

    struct Driver
    {
        GUID guid;
        GUID* guidPointer; // null for the primary DirectDraw driver
        uint8_t reserved[0x58];
        uint32_t caps2;
        uint8_t reserved2[0x308];
        Device* selectedDevice;
        Device* devices;
        Driver* next;
    };

    Driver** drivers;
    Driver** selectedDriver;
    bool fullscreen = true;
    int width, height; // 0 = use the monitor's size
    int (__cdecl* initDisplay)(HWND, void*, void*, void*, uint32_t);
    uint8_t** context; // the game's display context
    WNDPROC wndProc;

    // Replaces the picker dialog: primary driver, first hardware device able to render the desktop mode in 32-bit windowed mode
    void __cdecl SelectDevice()
    {
        for (Driver* driver = *drivers; driver; driver = driver->next) {
            if (driver->guidPointer || !(driver->caps2 & DDCAPS2_CANRENDERWINDOWED))
                continue;
            for (Device* device = driver->devices; device; device = device->next) {
                if (device->hardware && device->supportsCurrentMode && (device->renderBitDepths & DDBD_32)) {
                    device->windowed = 1;
                    driver->selectedDevice = device;
                    *selectedDriver = driver;
                    return;
                }
            }
        }
    }

    // Windowed: keeps the client area at the render aspect ratio while the frame is dragged, and moves the game image along with the window
    LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_SIZING) {
            RECT& drag = *(RECT*)lParam; // proposed window rectangle, frame included
            RECT frame = {};
            AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, FALSE);
            int frameWidth = frame.right - frame.left, frameHeight = frame.bottom - frame.top;
            int dragWidth = drag.right - drag.left, dragHeight = drag.bottom - drag.top;
            if (wParam == WMSZ_TOP || wParam == WMSZ_BOTTOM) dragWidth = (dragHeight - frameHeight) * width / height + frameWidth;
            else dragHeight = (dragWidth - frameWidth) * height / width + frameHeight;
            if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT) drag.left = drag.right - dragWidth;
            else drag.right = drag.left + dragWidth;
            if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT) drag.top = drag.bottom - dragHeight;
            else drag.bottom = drag.top + dragHeight;
            return TRUE;
        }
        if (message == WM_WINDOWPOSCHANGED && *context) {
            RECT& dest = *(RECT*)(*context + 0x10);
            GetClientRect(hwnd, &dest);
            MapWindowPoints(hwnd, NULL, (POINT*)&dest, 2);
        }
        return CallWindowProc(wndProc, hwnd, message, wParam, lParam);
    }

    // Replaces the display constructor call: place the window on its monitor, then build the game's windowed DirectDraw chain
    int __cdecl InitDisplay(HWND hwnd, void* driver, void* device, void* mode, uint32_t flags)
    {
        MONITORINFO monitor = { sizeof(monitor) };
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
        RECT bounds = monitor.rcMonitor;
        if (width <= 0) width = bounds.right - bounds.left;
        if (height <= 0) height = bounds.bottom - bounds.top;
        if (!fullscreen) {
            wndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
            SetClassLongPtr(hwnd, GCLP_HCURSOR, (LONG_PTR)LoadCursor(NULL, IDC_ARROW));
            SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            bounds = { 0, 0, width, height };
            AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
            OffsetRect(&bounds, monitor.rcWork.left - bounds.left, monitor.rcWork.top - bounds.top);
        }
        SetWindowPos(hwnd, HWND_NOTOPMOST, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, SWP_FRAMECHANGED);
        return initDisplay(hwnd, driver, device, mode, flags);
    }

    // Replaces GetClientRect(hwnd, &context->source): the back buffer and Blt source use the configured size
    void __cdecl SetSourceRect(uint8_t* context)
    {
        *(RECT*)(context + 0x20) = { 0, 0, width, height };
    }

    // Replaces the primary surface Lock in the game's pixel format query, which only reads the description
    // This fixes flickering during certain in-game screen transitions when using wine + d7vk
    int __cdecl DescribePrimary(DDSURFACEDESC2* description)
    {
        return (*(IDirectDrawSurface4**)(*context + 0x30))->GetSurfaceDesc(description);
    }
}
