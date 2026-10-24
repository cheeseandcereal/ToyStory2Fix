#!/usr/bin/env python3
"""Cross-build data/scripts/ToyStory2Fix.asi with clang-cl and lld-link, then package build/ToyStory2Fix.zip.

Usage: build-linux.py SDK
SDK is an unpacked MSVC/Windows SDK laid out as crt/{include,lib/x86} and sdk/{include/{ucrt,shared,um},lib/{ucrt,um}/x86}.
"""

import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCES = ["source/dllmain.cpp", "includes/stdafx.cpp", "external/hooking/Hooking.Patterns.cpp"]
INCLUDES = ["includes", "external/hooking", "external/injector/include", "external/inireader"]
LIBRARIES = ["kernel32", "user32", "winmm"]
PACKAGE = ["dsound.dll", "dsound.ini", "scripts/ToyStory2Fix.asi", "scripts/ToyStory2Fix.ini"]


def run(command):
    print(" ".join(map(str, command)), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sdk = Path(sys.argv[1]).resolve()
    build = ROOT / "build/linux"
    build.mkdir(parents=True, exist_ok=True)

    compiler = ["clang-cl", "--target=i686-pc-windows-msvc", "/nologo", "/X", "/std:c++17", "/EHsc", "/MT", "/O2",
                "/DWIN32", "/D_WINDOWS", "/D_MBCS", "/DNDEBUG", "-Wno-c++11-narrowing"]
    compiler += [f"/I{ROOT / path}" for path in INCLUDES]
    for path in ["crt/include", "sdk/include/ucrt", "sdk/include/shared", "sdk/include/um"]:
        compiler += ["/imsvc", str(sdk / path)]
    objects = []
    for source in SOURCES:
        obj = build / (Path(source).stem + ".obj")
        run(compiler + ["/c", ROOT / source, f"/Fo{obj}"])
        objects.append(obj)

    output = ROOT / "data/scripts/ToyStory2Fix.asi"
    run(["lld-link", "/nologo", "/machine:x86", "/dll", "/noimplib", f"/out:{output}"]
        + [f"/libpath:{sdk / path}" for path in ["crt/lib/x86", "sdk/lib/ucrt/x86", "sdk/lib/um/x86"]]
        + objects + [f"{name}.lib" for name in LIBRARIES])

    archive = ROOT / "build/ToyStory2Fix.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
        for name in PACKAGE:
            package.write(ROOT / "data" / name, name)
    print(f"Built {output}\nPackaged {archive}")


if __name__ == "__main__":
    main()
