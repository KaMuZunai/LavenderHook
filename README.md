# LavenderHook

LavenderHook is a universal graphics API overlay hook for Windows. It loads
into a running DirectX/OpenGL process and draws a Dear ImGui based overlay on
top of it, exposing a set of debug and quality-of-life utilities.

It provides hooks for the most common Windows renderers:

- **DirectX 9** (IDirect3DDevice9::EndScene)
- **DirectX 11** (IDXGISwapChain::Present)
- **DirectX 12** (IDXGISwapChain::Present)
- **OpenGL** (wglSwapBuffers)

Hooking is done through [MinHook](https://github.com/TsudaKageyu/minhook).

---------------------------------------------------------------------------------

# Features

## Menu Access

Press **Insert** or **CTRL + F1** to toggle the overlay menu.

## Debug Window

- **Game Speed**\
  Overrides the process speed with a configurable multiplier (0.10x – 10.00x)
  by hooking the QueryPerformanceCounter / QueryPerformanceFrequency timers.

- **Fullscreen Borderless**\
  Strips window borders and maximizes the window to the current monitor
  (one-shot, self-disabling toggle).

- **Texture Capture**\
  Captures and dumps game textures to the `DumpedTextures` folder.

- **Wireframe Mode**\
  Forces rendering in wireframe mode (DirectX 9 / DirectX 11).

- **No Fog**\
  Disables fog rendering (DirectX 9).

- **Freeze Frame**\
  Prevents the game from presenting new frames (DirectX 9 / DirectX 11).

Every toggle supports up to two configurable combo hotkeys (ESC binds to None)
and can be saved and restored through profiles.

## Info Overlay

Displays small indicators (e.g. ping, server) in the top-right corner of the
process.

## Performance Overlay

Displays live resource statistics — FPS, RAM, CPU, and GPU usage — each of
which can be toggled individually.

## Console

A built-in log console that captures hook status, errors, and runtime messages.

## Profiles

Create profiles that snapshot the currently active functions/toggles and
restore them with a single press. Profiles support rename, delete, and an
assignable hotkey.

## Settings

- **Theme Colors**\
  RGB values for the primary, alternate, and highlight colors used throughout
  the overlay.

- **Menu Size**\
  A slider that scales the entire UI between 50% and 200% with smooth
  animation.

- **Audio**\
  A master volume slider for overlay sounds, with options to mute button
  clicks and the stop-on-fail sound.

- **Windows**\
  A collapsible list to show or hide each overlay window (Debug, Info Overlay,
  Performance Overlay, Console, Profiles, Menu Logo).

## Menu Logo

A randomized animated logo (several chibi witch girl images) shown in the
bottom-left corner of the process.

## Misc

- **Network Monitor**\
  Hooks winsock / iphlpapi calls to monitor network activity.

- **Focus Shim**\
  Simulates an unfocused window state while the overlay menu is open.

- **Logging**\
  A file log is written alongside the hook to help diagnose issues.

- **Crash Handler**\
  Generates a minidump if the hooked process crashes.

- **Per-Process Configuration**\
  Settings are stored per executable name (e.g. `game.exe.ini`), so the hook
  keeps independent settings for every target process.

---------------------------------------------------------------------------------

# Installation

Build `LavenderHook.dll` and load it into the target process with your
preferred injection method (manual map, proxy DLL, or an injector of your
choice). When injected, the hook initializes MinHook, locates the target
window, installs the renderer hooks it finds, and draws the overlay on the
next presented frame.

---------------------------------------------------------------------------------

# Building

The project uses CMake and targets the MSVC toolchain (Windows SDK).

Requirements:

- Visual Studio 2022 (or Build Tools) with the C++ workload
- CMake 3.10+
- Windows SDK (DirectX headers)

Example for x64:

```bat
cmake -S LavenderHook -B build-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build-x64 --config Release
```

Example for x86:

```bat
cmake -S LavenderHook -B build-x86 -G "Visual Studio 17 2022" -A Win32
cmake --build build-x86 --config Release
```

The build produces `Release\LavenderHook.dll` for each architecture. The
project also ships `buildx64.bat` and `buildx86.bat` scripts.

---------------------------------------------------------------------------------

# License

## LavenderHook License (MIT License)

This project is licensed under the MIT License.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.

---------------------------------------------------------------------------------

# Third-Party Libraries

All licenses can be found under the `ThirdParty Licenses` folder.

## MinHook

MinHook is licensed under the 2-clause BSD License. It allows free use,
modification, and redistribution with attribution.

## miniaudio

miniaudio is released into the public domain under the Unlicense. It may
be freely used, modified, and distributed without restriction.

## Dear ImGui

Dear ImGui is licensed under the MIT License. You may freely use,
modify, and distribute it under MIT terms.

## stb

stb_image and stb_image_write are released into the public domain.

## Open Sans

Open Sans (c) 2010-2011 Google Corporation is licensed under the Apache
License, Version 2.0.
