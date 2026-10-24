# Q3Doom

Doom's levels, monsters and weapons played on the Quake III Arena engine. Doom 1 maps are converted to 3D at load time. You move and jump with Quake 3 physics, fight Doom's sprite monsters with Doom's weapons (or Quake 3's), and hear Doom's sounds and music.

It has two single player modes:

- **Doom Campaign:** play The Ultimate Doom's four episodes, with medals per difficulty.
- **DoomFrag:** Defrag-style time trials on any map, with VQ3 or CPM physics and several weapon and monster options.

An optional **Quake Visuals** mode redraws the maps with Quake 3 textures, item models and lighting.

> Windows 64-bit only for now. A Linux build is planned.

## What you need

- **Windows 10 or 11**, 64-bit.
- **Visual Studio 2022** with the "Desktop development with C++" workload.
- **CMake 3.20** or later.
- **SDL2 2.32.10**, the Visual C++ development package (`SDL2-devel-2.32.10-VC.zip` from the [SDL releases](https://github.com/libsdl-org/SDL/releases)). Extract it so that this folder exists:
  ```
  external/SDL2/SDL2-2.32.10/cmake
  ```
  To use another location, configure with `-DSDL2_DIR=<path to SDL2>/cmake`.

### Game data (not included)

This repository contains no game data. You need your own copies of:

- **The Ultimate Doom:** `DOOM.WAD`
- **Quake III Arena:** `pak0.pk3` to `pak8.pk3` (the 1.32 point release, as in the Steam version)

## Build

From the repository root:

```
cmake -S . -B build/x64 -A x64
cmake --build build/x64 --config RelWithDebInfo
```

The engine, its game modules and `SDL2.dll` are copied into `run/`.

## Run

1. Copy `DOOM.WAD` and `pak0.pk3` … `pak8.pk3` into `run/baseq3/`.
2. Start `run/q3doom.exe`.

The game starts on the main menu. **Single Player** leads to the Campaign and DoomFrag. Resolutions, borderless fullscreen and Quake Visuals are under **Setup → System**.

## Tests

The libdoom tests read the IWAD from `GameData/DOOM.WAD` (or `-DDOOM_IWAD=<path>`). They are skipped when it is missing.

```
ctest --test-dir build/x64 -C RelWithDebInfo
```

## Tools

`doomtool` (built in `build/x64/RelWithDebInfo/`) inspects a WAD: map statistics, texture and sound export, BSP building, and music rendering. Run it without arguments for its commands.

## License

The source code is licensed under the GNU General Public License, version 2 or later (see `src/COPYING.txt`). It is based on id Software's Quake III Arena source release.

Doom and Quake are trademarks of id Software. The game data is not covered by this license and is not distributed here. This is an unofficial fan project, not affiliated with id Software, ZeniMax or Bethesda.
