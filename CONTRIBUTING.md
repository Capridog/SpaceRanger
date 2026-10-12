# Contributing

Fixes, improvements and bug reports are welcome.

## Reporting a bug

Open an [issue](../../issues) and include:

- What happened and where in the game (level, what you were doing)
- Your GPU, Windows version and monitor refresh rate
- The release version, or the commit if you built it yourself

## Sending a fix

1. Fork the repository and create a branch for your change.
2. Follow [docs/BUILDING.md](docs/BUILDING.md) to build with your own ROM.
3. Keep each pull request focused on one fix or feature, and describe how you
   tested it.
4. Changes to a dependency (RT64, N64ModernRuntime, RecompFrontend, N64Recomp)
   go in its patch: commit inside that dependency, then run
   `tools/export_dep_patches.ps1` and commit the updated `patches/deps` files.

## Never commit game data

Do not commit or attach the ROM, recompiled game code (`RecompiledFuncs/`,
`generated/`, `rsp/*.cpp`), extracted assets, or screenshots of disassembly.
Pull requests that include them can't be accepted.

## License

By contributing you agree that your changes are released under the
[GPL-3.0](LICENSE) license, like the rest of the project.
