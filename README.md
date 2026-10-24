# Toy Story 2 Fix
Toy Story 2 Fix is a program that fixes and enhances Toy Story 2 for the PC. Features include:
* Fixes the "Unable to Enumerate Device" error.
* Fixes the framerate issues that can occur on modern PCs.
* Fixes texture-mapping bugs and the disk launcher and Zurg behaviour at 60 FPS.
* Allows the player to immediately skip the ESRB and Copyright screens.
* Allows the game to be played in widescreen with no 3D stretching.
* Increases the render distance of levels.
* Works on every regional release of the game.
* Skips the graphics picker and starts in borderless fullscreen or a resizable window at a chosen resolution.
* Makes first-person Up look up and Down look down.
* Better controller support: right stick camera, bindable D-pad, controller use in the controls menu and shared bindings.

# Download
Get the latest version [from the releases page](https://github.com/cheeseandcereal/ToyStory2Fix/releases/latest). Extract the ZIP file into the folder you installed Toy Story 2 into.

# Configuration
You can enable or disable any part of the patch by opening the `scripts\ToyStory2Fix.ini` file and setting the options to `true` or `false`.

## Resolution/Display Config
The pre-game display setup/resolution picker is skipped, and replaced with controls in this program. Defaults:

```ini
Fullscreen = true
ResolutionWidth = auto
ResolutionHeight = auto
```

`auto` uses the resolution of the monitor the game starts on.
It's recommended to set a value (such as 1600 width, 900 height) for the resolution options to increase performance (particularly with high resolution monitors), or when running in windowed mode.
The rendered image is stretched, so the game will fill the screen or window regardless of the resolution chosen. Because of this, in fullscreen, choose dimensions that match your monitor's aspect ratio (to avoid uneven stretching).

Command-line options are supported and override the ini i.e.:

```text
toy2.exe --windowed -w 640 -h 480
toy2.exe --fullscreen -w 1600 -h 900
```

# Building
## Windows
Run `premake5.bat`, then build `build\ToyStory2Fix.sln` (Release, Win32) with Visual Studio.

## Linux
Requires `clang-cl`, `lld-link` and an unpacked MSVC/Windows SDK (for example from [xwin](https://github.com/Jake-Shadle/xwin)) laid out as `crt/{include,lib/x86}` and `sdk/{include/{ucrt,shared,um},lib/{ucrt,um}/x86}`:

```bash
python3 build-linux.py /path/to/sdk
```

This writes `data/scripts/ToyStory2Fix.asi` and packages `build/ToyStory2Fix.zip`.
