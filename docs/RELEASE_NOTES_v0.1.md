Local split screen for GoldenEye: Source: up to 8 players on one PC, each with their own controller, on one or two monitors.

![8 players over 2 displays](https://raw.githubusercontent.com/T3chnobone/ges-code-8PlayerSplitscreen/ges-splitscreen/docs/screenshots/02-8-players-2-displays.png)

## Install

1. Download **GES-SplitScreenMod-Setup-0.1.0.exe** below and run it.
2. Restart Steam and start **GoldenEye: Source - Split Screen Mod** from your library, or use the desktop shortcut.

The setup checks for everything the mod needs and helps you get what's missing:

- **Steam**: if it isn't installed, setup opens the Steam download page.
- **Source SDK Base 2013 Multiplayer** (free on Steam): if it isn't installed, setup starts the install in Steam and waits until Steam has finished (click Retry when it's done).
- **GoldenEye: Source 5.0.6**: if setup can't find it, it opens the official download page ([geshl2.com/download](https://www.geshl2.com/download/)). Install it, restart Steam and run the setup again.

It finds GoldenEye: Source through Steam, backs up every file it replaces, and adds an uninstaller (Windows Settings > Apps, or the Start menu) that puts the originals back. It uses the game's original renderer, and asks whether you'd prefer Vulkan instead.

Windows may show "Windows protected your PC" because the setup isn't code-signed: click **More info > Run anyway**.

## What's new

- **Split screen for up to 8 players**: whole screen, halves, 2x2, 3x2 and 4x2 layouts, and a second monitor for players 5 to 8.
- **SPLIT SCREEN** on the main menu, with player profiles (name, character, look speed) and a Match page (map, mode, weapons, teams, time and kill limits).
- **Controllers**: Xbox, PlayStation, Switch Pro and more. Players press START to join. **A** jumps and **R3** uses.
- **Each player's own START menu**: the leaderboard with your health and armour dials around it, a Controller page (invert look, sensitivity, calibrate sticks), and Exit game or Commit Suicide and Respawn.
- **Match options**: jumping, radar, aim assist (Classic GoldenEye, Sticky aim or Slowdown) and win conditions.
- **Cheats**: **Big Head** and **Paintball** (colourful paint instead of bullet holes and blood, and paint running down your screen when you die).
- GoldenEye's own health and armour dials, ammo, radar and scoreboard in every view. The death screen plays in the dead player's own view, and spare screens stay blank until the match starts.
- A match report with names big enough to read from the couch.
- Menus that scale up for 1440p and 4K TVs.
- Runs on the engine's Vulkan renderer (`-vulkan`) as well as DirectX.
- Fixes: a crash when a player died after being shot, a crash when Exit game returned to the Split Screen menu, player 1 showing their Steam name instead of their profile name, jump animations, lag compensation, explosion crashes, Shader Editor materials (Stack's glass, Bunker's screens) and more.

Full list and controls: [README](https://github.com/T3chnobone/ges-code-8PlayerSplitscreen/blob/ges-splitscreen/README.md) and [install guide](https://github.com/T3chnobone/ges-code-8PlayerSplitscreen/blob/ges-splitscreen/INSTALL_SPLITSCREEN.md).

## Screenshots

All captured in game on Vulkan, from this release.

**8 players joined on the Split Screen menu**
![Split Screen menu with 8 players](https://raw.githubusercontent.com/T3chnobone/ges-code-8PlayerSplitscreen/ges-splitscreen/docs/screenshots/01-main-menu-8-players.png)

**Big Head cheat**
![Big Head](https://raw.githubusercontent.com/T3chnobone/ges-code-8PlayerSplitscreen/ges-splitscreen/docs/screenshots/03-big-head.png)

**Paintball cheat**
![Paintball](https://raw.githubusercontent.com/T3chnobone/ges-code-8PlayerSplitscreen/ges-splitscreen/docs/screenshots/04-paintball.png)

**Paint running down a dead player's screen**
![Paint death screen](https://raw.githubusercontent.com/T3chnobone/ges-code-8PlayerSplitscreen/ges-splitscreen/docs/screenshots/04c-paintball-death-screen.png)

**A player's START menu: Controller page**
![START menu Controller page](https://raw.githubusercontent.com/T3chnobone/ges-code-8PlayerSplitscreen/ges-splitscreen/docs/screenshots/05-start-menu-controller-page.png)

**START menu leaderboard with the health and armour dials**
![START menu leaderboard](https://raw.githubusercontent.com/T3chnobone/ges-code-8PlayerSplitscreen/ges-splitscreen/docs/screenshots/05b-start-menu-leaderboard-dials.png)

This is an unofficial fan build of GoldenEye: Source by Team GoldenEye: Source.
