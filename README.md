# Toy Story 2: Recompiled

A native PC port of **Toy Story 2: Buzz Lightyear to the Rescue** (Nintendo 64), built by statically recompiling the original game code with the [N64Recomp](https://github.com/N64Recomp/N64Recomp) toolchain and rendering it with [RT64](https://github.com/rt64/rt64). It is not an emulator: the game's MIPS code is translated to C ahead of time and compiled into a Windows executable.

**This repository and its releases contain no ROM, game assets, or copyrighted game data. You must supply your own legally obtained copy of the game.**

> Status: early pre-release (v0.1.0). The game is playable, but it has not been tested through every level yet, so expect rough edges. See *Known issues* below.

## Features

- Runs natively on Windows through RT64 (Direct3D 12 / Vulkan)
- High frame rate frame interpolation: the game logic still runs at its original rate, and RT64 generates in-between frames up to your monitor's refresh rate
- Widescreen
- Optional removal of the original draw-distance cutoff
- Modern controller and keyboard support through the RecompFrontend input stack
- MSAA and resolution scaling

## Requirements

- Windows 10 or 11, 64-bit
- A GPU with Direct3D 12 or Vulkan support
- Your own Toy Story 2: Buzz Lightyear to the Rescue N64 ROM (supported versions will be listed with the first release)

## Getting started

The first Windows build (v0.1.0 pre-release) is coming soon on the [Releases](../../releases) page.

1. Download the release zip and extract it.
2. Run `SpaceRanger.exe` and point it at your ROM.

## Known issues

- Buzz can show a thin dark band around his waist.
- Some particles, projectiles and HUD slides still move at the original frame rate instead of being interpolated.
- High-refresh frame pacing has been tested mainly on one machine. Please report any stutter, with your refresh rate and GPU.

Bug reports are welcome on the [Issues](../../issues) page.

## Building from source

Building needs your own ROM: the recompiled game code is generated locally from it and is never committed.

1. Install Visual Studio 2022 (C++ desktop workload), CMake and Python 3.
2. Put your ROM in `rom/` (see `rom/README.md`).
3. Run N64Recomp with the configs in `config/` to generate the recompiled code.
4. Configure and build with CMake (`build-native`, Release).

Detailed steps are in `docs/`.

## Credits

- [N64Recomp](https://github.com/N64Recomp/N64Recomp), N64ModernRuntime and RecompFrontend by Wiseguy and contributors
- [RT64](https://github.com/rt64/rt64) by Darío and contributors
- Reference projects: Zelda64Recomp, BanjoRecomp, Donkey Kong 64 Recompiled, Mischief Makers PC recomp

## Legal

This is an unofficial fan project. It is not affiliated with or endorsed by Disney, Pixar, Activision, Traveller's Tales or Nintendo. *Toy Story* and all related names are trademarks of their respective owners. No copyrighted game content is distributed; you need your own copy of the game.
