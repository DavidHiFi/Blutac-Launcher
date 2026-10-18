<div align="center">

<img src="https://socialify.git.ci/DavidHiFi/QualityOfLifeModManager/image?description=1&font=Inter&language=1&name=1&owner=1&pattern=Plus&theme=Dark" alt="Quality of Life Mod Manager project card" width="100%">

# Quality of Life Mod Manager

**Install, update, launch and remove Call of Duty mods on [Plutonium](https://plutonium.pw).**

One window for the [Quality of Life series](https://github.com/DavidHiFi/Plutonium-QoL-Series), every other mod in your Plutonium folders, LAN and online play, ReShade and a LAN server.

<a href="https://github.com/DavidHiFi/QualityOfLifeModManager/releases/latest">
<img src="https://img.shields.io/badge/%E2%AC%87%EF%B8%8F%20DOWNLOAD%20LATEST%20RELEASE-2EA043?style=for-the-badge&labelColor=161B22" alt="Download the latest release" height="42">
</a>

<br><br>

<img src="https://img.shields.io/github/v/release/DavidHiFi/QualityOfLifeModManager?style=flat-square&label=version&color=8957E5&labelColor=161B22" alt="Latest version">
<img src="https://img.shields.io/github/downloads/DavidHiFi/QualityOfLifeModManager/total?style=flat-square&label=downloads&color=1F6FEB&labelColor=161B22" alt="Total downloads">
<img src="https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-DB6D28?style=flat-square&labelColor=161B22" alt="Platforms: Windows 10 and 11">

</div>

---

## Installation

1. Download **`QualityOfLifeModManagerSetup.exe`** from the [latest release](https://github.com/DavidHiFi/QualityOfLifeModManager/releases/latest) and run it. It installs for your user only, with no admin rights, and adds Start menu and desktop shortcuts.
2. Run the app. The first-run wizard asks for your name, a theme, your Plutonium folder and your game folders.
3. Open **Quality of Life** in the sidebar and press **Install** on Black Ops II.
4. Open **Black Ops II**, pick **LAN** or **Online**, and press **Start**.

Prefer no installer? Unzip **`QualityOfLifeModManager-portable.zip`** anywhere you can write to and run the exe inside it. The `.update.bin` file on the release page is for the app's own updater, not an installer.

## Features

### Quality of Life series

- **One-click mods:** install, update or remove each game's Quality of Life mod.
- **Black Ops II extras:** HD texture pack, custom sounds and PlayStation 5, Switch or Xbox button prompts.
- **Backups:** every install first copies your own textures, sounds, icons, ReShade setup, scripts and settings.
- **Put it back:** restore any backup from the Quality of Life page, or remove a pack and get your originals back.

### Playing

- **Play page:** one page per game with its art, a LAN or Online choice and a Start button.
- **LAN:** starts the game with the selected mod already loaded.
- **Online:** signs in with your Plutonium account and starts the game without opening Plutonium's launcher.
- **ReShade:** colour grading for every Plutonium game, kept in place while you play.
- **DLSS 5:** an optional switch for Black Ops II LAN games, once you import it from a folder that already has it.

### Mods and servers

- **Home:** a catalog of community mods with cover art and one-click install.
- **Mods:** drop a `.zip`, `.rar`, `.7z`, `.exe` or `.cll` file, or paste a GitHub link.
- **Safe uninstall:** every install saves a checkpoint, so removing a mod restores what it replaced.
- **LAN server:** start and stop a dedicated server with generated configs.

### The app

- **Fifteen themes:** Nocturne, Classic Things, OLED, the four Catppuccin flavours and six T3 palettes.
- **Four languages:** English, Spanish, Brazilian Portuguese and Russian.
- **Self-updating:** checks this repo's releases when it starts.

### Supported games

| Code | Game | Client |
|---|---|---|
| T6 | Black Ops II | Plutonium |
| T4 | World at War | Plutonium |
| T5 | Black Ops | Plutonium |
| IW5 | Modern Warfare 3 | Plutonium |
| T7 | Black Ops III | T7-CLL or BOIII Community |
| S1 | Advanced Warfare | S1-CLL |

## Notes

> [!NOTE]
> **Only the Black Ops II mod is out so far.** The World at War, Black Ops and Black Ops III mods in the series are planned.

> [!IMPORTANT]
> **Plutonium deletes ReShade every time it starts.** Tick **ReShade** on the Play page and the app's watchdog puts the files back as the game opens, then saves your in-game preset edits.

> [!NOTE]
> **Mods load on LAN only.** Online games start without a mod. Pick one from the game's own Mods menu once you are in.

> [!NOTE]
> **Settings live in `QualityOfLife.ini` next to the exe.** If your Plutonium folder is not `%LOCALAPPDATA%\Plutonium`, point the app at it from **Settings**.

<details>
<summary><b>Building from source</b></summary>

<br>

You need Qt 6 (Widgets, Network, Concurrent, Svg) with its MinGW kit, CMake and Ninja. The paths to all three sit at the top of `scripts\build.ps1`; edit them if yours live elsewhere.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Clean
dist\QualityOfLifeModManager.exe -installcheck
dist\QualityOfLifeModManager.exe -selftest -plutoniumdir <throwaway folder>
```

`build.ps1` writes `dist\` with the exe and the Qt runtime beside it. `scripts\build-setup.ps1 -NsisDir <NSIS folder>` checks every imported DLL, builds Setup with NSIS, unpacks the finished Setup into a throwaway folder and checks it again. `scripts\README.txt` covers the static single-exe route.

The version lives in `src/Version.h`. [AGENTS.md](AGENTS.md) explains the parts that are easy to break.

</details>

<details>
<summary><b>The two feeds</b></summary>

<br>

- `qol_home.json` is the Home catalog. Edit it and commit, and the app picks it up on its next start. A copy is built into the exe as a fallback.
- `qol_update.json` is the update feed. Regenerate it after each release with `python scripts\gen_update.py -o qol_update.json`, then check it with `python scripts\gen_update.py --verify qol_update.json`. Never edit it by hand.

</details>

---

## Support development

The manager is free and always will be. If it saves you some fiddling with mod folders, a coffee keeps it going.

[![Support DavidHiFi on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/davidhifi)

## Contributing

Bug reports with the game and mod involved, catalog fixes and pull requests all help.

<a href="CONTRIBUTING.md">
<img src="https://img.shields.io/badge/%F0%9F%A4%9D%20HOW%20TO%20CONTRIBUTE-8957E5?style=for-the-badge&labelColor=161B22" alt="How to contribute" height="36">
</a>

## Star history

Thanks to everyone using the manager, reporting mod feed bugs and sending fixes.

<a href="https://www.star-history.com/?repos=DavidHiFi%2FQualityOfLifeModManager&amp;type=date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=DavidHiFi/QualityOfLifeModManager&amp;type=date&amp;theme=dark&amp;legend=top-left" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=DavidHiFi/QualityOfLifeModManager&amp;type=date" />
    <img alt="DavidHiFi/QualityOfLifeModManager GitHub star history" src="https://api.star-history.com/chart?repos=DavidHiFi/QualityOfLifeModManager&amp;type=date" />
  </picture>
</a>

## Credits

- **Author:** Myself, [DavidHiFi](https://github.com/DavidHiFi)

| Who | What |
|---|---|
| **MestreTM** ([Cod Lan Launcher](https://github.com/MestreTM/CLL-Cod-Lan-Launcher)) | The app this one is forked from, at v1.2.0. The games list, mod installer, LAN server, base themes and updater are its work. |
| **JugAndDoubleTap** ([LanLauncher](https://github.com/JugAndDoubleTap/LanLauncher)) | The original LAN launcher that Cod Lan Launcher grew from, and the 7-Zip download the kit importer uses. |
| **xerxes-at** ([T4](https://github.com/xerxes-at/T4ServerConfigs), [T5](https://github.com/xerxes-at/T5ServerConfig) and [T6](https://github.com/xerxes-at/T6ServerConfigs) server configs) | The dedicated server configs the LAN server page downloads. |
| **AlterWare** ([alterware.dev](https://alterware.dev)) | Some of the standalone client sources. |
| **BOIII Community** ([BOIII-Community](https://gitlab.com/boiii-community/BOIII-Community)) | The alternative Black Ops III client. |
| **crosire** ([ReShade](https://reshade.me)) | ReShade, bundled unmodified for the colour grading. |
| **Catppuccin** ([catppuccin](https://github.com/catppuccin/catppuccin)) | The four Catppuccin colour palettes. |

Built on [Qt 6](https://www.qt.io), with [CMake](https://cmake.org), [Ninja](https://ninja-build.org), [MinGW-w64](https://www.mingw-w64.org), [NSIS](https://nsis.sourceforge.io) for Setup, [7-Zip](https://www.7-zip.org) for archives and [Python](https://www.python.org) for the feed scripts.

> Missing or wrong credit? [Open an issue](https://github.com/DavidHiFi/QualityOfLifeModManager/issues/new) and I'll fix it.

## Disclaimers

Cod Lan Launcher, the app this is forked from, was written by MestreTM. Most of the code this fork adds was developed with AI coding agents, including Claude Code and Codex. No single AI agent made the project.

Online play uses your Plutonium account. The app keeps your sign-in in Plutonium's own `config.json`, encrypted for your Windows user, exactly where the official launcher keeps it, and sends it only to Plutonium. Setup is not code-signed, so Windows SmartScreen may warn the first time you run it.

The app is LGPL-3.0, the license it inherited from Cod Lan Launcher. See [LICENSE](LICENSE).

<div align="center">
<br>
<sub>Not affiliated with Activision, Treyarch or Plutonium. Requires your own copies of the games and <a href="https://plutonium.pw">Plutonium</a>.</sub>
</div>
