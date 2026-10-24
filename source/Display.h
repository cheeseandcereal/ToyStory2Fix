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
}
