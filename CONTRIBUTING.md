# Contributing to Blutac Launcher

Thanks for helping. Bug reports, ideas and pull requests are all welcome.

## Reporting a bug

[Open an issue](https://github.com/DavidHiFi/QualityOfLifeModManager/issues/new) and include:

- what you did, what you expected and what happened instead
- your Windows version, the app version (shown on the About page) and the game and mod involved
- a screenshot of any error box, and whether you played LAN or Online

A broken entry in the Home catalog or the update feed is a bug too. Name the mod and what it shows.

## Suggesting a feature

Open an issue that describes what you want to do and why. A screenshot or a mock-up helps.

## Sending a pull request

1. Fork the repo and create a branch from `main`.
2. Install Qt 6 (Widgets, Network, Concurrent, Svg) with its MinGW kit, plus CMake and Ninja. Set their paths at the top of `scripts\build.ps1`, then build:

   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Clean
   ```

3. Run the checks against the build in `dist\`. Use a throwaway Plutonium folder that holds an empty `bin\plutonium-bootstrapper-win32.exe`, never your real one:

   ```powershell
   dist\QualityOfLifeModManager.exe -installcheck
   dist\QualityOfLifeModManager.exe -selftest -plutoniumdir <throwaway folder>
   ```

   Every `-selftest` line should say PASS. If your change touches Setup, also run `scripts\build-setup.ps1 -NsisDir <NSIS folder>`.
4. Keep each pull request to one change, and say in the description how you tested it.

Read [AGENTS.md](AGENTS.md) before you start. It lists the rules that are easy to break: brand strings and URLs live only in `src/Version.h`, translation keys are the Portuguese source strings, and `qol_update.json` is regenerated with `scripts/gen_update.py`, never edited by hand.

## License

By contributing, you agree that your work is released under this repo's [LGPL-3.0 license](LICENSE).
