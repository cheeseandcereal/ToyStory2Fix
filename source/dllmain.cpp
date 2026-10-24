#include "stdafx.h"
#include "Display.h"
#include <mmsystem.h>
#include <cfloat>

uintptr_t sub_490860_addr;
uintptr_t sub_49D910_addr;
TIMECAPS tc;
LARGE_INTEGER Frequency;
LARGE_INTEGER PreviousTime, CurrentTime, ElapsedMicroseconds;
int sleepTime;
int framerateFactor;

struct Variables
{
    uint32_t nWidth;
    uint32_t nHeight;
    float fAspectRatio;
    float fScaleValue;
    uint32_t* speedMultiplier;
    bool* isDemoMode;
} Variables;

namespace Sky
{
    float* imageWidth; // the level's sky image width; a tile spans textureWidth / imageWidth of the screen width
    float narrowing = 1.0f; // (4/3) / (width / height)
    float tileDivisor, scroll; // read by the game: imageWidth adjusted for the tile width, screen widths scrolled per full turn
    const float pitchDivisor = 0.75f * 3.0f / (4.0f * (float)M_PI); // replaces aspect * 3/(4 pi): the vertical view is unchanged
    void (__cdecl* update)(int pitch, int yaw);

    void __cdecl Update(int pitch, int yaw)
    {
        tileDivisor = *imageWidth / narrowing;
        scroll = 4.0f * (float)M_PI / 3.0f * narrowing;
        update(pitch, yaw);
    }
}

void UpdateElapsedMicroseconds() {
    QueryPerformanceCounter(&CurrentTime);
    ElapsedMicroseconds.QuadPart = CurrentTime.QuadPart - PreviousTime.QuadPart;
    ElapsedMicroseconds.QuadPart *= 1000000;
    ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
}

int __cdecl sub_490860(int a1) {
    timeBeginPeriod(tc.wPeriodMin);

    if (PreviousTime.QuadPart == 0)
        QueryPerformanceCounter(&PreviousTime); // initialise

    UpdateElapsedMicroseconds();

    framerateFactor = ((int)ElapsedMicroseconds.QuadPart / 16667) + 1;
    // Demo mode needs 30fps maximum
    if (*Variables.isDemoMode && framerateFactor < 2)
        framerateFactor = 2;

    *Variables.speedMultiplier = std::clamp(framerateFactor, 1, 3);

    sleepTime = 0;
    // Loop until next frame due
    do {
        sleepTime = (16949 * framerateFactor - (uint32_t)ElapsedMicroseconds.QuadPart) / 1000; // calculate sleep time, 16949 µs = 59 fps (to limit frame drops)
        sleepTime = ((sleepTime / tc.wPeriodMin) * tc.wPeriodMin) - tc.wPeriodMin; // truncate to multiple of period
        if (sleepTime > 0)
            Sleep(sleepTime); // sleep to avoid wasted CPU
        UpdateElapsedMicroseconds();
    } while (ElapsedMicroseconds.QuadPart < 16667 * framerateFactor);

    QueryPerformanceCounter(&PreviousTime);
    timeEndPeriod(tc.wPeriodMin);
    return (int)(PreviousTime.QuadPart / 1000);
}

int sub_49D910() {
    auto _sub_49D910 = (int(*)()) sub_49D910_addr;

    /* FIX WIDESCREEN */
    // this code can't be in Init() because width/height are not set at first

    /* Set width and height */
    auto pattern = hook::pattern("8B 15 ? ? ? ? 89 4C 24 08 89 44 24 0C"); //4B5672
    uint32_t* resolution = *(uint32_t**)pattern.get_first(2); // width, height
    Variables.nWidth = resolution[0];
    Variables.nHeight = resolution[1];
    Variables.fAspectRatio = float(Variables.nWidth) / float(Variables.nHeight);
    Variables.fScaleValue = 1.0f / Variables.fAspectRatio;
    Sky::narrowing = (4.0f / 3.0f) / Variables.fAspectRatio;

    /* Fix 3D stretch */
    pattern = hook::pattern("C7 40 44 00 00 40 3F"); //4CE80F
    injector::WriteMemory<float>(pattern.get_first(3), Variables.fScaleValue, true); // mov [eax+44h], 0.75 -> height/width

    return _sub_49D910();
}



