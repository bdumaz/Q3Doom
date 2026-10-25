# Q3Doom

Doom's levels, monsters and weapons played on the Quake III Arena engine. Doom 1 maps are converted to 3D at load time. You move and jump with Quake 3 physics, fight Doom's sprite monsters with Doom's weapons (or Quake 3's), and hear Doom's sounds and music.

It has two single player modes:

- **Doom Campaign:** play The Ultimate Doom's four episodes, with medals per difficulty.
- **DoomFrag:** Defrag-style time trials on any map, with VQ3 or CPM physics and several weapon and monster options.

An optional **Quake Visuals** mode redraws the maps with Quake 3 textures, item models and lighting.

**Multiplayer** plays Quake 3's Deathmatch, Team Deathmatch, Tournament and CTF with Doom's weapons and items, as Doom marines in five colours. It runs on Quake 3's maps and, for Deathmatch and Team Deathmatch, on all 36 Doom levels.

> Runs on 64-bit Windows and Linux.

## What you need

### Windows

- **Windows 10 or 11**, 64-bit.
- **Visual Studio 2022** with the "Desktop development with C++" workload.
- **CMake 3.20** or later.
- **SDL2 2.32.10**, the Visual C++ development package (`SDL2-devel-2.32.10-VC.zip` from the [SDL releases](https://github.com/libsdl-org/SDL/releases)). Extract it so that this folder exists:
  ```
  external/SDL2/SDL2-2.32.10/cmake
  ```
  To use another location, configure with `-DSDL2_DIR=<path to SDL2>/cmake`.

### Linux

- A 64-bit distribution (x86_64; aarch64 should work but is untested).
- GCC, CMake 3.20 or later, and the SDL2 and OpenGL development packages. On Debian or Ubuntu:
  ```
  sudo apt install build-essential cmake libsdl2-dev libgl-dev
  ```

### Game data (not included)

This repository contains no game data. You need your own copies of:

- **The Ultimate Doom:** `DOOM.WAD`
- **Quake III Arena:** `pak0.pk3` to `pak8.pk3` (the 1.32 point release, as in the Steam version)

## Build

From the repository root, on Windows:

```
cmake -S . -B build/x64 -A x64
cmake --build build/x64 --config RelWithDebInfo
```

On Linux:

```
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/linux -j
```

The engine and its game modules are copied into `run/` (with `SDL2.dll` on Windows).

## Run

1. Copy `DOOM.WAD` and `pak0.pk3` … `pak8.pk3` into `run/baseq3/`.
2. Start `run/q3doom.exe` on Windows, or `run/q3doom.x86_64` on Linux.

On Linux, settings and screenshots go to `~/.q3doom`.

The game starts on the main menu. **Single Player** leads to the Campaign and DoomFrag. Resolutions, borderless fullscreen and Quake Visuals are under **Setup → System**.

### Multiplayer

**Multiplayer** lists the games on your network and joins them. **Create** hosts a game: pick a Quake 3 map or a Doom level, the game type and the limits. Your name and marine colour are under **Setup → Player**.

To join a game on another network, open the console and type `connect <address>`. id's master servers are gone, so internet games don't show in the list.

A dedicated server runs without a window:

```
q3doom +set dedicated 1 +set g_doomDeathmatch 1 +map E1M1
```

On Doom levels, these settings change the rules:

| Setting | Default | Effect |
|---|---|---|
| `g_doomDeathmatch` | 0 (1 when hosted from the menu) | Deathmatch rules on Doom levels: deathmatch starts, all keys, multiplayer-only things |
| `g_doomMatchMonsters` | 0 | Keep the monsters in a match |
| `g_doomWeaponsStay` | 1 | Weapons stay where they are, as in Doom's deathmatch |
| `g_doomExitEndsMatch` | 0 | The exit ends the match instead of fragging whoever takes it |
| `g_spawnProtection` | 1500 | Milliseconds after a respawn when other players do no damage (until you fire) |

**Doom Duel** is a race through a Doom level, in rounds:
- One side starts at the level's start, the other at its exit. Each must reach the other's starting point, marked by a Quake 3 CTF flag.
- A round starts with a 3, 2, 1 countdown. The dead come back after 3 seconds at their own start, and the sides swap every round.
- The host chooses the rounds to win, Doom or Quake weapons, weapon and health pickups, and nightmare enemies. Dead enemies come back whenever a player dies.
- In teams, **To the Goal** sets how many players of a team must get there. Those who arrive watch the rest of the round.

Q3Doom uses its own network protocol (1068). It doesn't see or join Quake 3 servers, and Quake 3 can't join it.

## Tests

The libdoom tests read the IWAD from `GameData/DOOM.WAD` (or `-DDOOM_IWAD=<path>`). They are skipped when it is missing.

```
ctest --test-dir build/x64 -C RelWithDebInfo     # Windows
ctest --test-dir build/linux                      # Linux
```

## Tools

`doomtool` (built in `build/x64/RelWithDebInfo/`, or `build/linux/`) inspects a WAD: map statistics, texture and sound export, BSP building, and music rendering. Run it without arguments for its commands.

`tools/qvedit/qvedit.py` is the **Quake Visuals editor**. It shows Doom's wall textures, flats, skies and items next to the Quake 3 textures and models that replace them in Quake Visuals. Pick a Doom texture, then a Quake 3 one, and save; in the game, `vid_restart` reloads the result.

```
python tools/qvedit/qvedit.py [--game <folder with baseq3/ and q3doom/>]
```

It needs Python 3 and nothing else, and opens in your browser. Everything stays on your computer: the page is served on 127.0.0.1. It saves `q3doom/quakevisuals.dat` in the game folder. In the source tree it also saves `data/q3doom/quakevisuals.dat`, because a build copies that file over the run folder's copy.

## License

The source code is licensed under the GNU General Public License, version 2 or later (see `src/COPYING.txt`). It is based on id Software's Quake III Arena source release.

Doom and Quake are trademarks of id Software. The game data is not covered by this license and is not distributed here. This is an unofficial fan project, not affiliated with id Software, ZeniMax or Bethesda.
