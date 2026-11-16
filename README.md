# GoldenEye: Source - Couch Party (local split screen)

An unofficial build of [GoldenEye: Source](http://www.geshl2.com) 5.0.6 that adds **local split
screen for up to 8 players on one PC**, each with their own controller, on one or two monitors.
It runs on the current Source SDK Base 2013 Multiplayer (the 2025 engine update).

This is the `ges-splitscreen` branch. It replaces the game's `client.dll` and `server.dll`; the
GoldenEye: Source maps, models and sounds are unchanged.

- Install it, controls and troubleshooting: [INSTALL_SPLITSCREEN.md](INSTALL_SPLITSCREEN.md)
- How it works, every option and the full change list: [SPLITSCREEN.md](SPLITSCREEN.md)
- Building, forking and publishing a release: [INSTALL_SPLITSCREEN.md](INSTALL_SPLITSCREEN.md#for-builders) and [GITHUB_SETUP.md](GITHUB_SETUP.md)

## What's new

### Split screen
- Up to 8 players, each with their own view, HUD and controls. Layouts: whole screen, top / bottom
  halves, 2x2, 3x2 and 4x2.
- **Players per display** on the Match page; with two monitors, players 5 to 8 go on the second.
- A **SPLIT SCREEN** entry on the main menu. **Profiles** page: names, character (with a picker),
  look speed and invert per profile. **Match** page: map, game mode, weapons, teams and the
  options below, with a diagram of how the screens will be split.
- A lobby list in the top right: players press START to join and LB / RB to pick a profile.
- When the map loads, each screen asks for its player (right trigger) and, in team games, a team.
- **START** in a match opens that player's own menu in their view: leaderboard, swap team, change
  profile, and exit with a confirmation (player 1 exiting ends the match and shows the results).
  The game's own pause menu no longer covers everyone's view during a match.
- A grid cell nobody plays in shows the live scoreboard.

### Controllers
- Xbox, PlayStation, Switch Pro and other pads through SDL2 (XInput, up to 4 Xbox-style pads,
  without it).
- The first controller to press a button on the main menu is player 1. It drives all the game's
  menus and dialogs: D-pad / left stick to move, A / Cross to choose, B / Circle to go back. The
  mouse keeps working.
- Stick dead zones are measured automatically per controller and saved. They get 0.01 extra so a
  character standing still doesn't creep. **R3** in the Split
  Screen menu recalibrates just that controller, with the steps shown on its card.

### Match options
- **Jumping allowed**, **Radar**, and **Aim assist** for controller players: Off, Classic
  GoldenEye (shots home in on an enemy near the crosshair), Sticky aim, or Slowdown near target.
  Teammates are never targets.
- **Win conditions**: highest score when time runs out, first to a number of kills, or best
  kill/death ratio; with time and kill limits.
- **Cheats** page (from the Match page): on / off cheats for everyone, starting with **Big Head**
  (every character's head drawn twice as big; looks only, hitboxes unchanged).

### HUD (per view)
- GoldenEye's own health and armour dials, ammo counter, radar (centred on that player) and
  scoreboard in every view, scaled to the view.
- The blood runs down a dying player's whole view, and they keep looking through their body's
  eyes until they respawn.
- Readable START menu scoreboard: large text, sized to the view.

### Engine and rendering fixes
- The Shader Editor (Stack's glass, Bunker's and Silo's screens, Complex's walls) rebuilt from
  source for the current engine; the 2016 DLLs didn't load. If it ever fails to load, those
  materials fall back to standard shaders instead of crashing (this was the Stack crash).
- Props and entities near guests fade and appear correctly in every view.
- Mirrors no longer black out parts of the world in the views: while the screen is split their
  reflection pass is skipped, so mirrors and reflective glass don't reflect then.
- Guests' first person guns, muzzle flashes and animations are correct in their own views.

### Gameplay fixes that also affect online play
- Players visibly jump (the jump animation never started).
- Fire, reload and jump animations no longer play twice on your own player model, and **lag
  compensation works again** for everyone's shots: the server had treated every player as not
  predicting, because the current engine doesn't send `cl_predict`.
- Grenade, rocket and mine explosions no longer crash the server.

## Known limitations and tips
- Jumping, radar, aim assist and cheats are server settings and stay set after a split screen match.
  Before hosting a normal online game, set `ge_wincondition 0`, `ge_allowjump 1`,
  `ge_allowradar 1`, `ge_ss_aimassist 0` and `ge_cheat_bighead 0` in the console.
- A frozen game in exclusive fullscreen can sit on top of everything. Use **Fullscreen
  (windowed)** or **Windowed** in Video options (or launch with `-windowed -noborder`); then
  Alt+Tab and Ctrl+Shift+Esc work over it.
- Sound is heard from player 1's position, and full-screen flashes and fades aren't per view
  (the engine has one listener and one screen).
- Each extra player renders the whole scene again, so 5 to 8 players need a strong PC.
- The engine's own `-vulkan` launch option (Source SDK Base 2013 Multiplayer ships its Vulkan
  renderer) works with this build. Nothing in this branch adds or changes Vulkan or DXVK.
- More in [SPLITSCREEN.md](SPLITSCREEN.md#requirements-and-limitations).

## Original GoldenEye: Source README
### About
A total conversion modification of Half-Life 2. We are a fan made artistic 
recreation, released for free, with a primary goal in mind;  To bring the 
memories and experiences from the original GoldenEye-64 back to life using 
the Source SDK 2013 Technology. As one of the premier and consistently released 
HL2 mods, GES has been seen out and about, reviewed by the best, and played by 
hundreds and thousands of PC gamers for Eight long years and running.

Visit our [website](http://www.geshl2.com) and [forums](http://forums.geshl2.com)!

### Current State
The last major public release of GES was v5.0 on August 12, 2016 using the 2007 Source SDK. 

### Navigating the Code
The main code base for GES is located in the following directories:

- /game/client/ges
- /game/server/ges
- /game/shared/ges

TODO: Write more specific code navigation readme

### How to build
#### Prerequisites
You must have the Boost libraries (v1.49  or higher) installed on your system path such that CMAKE can find them.
You can download Boost [here](https://sourceforge.net/projects/boost/files/boost/) or through your distro's package manager.

You must also have CMAKE v2.8.12 or higher installed on your system, available [here](https://cmake.org/download/) or through
your distro's package manager.

#### Linux Build Process
1. Checkout the repository using git
2. Open a terminal window in the root of the repository you just checked out
3. Initialize the submodules: ```git submodule init --recursive```
4. Build with CMake: ```mkdir build && cd build```
5. ```cmake -DCMAKE_INSTALL_PREFIX=[path_to_gesource] ..```
6. ```make install```

#### Windows Build Process
1. Checkout the repository using git and initialize the submodules (recommend TortoiseGit)
2. Create a build directory in the repository's root
3. Open CMAKE GUI and configure the project for the visual studio version you are using (prefer 2015)
4. Open Visual Studio using the generated solution files
5. Build and install as usual

### Third-Party Libraries
GES relies on several third-party libraries during compilation. We have an integrated
Python 3.4 interpreter to run our Gameplay Scenarios and AI. To drive all this, we use
the Boost Python library. To play our music, we utilize FMOD since Valve's internal
sound functions do not operate across level transitions.

### Contributing
We will accept pull requests and bug reports from anyone. All we ask at this state
is to limit your requests to bug fixes and compilation fixes. New capability and major
code revisions will not be accepted until further notice.

## Code License
GoldenEye: Source original code (files starting with 'ge_') is licensed under the
GNU GPL v3. The full license text can be found in LICENSE_GES.

It's important to know that this code is supplied WITH ABSOLUTELY NO WARRANTY WHATSOEVER.
We are not responsible for any damages resulting from the use of this code. Please see the
license for more information.

TODO: Replace file headers to comply with GNU GPL v3
