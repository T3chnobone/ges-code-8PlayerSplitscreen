# GoldenEye: Source split screen: install and build

Local split screen for GoldenEye: Source: up to 8 players on one PC, each with a controller,
on one or two monitors. How it works and every option: [SPLITSCREEN.md](SPLITSCREEN.md).

This is an unofficial build. It replaces the game's `client.dll` and `server.dll`, so keep the
backups described below.

---

## For players

### Requirements

- Windows 10 or 11 and Steam, signed in.
- **Source SDK Base 2013 Multiplayer** (free on Steam, app 243750). GoldenEye: Source runs on it.
- **GoldenEye: Source 5.0.6**, installed as a Steam source mod:
  `C:\Program Files (x86)\Steam\steamapps\sourcemods\gesource`.
- Controllers: Xbox, PlayStation, Switch Pro or anything SDL2 understands, up to 8. Without
  `SDL2.dll` the game falls back to XInput (Xbox-style controllers, 4 at most). Keyboard and mouse
  still drive the menus.

### What's in the release zip

| File in the zip | Copy it to (inside `sourcemods\gesource`) |
|---|---|
| `bin\client.dll` | `bin\client.dll` |
| `bin\server.dll` | `bin\server.dll` |
| `bin\SDL2.dll` (SDL 2.30.12) | `bin\SDL2.dll` |
| `bin\shadereditor.dll` | `bin\shadereditor.dll` |
| `bin\game_shader_generic_procshader.dll` | `bin\game_shader_generic_procshader.dll` |
| `materials\decals\paintball\` (8 splats) | `materials\decals\paintball\` (the Paintball cheat) |
| `materials\VGUI\hud\paintscreen\` | `materials\VGUI\hud\paintscreen\` (the Paintball cheat's death screen) |
| `resource\GameMenu.res` | `resource\GameMenu.res` |
| `resource\ClientScheme.res` | `resource\ClientScheme.res` |
| `resource\SourceScheme.res` | `resource\SourceScheme.res` |
| `resource\GESMenuScheme.res` | `resource\GESMenuScheme.res` (new) |
| `resource\GESPropScheme.res` | `resource\GESPropScheme.res` (new) |
| `resource\ges_splitscreen_english.txt` | `resource\ges_splitscreen_english.txt` (new) |
| `gameinfo.txt` | `gameinfo.txt` |
| `steam.inf` | `steam.inf` |

What the text files change:

- `resource\GameMenu.res`: adds **SPLIT SCREEN** to the main menu (`engine ges_splitscreen`).
- `resource\ClientScheme.res`: anchors the main menu to the bottom left, so the new entry fits
  under the title (`Main.Menu.X` 21, `Main.Menu.Y` 480, `Main.BottomBorder` 6: the X and the
  bottom gap are 1.5x, like the menu text).
- `gameinfo.txt`: runs the mod on Source SDK Base 2013 Multiplayer (`SteamAppId 243750`) with its
  search paths (`game+mod+mod_write+default_write_path`, `gamebin |gameinfo_path|bin`, the hl2
  VPKs). If yours already says 243750 with these paths, keep yours.
- `steam.inf`: version lines the current engine checks when the mod loads (`PatchVersion`,
  `ClientVersion`, `ServerVersion` 9546320, `ProductName gesource`).
- `resource\SourceScheme.res`: bigger main menu text from 1080p up (1.5x at 1080p, 2.25x at
  1440p, 3.75x at 4K); the Options and other GameUI dialogs get text up to 16 pixels, as big as
  their fixed size layouts hold.
- `resource\GESMenuScheme.res`, `resource\GESPropScheme.res` (new): the fonts of the mod's own
  menus (Split Screen, Create Server, credits, intro video) at the same sizes. The menus grow with
  them.
- `resource\ges_splitscreen_english.txt` (new): split screen's own text. For now one string,
  `GES_SS_Suicide`, the label of the START menu's Commit Suicide and Respawn button; change it
  there to rename the button (without the file the button says Commit Suicide and Respawn).

### Install

1. Quit the game.
2. **Back up** the files you're about to replace: copy each one to the same name with `.orig` on
   the end (for example `bin\client.dll` to `bin\client.dll.orig`).
3. Copy the files from the zip over, as in the table.
4. Restart Steam if GoldenEye: Source isn't in your library yet.

### Launch

Start **GoldenEye: Source** from the Steam library. Or run:

```
"C:\Program Files (x86)\Steam\steam.exe" -applaunch 243750 -game "C:\Program Files (x86)\Steam\steamapps\sourcemods\gesource"
```

Two monitors (players 5 to 8 on the second): run `ges_ss_span_monitors` in the console, or add
`-windowed -noborder -w <both widths together> -h <height>` to the launch options.

### Controller quick start

1. On the main menu, press any button on a controller: that controller is player 1. It drives the
   menus: D-pad or left stick to move, A / Cross to choose, B / Circle to go back. In a dialog
   (for example "Quit game? / Cancel") the D-pad moves between its buttons. The mouse
   works too (the right stick moves the cursor, the right trigger clicks).
2. Open **SPLIT SCREEN**: it opens on the **Match** page. On the **Profiles** tab, set each player's name, character and look speed.
   The stick dead zones of every controller are measured automatically while the sticks rest in
   the menus. To calibrate one controller, press **R3** (click the right stick) in the Split Screen
   menu: its card in the Players list (top right) says what to do (leave the sticks centred, then
   turn both all the way round) and shows the result.
3. On **Match**, pick the map, game mode, weapons, team game, jumping, radar, aim assist, win
   condition, time and kill limits, and how many players share each display.
4. Other players press **START** to join and **LB / RB** to pick their profile.
5. **Start match**. When the map loads, each screen asks for its player: press the **right
   trigger**. Player 1 can press **START** to start with whoever is in.

In the game:

| Button | Action |
|---|---|
| Left stick / right stick | Move / look |
| Right trigger | Fire |
| Left trigger | Aim mode |
| RB | Alt fire / zoom |
| X | Reload |
| Y / LB | Next / previous weapon |
| A | Jump (when the match allows jumping) |
| B or left stick click | Crouch |
| Right stick click | Use (doors, buttons) |
| START | Your menu: scoreboard, swap team, controller settings (invert, sensitivity, calibrate sticks), exit, commit suicide (player 1: the mouse works there too) |

Keyboard and mouse are off during split screen matches. The game's pause menu doesn't open during
a match: Esc (or a controller's own pause button) opens player 1's START menu instead, which has
Exit. The console (~) still works.

### Troubleshooting

- **Hide the mouse on the main menu while a controller is plugged in**: `ges_ss_menu_hide_mouse 1`
  in the console. `ges_ss_menu_pad_nav 0` stops the controller from driving the game's menus.
- **A character drifts or a stick feels slow to start**: open SPLIT SCREEN and press **R3** on that
  controller, leave its sticks alone until its card says so, then turn each stick all the way round. `ges_ss_pad_deadzone_auto 0` goes back to one fixed dead zone (`ges_ss_pad_deadzone`).
  The measured values are in `cfg\ges_splitscreen_pads.txt` (delete it to start over).
- **Nothing happens when I press START / only 4 controllers work**: `SDL2.dll` must be in
  `gesource\bin`. `ges_ss_status` in the console lists the controllers and who has which.
- **The game freezes and I can't get to Task Manager**: in exclusive fullscreen a frozen game can
  sit on top of everything. Use **Fullscreen (windowed)** or **Windowed** in Video options, or
  launch with `-windowed -noborder`. Ctrl+Shift+Esc then opens Task Manager over it.
- **Glass, monitors and some Complex walls look plain**: they use GoldenEye's Shader Editor. Check
  that `bin\shadereditor.dll` and `bin\game_shader_generic_procshader.dll` are the ones from this
  zip (the 2016 ones that come with 5.0.6 don't load on the current engine). If the editor can't
  load, the game draws those materials with normal shaders instead (glass see-through but without
  reflection, monitors still) rather than crashing.
- **The second screen stays black**: the window must span both monitors (see Launch).
- **Guests can't join**: the server needs free player slots. Matches started from the Split Screen
  menu use `maxplayers 16`.
- **Hosting a normal online game after split screen**: the match settings are server settings and
  stay set. Before hosting online, set `ge_wincondition 0`, `ge_allowjump 1`, `ge_allowradar 1`,
  `ge_ss_aimassist 0`, `ge_cheat_bighead 0` and `ge_cheat_paintball 0`.
- **Crash dumps**: Windows writes them to `%LOCALAPPDATA%\CrashDumps`. Include the newest
  `hl2.exe.*.dmp` and `gesource\console.log` (launch with `-condebug`) in a bug report.

### Uninstall

Quit the game and copy each `.orig` file back over the new one (for example `bin\client.dll.orig`
to `bin\client.dll`). Delete `bin\SDL2.dll` if you didn't have it before. The split screen profiles
and match settings are in `cfg\ges_splitscreen_profiles.txt` and `cfg\ges_splitscreen_match.txt`.
Delete them if you want them gone.

---

## For builders

### Toolchain (what this build was made with)

- Windows 10/11.
- **Visual Studio 2022** (17.x) with "Desktop development with C++": MSVC v143 (19.44), a Windows
  10/11 SDK, and the 32-bit (x86) tools. The game is 32-bit.
- **CMake** with the Visual Studio 2022 generator (3.21 or newer; built with 4.4.4. CMake 4.x
  needs `CMAKE_POLICY_VERSION_MINIMUM=3.5`, see below).
- Git.

### Dependencies

| Dependency | Version | Where it goes | Used for |
|---|---|---|---|
| Boost source tree | 1.83.0 (`boost_1_83_0.zip` from boost.org) | unpack anywhere, pass as `GES_BOOST_ROOT` | headers, and Boost.Python built from `libs/python/src` by `thirdparty/boost-python` |
| Python | 3.4.1, **32-bit** (`python-3.4.1.msi`) | install anywhere, pass as `GES_PYTHON_ROOT` (needs `include\` and `libs\python34.lib`) | the game's embedded Python (must match the game's `python34.dll`) |
| SDL2 | 2.30.12, 32-bit `SDL2.dll` (from the SDL GitHub releases, `SDL2-2.30.12-win32-x86.zip`) | runtime only: next to `client.dll` in `gesource\bin` | controllers (loaded at runtime, nothing to link) |
| FMOD Ex | the `fmodex.dll` the game ships | nothing to do | music (loaded at runtime by `game/client/ges/ge_fmodex.cpp`) |

### Updated engine headers

The current Source SDK Base 2013 Multiplayer engine (2025 update) changed several engine
interfaces. This branch updates the matching headers in `public\`: `basehandle.h`, `cdll_int.h`,
`const.h`, `eiface.h`, `filesystem/IQueuedLoader.h`, `game/client/IGameClientExports.h`,
`iachievementmgr.h`, `igameevents.h`, `inetchannel.h`, `inetmessage.h`, `iserver.h`,
`tier0/memoverride.cpp` and `vgui_controls/Panel.h`. Without them the DLLs load but crash, or
the engine refuses them. `cmake/msvc_compat` holds two small shims for current MSVC: `__iob_func`
for the old prebuilt `lib/common/curl.lib`, and the `<typeinfo.h>` old SDK files include.

### Build

From a Developer Command Prompt (or any shell with CMake on the PATH):

```
git clone --recursive https://github.com/T3chnobone/ges-code-Couch-Party.git
cd ges-code-Couch-Party
git checkout ges-splitscreen
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" "-DGES_BOOST_ROOT=C:/deps/boost_1_83_0" "-DGES_PYTHON_ROOT=C:/deps/py34"
cmake --build build --config Release --target client server
```

Keep the `-D` arguments in quotes in PowerShell, which otherwise splits `3.5` at the dot.
Building the whole solution also works.

### Output

- `build\game\Release\client.dll` (and `client.pdb`)
- `build\game\Release\server.dll` (and `server.pdb`)

Copy the two DLLs to `sourcemods\gesource\bin`. Keep the `.pdb` files next to your build to read
crash dumps.

### Shader Editor DLLs

`bin\shadereditor.dll` and `bin\game_shader_generic_procshader.dll` (glass, monitor screens) are
built separately, from Biohazard90's Source Shader Editor source with a small patch, after the
main build: see [shadereditor/README.md](shadereditor/README.md).

### Source map

- Split screen: `game/client/ges/ge_splitscreen_*`, `game/client/ges/hud/ge_splitscreen_hud.cpp`,
  `game/client/ges/vgui/ge_splitscreen_menu.cpp`, `game/server/ges/ge_splitscreen_server.*`,
  `game/shared/ges/ge_splitscreen_shared.h`, `CUserCmd::splitguests` in `game/shared/usercmd.*`,
  and `CViewRender::RenderSplitScreen` / `SetUpSplitScreenViews` in `game/client/view.cpp`.
- Engine update and Windows build fixes: `CMakeLists.txt`, `game/CMakeLists.txt`, `cmake/`,
  `public/` (above), `game/client/ges/ge_fmodex.*`.
- Shader Editor: `game/client/ShaderEditor/` (client side) and `shadereditor/` (the two DLLs).
- Optional standalone launcher: `launcher/` (see its README).
- Resource files for the release: `splitscreen_resources/` (same layout as `gesource`).