DWORD WINAPI Init(LPVOID bDelay)
{
    /* INITIALISE */
    auto pattern = hook::pattern("03 D1 2B D7 85 D2 7E 09 52 E8 ? ? ? ?"); //4909B5

    if (pattern.count_hint(1).empty() && !bDelay)
    {
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&Init, (LPVOID)true, 0, NULL);
        return 0;
    }

    if (bDelay)
        while (pattern.clear().count_hint(1).empty()) { Sleep(0); };

    CIniReader iniReader("ToyStory2Fix.ini");
    constexpr char* INI_KEY = "ToyStory2Fix";

    if (iniReader.ReadBoolean(INI_KEY, "FixFramerate", true)) {
        timeGetDevCaps(&tc, sizeof(tc));
        QueryPerformanceFrequency(&Frequency);

        pattern = hook::pattern("8B 0D ? ? ? ? 2B F1 3B"); //4011DF
        Variables.speedMultiplier = *(uint32_t**)pattern.get_first<uint32_t**>(2);
        pattern = hook::pattern("39 3D ? ? ? ? 75 27"); //403C3A
        Variables.isDemoMode = *(bool**)pattern.get_first<bool*>(2);

        pattern = hook::pattern("C7 05 ? ? ? ? 00 00 00 00 E8 ? ? ? ? E8 ? ? ? ? 33"); //49BBD8
        sub_490860_addr = ((uintptr_t)pattern.get_first(15) + *pattern.get_first<uintptr_t>(11));
        injector::MakeCALL(pattern.get_first(10), sub_490860);
        pattern = hook::pattern("83 C4 08 6A 01 E8 ? ? ? ?"); //441906
        injector::MakeCALL(pattern.get_first(5), sub_490860);
        pattern = hook::pattern("6A 00 E8 ? ? ? ? 6A 01 E8 ? ? ? ? 83"); //4419F4
        injector::MakeCALL(pattern.get_first(2), sub_490860);
    }


    /* Allow 32-bit modes regardless of registry settings - thanks hdc0 */
    // Always on: automatic device selection needs the desktop's 32-bit modes enumerated
    pattern = hook::pattern("74 0B 5E 5D B8 01 00 00 00"); //4ACA44
    injector::WriteMemory<uint8_t>(pattern.get_first(0), '\xEB', true);

    /* Skip the graphics picker and select a device automatically */
    pattern = hook::pattern("A1 ? ? ? ? B9 ? ? ? ? 85 C0 74 10 8D 88 80 03 00 00"); //4AC6FD
    Display::drivers = *pattern.get_first<Display::Driver**>(1);
    pattern = hook::pattern("8B 0D ? ? ? ? 85 C9 75 06 B8 01 00 00 81"); //4ACFC0
    Display::selectedDriver = *pattern.get_first<Display::Driver**>(2);
    pattern = hook::pattern("E8 ? ? ? ? 8D 44 24 00 8D 4C 24 04 50 51 E8"); //412B9B
    injector::MakeCALL(pattern.get_first(0), Display::SelectDevice);

    /* Render at a fixed size into a borderless fullscreen or resizable window */
    Display::fullscreen = iniReader.ReadBoolean(INI_KEY, "Fullscreen", true);
    Display::width = iniReader.ReadInteger(INI_KEY, "ResolutionWidth", 0); // "auto" reads as 0
    Display::height = iniReader.ReadInteger(INI_KEY, "ResolutionHeight", 0);
    SetProcessDPIAware();
    pattern = hook::pattern("6A 51 52 50 8B 08 FF 51 50"); //408DE2
    injector::WriteMemory<uint8_t>(pattern.get_first(1), DDSCL_NORMAL, true); // no exclusive mode while enumerating
    pattern = hook::pattern("8B 0D ? ? ? ? 6A 03 51 FF 15"); //412CA4
    injector::WriteMemory<uint8_t>(pattern.get_first(7), SW_SHOW, true); // SW_MAXIMIZE would override the window placement
    pattern = hook::pattern("8D 46 20 50 8B 06 50 FF D5"); //4AF25E
    char sourceRect[] = {0x56, 0xE8, 0, 0, 0, 0, 0x83, 0xC4, 0x04}; // push esi (context); call SetSourceRect; add esp, 4
    injector::WriteMemoryRaw(pattern.get_first(0), sourceRect, sizeof(sourceRect), true);
    injector::MakeCALL(pattern.get_first(1), Display::SetSourceRect);
    pattern = hook::pattern("8B 44 24 14 50 51 E8 ? ? ? ? 83 C4 14 85 C0"); //412CC6
    Display::initDisplay = injector::MakeCALL(pattern.get_first(6), Display::InitDisplay).get();
    pattern = hook::pattern("8B 0D ? ? ? ? E9 ? ? ? ? 90 90 90 90 90 8B 0D ? ? ? ? E9"); //4ABD30
    Display::context = *pattern.get_first<uint8_t**>(2);
    if (!Display::fullscreen) {
        pattern = hook::pattern("8B 35 ? ? ? ? 6A 00 FF D6 85 C0 7C 08 6A 00 FF D6 85 C0 7D F8"); //412E49
        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0x14EB, true); // jmp over the ShowCursor(FALSE) loop: windowed mode keeps the cursor
    }

    /* Fix "Unable to enumerate a suitable device - thanks hdc0 */
    if (iniReader.ReadBoolean(INI_KEY, "IgnoreVRAM", true)) {
        pattern = hook::pattern("74 44 8B 8A 50 01 00 00 8B 91 64 03 00 00"); //4ACAC2
        injector::WriteMemory<uint8_t>(pattern.get_first(0), '\xEB', true);
    }

    /* Allow copyright/ESRB screen to be skipped immediately */
    if (iniReader.ReadBoolean(INI_KEY, "SkipSplash", true)) {
        pattern = hook::pattern("66 8B 3D ? ? ? ? 83 C4 1C"); //438586
        struct CopyrightHook
        {
            void operator()(injector::reg_pack& regs)
            {
                _asm mov di, 1
            }

        }; injector::MakeInline<CopyrightHook>(pattern.get_first(0), pattern.get_first(7));
    }

    /* Increase Render Distance to Max */
    if (iniReader.ReadBoolean(INI_KEY, "IncreaseRenderDistance", true)) {
        pattern = hook::pattern("D9 44 24 04 D8 4C 24 04 D9 1D"); //4BC410
        float** flt_5088B0_addr = (float**)pattern.get_first(10);
        **flt_5088B0_addr = sqrtf(FLT_MAX); // large but finite, so later arithmetic cannot produce NaN
        injector::WriteMemory<uint16_t>(pattern.get_first(8), 0xD8DD, true); // fstp st(0): pop instead of storing, keeps the x87 stack balanced
        injector::MakeNOP(pattern.get_first(10), 4);
    }

    /* Fix widescreen once game loop begins */
    if (iniReader.ReadBoolean(INI_KEY, "Widescreen", true)) {
        pattern = hook::pattern("8D 44 24 10 50 57 E8 ? ? ? ? 83"); //4317EC
        sub_49D910_addr = ((uintptr_t)pattern.get_first(11) + *pattern.get_first<uintptr_t>(7));
        injector::MakeCALL(pattern.get_first(6), sub_49D910);

        // Sky backdrop: tile width, scroll per turn and pitch divisor as on a 4:3 screen of the same height
        pattern = hook::pattern("D8 35 ? ? ? ? 8B 10 83 C4 18"); //48F295
        Sky::imageWidth = *pattern.get_first<float*>(2);
        injector::WriteMemory(pattern.get_first(2), &Sky::tileDivisor, true); // fdiv [imageWidth]
        pattern = hook::pattern("D8 0D ? ? ? ? D9 1D ? ? ? ? D9 05 ? ? ? ? D8 05"); //48F323
        injector::WriteMemory(pattern.get_first(2), &Sky::scroll, true); // fmul [4 pi / 3]
        pattern = hook::pattern("D9 42 44 D8 0D ? ? ? ? D8 3D ? ? ? ? DB 05"); //48F3B4
        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0x05D9, true); // fld [camera aspect]; fmul [3 / (4 pi)] -> fld [pitchDivisor]
        injector::WriteMemory(pattern.get_first(2), &Sky::pitchDivisor, true);
        injector::MakeNOP(pattern.get_first(6), 3);
        pattern = hook::pattern("50 51 E8 ? ? ? ? 6A 00 E8 ? ? ? ? 6A 01 E8"); //441250
        Sky::update = injector::MakeCALL(pattern.get_first(2), Sky::Update).get();
    }

    /* Fix texture-mapping bugs */
    // The texture and Zurg routines run from the ASI's own memory: the game's .text padding is not free in every release (the Polish one keeps translation code there)
    if (iniReader.ReadBoolean(INI_KEY, "TextureFix", true)) {
        static char textureFix[] = {0x8B, 0xD1,
                             0xC1, 0xE9, 0x02,
                             0x56,
                             0x51,
                             0xD9, 0x46, 0x14,
                             0xC7, 0x46, 0x14, 0x00, 0x00, 0x80, 0xB7,
                             0xD8, 0x46, 0x14,
                             0xD9, 0x5E, 0x14,
                             0xD9, 0x46, 0x1C,
                             0xC7, 0x46, 0x1C, 0x00, 0x00, 0x80, 0x37,
                             0xD8, 0x46, 0x1C,
                             0xD9, 0x5E, 0x1C,
                             0xD9, 0x46, 0x20,
                             0xC7, 0x46, 0x20, 0x00, 0x00, 0x80, 0x37,
                             0xD8, 0x46, 0x20,
                             0xD9, 0x5E, 0x20,
                             0x83, 0xC6, 0x24,
                             0x83, 0xE9, 0x09,
                             0x75, 0xC8,
                             0x59,
                             0x5E,
                             0xC3};
        injector::ProtectMemory(textureFix, sizeof(textureFix), PAGE_EXECUTE_READWRITE);

        auto textureJump = hook::pattern("8B D1 C1 E9 02 F3 A5 8B CA EB AB"); //4B300E
        injector::MakeCALL(textureJump.get_first(0), textureFix);
    }

    /* Fix broken disk launcher at 60 FPS */
    if (iniReader.ReadBoolean(INI_KEY, "DiskFix", true)) {
        pattern = hook::pattern("A1 ?? ?? ?? ?? 0F AF C1 99 F7 7C 24 18"); //411099
        char diskFix[] = {0xB8, 0x02, 0x00, 0x00, 0x00};
        injector::WriteMemoryRaw(pattern.get_first(0), diskFix, sizeof(diskFix), true);
    }

    /* Fix fast Zurg and other flying enemies at 60 FPS */
    if (iniReader.ReadBoolean(INI_KEY, "ZurgFix", true)) {
        auto zurgXJump = hook::pattern("C1 FB 04 2B CB 8B 5C 24 24 89 0E"); //407F8E
        static char zurgXFix[] = {0x0F, 0xAF, 0x1D, *zurgXJump.get_first<char>(0x88), *zurgXJump.get_first<char>(0x89), *zurgXJump.get_first<char>(0x8A), *zurgXJump.get_first<char>(0x8B),
                           0xC1, 0xFB, 0x05,
                           0x29, 0xD9,
                           0xC3,
                           0x90};

        auto zurgZJump = hook::pattern("C1 FA 04 2B CA 89 4E 08 0F BF 46 0E"); //407FB0
        static char zurgZFix[] = {0x0F, 0xAF, 0x15, *zurgXJump.get_first<char>(0x88), *zurgXJump.get_first<char>(0x89), *zurgXJump.get_first<char>(0x8A), *zurgXJump.get_first<char>(0x8B),
                           0xC1, 0xFA, 0x05,
                           0x29, 0xD1,
                           0xC3,
                           0x90};

        injector::ProtectMemory(zurgXFix, sizeof(zurgXFix), PAGE_EXECUTE_READWRITE);
        injector::ProtectMemory(zurgZFix, sizeof(zurgZFix), PAGE_EXECUTE_READWRITE);
        injector::MakeCALL(zurgXJump.get_first(0), zurgXFix);
        injector::MakeCALL(zurgZJump.get_first(0), zurgZFix);
    }

    return 0;
}


BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        Init(NULL);
    }
    return TRUE;
}
