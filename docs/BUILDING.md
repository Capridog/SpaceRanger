# Building Toy Story 2: Recompiled

The build is Windows-only for now, and some scripts still assume the project
lives at `C:\SpaceRanger`. Clone or copy the repository there.

Nothing derived from the ROM is in this repository. You generate the
recompiled code from your own ROM, so every build needs one.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 with the "Desktop development with C++" workload
- CMake 3.20 or newer and Git
- Python 3.11 or newer (`tomllib` is used), installed as a virtual environment
  at `.venv` (`python -m venv .venv`)

## 1. ROM

Copy your Toy Story 2 (N64, USA) ROM to `rom/baserom.us.rev0.z64`.

## 2. Dependencies

Dependencies are upstream repositories plus SpaceRanger patches.
`patches/deps/MANIFEST.md` lists each one's upstream URL, the base commit, and
its patch:

```powershell
git clone https://github.com/rt64/rt64 deps/rt64
git -C deps/rt64 checkout <base commit>
git -C deps/rt64 submodule update --init --recursive
git -C deps/rt64 apply ../../patches/deps/deps_rt64.patch
```

Repeat the same steps for `deps/rt64/src/contrib/plume`,
`deps/N64ModernRuntime` (and its `N64Recomp` submodule),
`deps/RecompFrontend`, and `tools/N64Recomp`.

## 3. Recompile the game code

Build N64Recomp (and RSPRecomp) from `tools/N64Recomp` into
`build/N64Recomp/Release`. Then run:

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_overlays_pass10.ps1
```

The script audits the ROM against the function map in `config/`, then writes
the recompiled C into `generated/<run id>`. Copy that folder to
`RecompiledFuncs/`.

Generate the audio microcode with RSPRecomp from `rsp/ts2_audio.toml`; it writes
`rsp/ts2_aspMain.cpp`.

## 4. Build

```powershell
cmake -S . -B build-native -G "Visual Studio 17 2022" -A x64
cmake --build build-native --config Release --target spaceranger
```

The game is `build-native/Release/SpaceRanger.exe`. To make a player zip without
ROM data, run `tools/package_release.ps1`. It packages the exe as
`ToyStory2Recompiled.exe` inside a `ToyStory2Recompiled` folder.

## Testing tools

- `TS2_INPUT_RECORD=<file>` records controller input, frame-tagged.
- `TS2_INPUT_REPLAY=<file>` plays a recording back exactly, frame by frame.
- `tools/replay_test.ps1` runs a replay with timing logs.
