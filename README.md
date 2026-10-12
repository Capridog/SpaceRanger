# SpaceRanger

SpaceRanger is a project that uses [N64: Recompiled](https://github.com/N64Recomp/N64Recomp) to **statically recompile** *Toy Story 2: Buzz Lightyear to the Rescue* (Nintendo 64) into a native Windows port, rendered with [RT64](https://github.com/rt64/rt64).

### [Check out the latest release here](../../releases/latest).

### **This repository and its releases do not contain game assets. The original game is required to build or run this project.**

> **Status:** early pre-release (v0.1.0). The game is playable from start to finish, but still has work ahead of it, so expect rough edges. See [Known Issues](#known-issues).

## Table of Contents
* [System Requirements](#system-requirements)
* [Features](#features)
* [Planned Features](#planned-features)
* [FAQ](#faq)
* [Known Issues](#known-issues)
* [Building](#building)
* [Contributing](#contributing)
* [Libraries Used and Projects Referenced](#libraries-used-and-projects-referenced)
* [License](#license)

## System Requirements
* Windows 10 or 11, 64-bit.
* A GPU supporting Direct3D 12.0 (Shader Model 6) or Vulkan 1.2.

If the game crashes on startup, make sure your graphics drivers are fully up to date.

## Features

#### Plug and Play
Run `SpaceRanger.exe`, choose **Load ROM** on the title screen and select your copy of the North American (USA) version of the game, then start playing. There is no separate extraction step: assets are loaded from your copy of the game. `.z64`, `.n64` and `.v64` dumps all work. You can also place the ROM next to `SpaceRanger.exe` before the first launch and it will be found automatically.

#### High Framerate Support
The game logic still runs at its original rate, and RT64 generates in-between frames up to your monitor's refresh rate.

#### Widescreen
The view expands to fill widescreen displays.

#### Full Draw Distance
The original game's distance culling is removed, so objects and enemies are drawn across the whole level.

#### Easy-to-Use Menus
An animated title screen, plus in-game menus for graphics, controls and sound: resolution scaling, MSAA, refresh rate, window mode, and controller and keyboard bindings.

## Planned Features
* Interpolation for the remaining effects (particles, projectiles, HUD slides)
* Testing and fixes across every level
* Linux support
* Cheat Codes
* Randomizer

## FAQ

#### What is static recompilation?
Static recompilation is the process of automatically translating an application from one platform to another. For more details, check out [N64: Recompiled](https://github.com/N64Recomp/N64Recomp).

#### Is this an emulator?
No. The game's code was translated to native code ahead of time. It is not an emulator, and it cannot run any arbitrary ROM.

#### Where is the savefile stored?
In the `spaceranger-config` folder next to `SpaceRanger.exe`, along with your settings and the validated copy of your ROM. The game always runs portably from its own folder.

#### How do I choose a different ROM?
**You don't.** This project only supports the US version of the N64 release of Toy Story 2: Buzz Lightyear to the Rescue. ROMs in byteswapped formats are converted automatically if they're the correct version. To start over with a fresh ROM, delete the `spaceranger-config` folder.

## Known Issues
* Buzz can show a thin dark band around his waist.
* Some particles, projectiles and HUD slides still move at the original frame rate instead of being interpolated.
* Frame pacing has mostly been tested on one PC. If you see stutter, please [open an issue](../../issues) with your GPU and refresh rate.
* Overlays such as MSI Afterburner can cause performance issues.

## Building
Building is not required to play this project; prebuilt binaries, which do not contain game assets, are on the [Releases](../../releases) page. To build it yourself, see [docs/BUILDING.md](docs/BUILDING.md). You'll need your own ROM, since the recompiled game code is generated locally from it and is never committed.

## Contributing
Pull requests and bug reports are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md).

## Libraries Used and Projects Referenced
* [N64: Recompiled](https://github.com/N64Recomp/N64Recomp) for the static recompiler
* [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) for the runtime that runs the recompiled code
* [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) for the menus, input and launcher framework
* [RT64](https://github.com/rt64/rt64) for the project's rendering engine
* [RmlUi](https://github.com/mikke89/RmlUi) for building the menus
* [FreeType](https://freetype.org/) for font rendering, used by RmlUi
* [SDL2](https://www.libsdl.org/) for windowing, input and audio
* [Fredoka](https://fonts.google.com/specimen/Fredoka) and [Bungee](https://fonts.google.com/specimen/Bungee) fonts for the title screen
* [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp) and [BanjoRecomp](https://github.com/BanjoRecomp/BanjoRecomp) as reference projects

## License
This project's own code and title-screen art are released under the [GPL-3.0](LICENSE) license, the same as Zelda64Recomp and BanjoRecomp. Dependencies and fonts keep their own licenses (see [THIRD-PARTY-NOTICES](tools/release/THIRD-PARTY-NOTICES.txt)).

*SpaceRanger is an unofficial fan project. It is not affiliated with or endorsed by Disney, Pixar, Activision, Traveller's Tales or Nintendo. Toy Story and all related names are trademarks of their respective owners.*
