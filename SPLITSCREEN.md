# Local split screen

Up to 8 players can play on one PC. Player 1 chooses how many players share each display (1 to 8)
on the Split Screen menu's Match page, and every player's view is the same size. With the game
window spanning two monitors, each monitor gets that many players (8 at most in total).

## How it works

Source SDK 2013 supports one local player per client and one game window, and the engine
is closed source. This feature is built entirely in the mod code:

- **Player 1** is the normal local player.
- **Players 2 to 8 are "guests"**. A guest is a fake client that the server creates and that
  belongs to player 1's client (`game/server/ges/ge_splitscreen_server.*`). Guests play as
  normal players, not bots.
- **Guest input** is read from controllers by the client
  (`game/client/ges/ge_splitscreen_gamepad.*`). It is sent to the server inside player 1's
  own usercmds, in `CUserCmd::splitguests` (`game/shared/usercmd.*`). Guest movement runs in
  the same tick as player 1's.
- **Rendering**: `CViewRender::RenderSplitScreen` (`game/client/view.cpp`) draws one view per
  player into its viewport. Each player's own body, weapon and hat are hidden in their own
  view. Guests' viewmodels are sent to the owning client and only drawn in that guest's view.
- **HUD**: while the screen is split, the normal HUD (which belongs to player 1) is hidden,
  except chat and kill feed, and so is the full-screen spectator bar. Each view shows its own
  name, time left, score (and team scores), health and ammo, with fonts sized for the screen.
  Health and armour only appear for a few seconds when that player is hit, healed, picks up
  armour or spawns, then fade out, like the single player game (`ges_ss_hud_bars_hit 0` keeps them
  on screen, `ges_ss_hud_bars_time` sets how long they stay). `CGESplitScreenHud`
  (`game/client/ges/hud/ge_splitscreen_hud.cpp`) draws a compact HUD and the join menus in
  each viewport.

This also works on a dedicated server: guests ride on their owner's connection.

## Split Screen menu (main menu)

One main menu entry opens the Split Screen menu (`ges_splitscreen`). Add it to the mod's
`resource/GameMenu.res`:

```
"SplitScreen"
{
	"label"		"Split Screen"
	"command"	"engine ges_splitscreen"
}
```

The menu has two pages, **Profiles** and **Match**, picked with the tabs at the top (or Y).
`ges_splitscreen_profiles` and `ges_splitscreen_match` open it on that page.

### Profiles page

Each profile has a name, a character (or random), controller sensitivity (0.1 to 3.0),
inverted look, and **Invited**. Only invited profiles are offered when a player joins, and a
"Guest" entry is always available. Profiles are saved to `cfg/ges_splitscreen_profiles.txt`.
The page also lists connected controllers and which players are ready (the lobby).

### Using the menu with a controller

Every player's controller can drive the menu, not only player 1's:

| Controller | In the menu |
|---|---|
| D-pad up / down | Move between settings (a gold outline shows where you are) |
| D-pad left / right | Change the setting: next / previous choice, slider, number, tick box |
| A | Press a button, tick a box, or type in a text field |
| B | Close the menu |
| Y | Profiles / Match page |
| START | Join (or leave) the next match |
| LB / RB | Previous / next profile for that player |

Holding the d-pad repeats, and speeds up after a second and a half (handy for kill limits).
Other players' controllers can also use the left stick like a d-pad; player 1's sticks still move
the mouse cursor, and its right trigger still clicks.

### Typing with a controller

Pressing A on a text field (the profile name) opens the game's on-screen keyboard: a grid of
letters, numbers and symbols with Shift, Space, Delete and Done keys. D-pad moves, A types,
X deletes, Y adds a space, LB is shift, RB finishes, B cancels. The first letter is a capital.
The mouse and the real keyboard also work on it.

### Match page (game modes and win conditions)

On the Match page you pick:

- **Map**: any installed map.
- **Game mode**: the same scenarios as online matches: Deathmatch, The Man with the Golden
  Gun, License to Kill, You Only Live Twice, Arsenal, Gun Trade, Dr. No Armor, A View to a
  Kill, The Living Daylights, Capture the Key and Tournament Deathmatch. Custom scenarios
  found in `python/ges/GamePlay` are listed too.
- **Weapons**: which weapons are on the map, like the N64 game's weapon options. The list is
  the same weapon sets online servers use (for example Power Weapons or Proximity Mines),
  read from `scripts/loadouts/weapon_sets_default.txt` and any `weapon_sets_custom*.txt`, so
  custom sets show up too. "Random" picks a new set each round. Gun Trade hands out its own
  weapons, so the list is greyed out for it.
- **Team game**: MI6 vs Janus. It is greyed out for modes that are always or never team
  games (for example the Golden Gun is never a team game, Capture the Key always is).
- **Jumping allowed**: `ge_allowjump` for everyone in the match (guests too). Jump is the right
  stick click.
- **Radar**: `ge_allowradar` for everyone in the match. Each player sees GoldenEye's own radar in
  their view (see Changes, 15 November). A game mode that forces the radar on still shows it.
- **Aim assist** (`ge_ss_aimassist`, saved as `aimassist`): only for the players on controllers
  (guests, and player 1 when on a controller), never keyboard and mouse, and never at teammates in
  a team game. Targets must be in sight and within 2500 units.
  - *Off*.
  - *Classic GoldenEye*: like the N64 game. A shot goes to an enemy close to the crosshair (within
    the target's body plus about 4 degrees, so wider up close); the view doesn't move. Worked out
    by the server, so the hits count. `ge_ss_aimassist_classic_cone` (4) and `_maxdist` (2500).
  - *Sticky aim*: while the crosshair is on or just off an enemy, the view turns with them as they
    move or as you strafe past (`ges_ss_aim_sticky`, 0.85 of their movement).
  - *Slowdown near target*: looking around slows to `ges_ss_aim_slowdown` (0.4) over an enemy,
    easing back to full speed at twice the target's size away.
- **Cheats...** (bottom left) opens the **Cheats** page: on / off cheats for everyone in the match,
  saved with the match (`cheat_<name>`); B or Back returns to the Match page.
  - *Big Head* (`ge_cheat_bighead`): every character's head is drawn twice as big, players and
    ragdolls, in every view (hats grow with it). Looks only: the hitboxes are unchanged.
- **Win condition**:
  - *Timed, highest score*: the mode's normal scoring when the time limit runs out.
  - *First to N kills*: the match ends as soon as a player (or team) reaches the kill limit.
    With a time limit, the most kills wins when time runs out. 0 minutes means no time limit.
  - *Timed, best kill/death ratio*: when time runs out, the best kills divided by deaths
    wins (no deaths counts as 1). Ties go to the player with more kills.
- **Time limit** (minutes) and **kills to win**.

Kills of teammates and suicides don't count as kills; suicides do count as deaths. While
kills or K/D decides the match, each player's split screen HUD shows their kills
(`Kills 7 / 20`) or ratio (`K/D 1.40 (7/5)`) instead of the score.

Matches start straight into play: teams are set before the map loads, so there's no restart and
no empty round report. A round that ends with nothing scored shows no report either; the match
report at the end of a match always shows.

**Start match** saves the settings to `cfg/ges_splitscreen_match.txt` and loads the map with
`maxplayers 16`. The limits are set again a second after the map loads, because a
mode's own config file can change them. `ges_splitscreen_match_start` starts a match with the
saved settings without opening the menu.

These are server settings (`ge_wincondition`, `ge_killlimit`), so they also work on a
dedicated server. They stay set until changed, so set `ge_wincondition 0` before hosting a
normal online game in the same session.

## Players per display and the screen layout

**Players per display** (Match page) sets how many players share each display. It only goes up to the number of players who have joined in the Split Screen menu (player 1 plus everyone who pressed START, as listed in the top right), shared across the displays (display 1 first), and shows "N players joined" next to it. It follows players joining and leaving while the menu is open, and player 1's choice comes back when players rejoin. 4 on one display
gives 4 players, on two displays 8; 5 on one display gives 5. The page shows a diagram of the
displays and how they will be split. Every view is the same size, in an equal grid per display:

| Players | Grid |
|---|---|
| 1 | Whole screen |
| 2 | Top / bottom halves |
| 3-4 | 2x2 |
| 5-6 | 3x2 |
| 7-8 | 4x2 |

A grid cell nobody plays in (the 4th quarter with 3 players, the 6th cell with 5) shows the
**scoreboard**: GoldenEye's own one, as when holding TAB online (players by score with their character,
deaths and ping, MI6 and Janus sections in team games, map, time left and game mode). Any further spare cells show the GoldenEye logo.

## Joining the match (before it starts)

When a match from the Split Screen menu loads, the screen splits into its final layout and the game
waits: no timers, no music, and nobody can move or fire. One by one, each screen asks for its
player: **press the right trigger** to take it (screen 1 is player 1's controller). In a team game
that screen then asks for a team: **left stick left / right** for MI6 or Janus, **right trigger**
to confirm. Players who are in can change their profile with LB / RB, and B gives a screen back.
The match (clocks, music, movement) starts when every screen has a player. Player 1 can press START
to begin with the players who are in so far; the screens nobody took close and the layout is worked
out again for the players who are playing. The spare cell lists who is in while this happens.
## Joining

- **In the menus (lobby)**: profiles are chosen here, not in the game. Player 1 is always in; the
  other players press START to join (START again leaves). Each player then rotates through the
  invited profiles and Guest with LB / RB (profiles another player is using are skipped). The
  Split Screen menu's Profiles page lists who is in and with which profile. Everyone joins with
  their profile when the match starts; nobody gets a picker at the start of the game.
- **In a game**: a player presses START and joins at once with the next free profile.
- **While playing**: START opens that player's menu in their own view (the game carries on). It
  opens on the **leaderboard** (GoldenEye's own scoreboard, with their row highlighted and their health and armour shown); left / right go to the
  other pages: **Swap team** (team games: A or the right trigger moves them to the other team and
  respawns them at once), **Change profile** (LB / RB, or A for the list) and **Exit game** (No / Yes
  with up / down, then A or the right trigger). A player who exits leaves the match and the screen
  is split again for the others. When player 1 exits, the match ends for everyone, the results
  (match report) are shown and then everyone goes back to the Split Screen menu. START or B closes
  the menu. The Back button does nothing in split screen.
- Guests stay through map changes. They leave when player 1 leaves.

Controllers belong to players in the order they press a button. When the game starts no
controller is player 1 yet, and the main menu shows "Press any button on a controller to be
player 1". The first controller to press any button becomes player 1: it drives the menus and
is player 1's controller in matches. The press that claims player 1 doesn't also click anything.
The next controllers become players 2 to 8 in the order they press a button. A controller only
ever drives one player. Mouse and keyboard always work for player 1 too.

If player 1's controller is unplugged, the next controller to press a button takes over (a
spare one, or one whose player hasn't joined yet). An unplugged guest who was ready in the menus
is un-readied; one who is playing stays in the game, idle, until a spare controller presses a
button. `ges_ss_status` shows which controller belongs to which player.

Set `ges_ss_host_pad 0` if player 1 only uses keyboard and mouse; every controller is then a guest.

During a match, a player whose controller is unplugged keeps their place: their view shows
"Controller disconnected" until a spare controller presses a button and takes it over. Another
player's controller never takes over player 1 mid-match; that only happens in the menus.

**Keyboard and mouse are off during split screen matches** (`ges_ss_kbm_off 1`): only controllers
play, player 1 included. Esc, the console key and the menus still use the keyboard and mouse.
Online and normal games are unchanged.

The first time the Split Screen menu opens it explains this, with a "Don't show this again" tick
box (`ges_ss_hide_intro`).

**No spectators.** Split screen players never become spectators. Player 1 spawns as soon as the
map loads with the profile chosen in the lobby (or the first free one) and the auto team. Guests spawn as soon as they join. Team
changes respawn the player straight away, with no suicide or lost point. `spectate` and
`jointeam 1` are ignored for them, and GoldenEye's full-screen spectator bars are hidden. A
mode that eliminates players (You Only Live Twice when out of lives) shows "Out - waiting for
the next round" in that player's own view.

## Controls

| Controller | Action |
|---|---|
| Left stick | Move |
| Right stick | Look |
| Right trigger | Fire |
| Left trigger | Aim mode |
| RB | Alt fire / zoom |
| A | Use (doors, buttons) |
| B / left stick click | Crouch |
| X | Reload |
| Y / LB | Next / previous weapon |
| Right stick click | Jump (when the match allows jumping) |
| Back | Nothing (the leaderboard is on START) |
| Start | Join / leaderboard and menu |

**The game's own menus with a controller**: player 1's controller (any kind: Xbox, PlayStation,
Switch Pro...) drives the main menu, the pause menu and their dialogs like a console: D-pad or left
stick to move (held: repeats), A / Cross to choose, B / Circle to go back or close a dialog. The
mouse keeps working alongside it; player 1's right stick moves the cursor and the right trigger is
the left mouse button. `ges_ss_menu_hide_mouse 1` hides the cursor and ignores the mouse on the main
menu and in the Split Screen menu while a controller is connected (dialogs such as Options keep
it). `ges_ss_menu_pad_nav 0` turns the controller menu keys off.

**Stick dead zones**: each controller's sticks are measured automatically
(`ges_ss_pad_deadzone_auto 1`). While a stick rests in the menus (nothing pressed), its resting point
and how far it wanders are measured over a second and a half; the dead zone is that wander plus a
margin (0.06 to 0.35), plus `ges_ss_pad_deadzone_extra` (0.01, so 0.07 to 0.36), around the resting
point. The first measurement after a controller is plugged
in replaces the saved one; later ones only make it wider, so a stick that starts drifting doesn't
creep. The furthest a stick has gone from its resting point counts as 100%, so worn sticks still
reach full speed, and movement starts smoothly from the dead zone's edge. Results are saved per
controller (SDL's GUID plus its serial number when SDL can read one, else plus its name; two
identical controllers without serials share one entry) in `cfg/ges_splitscreen_pads.txt`. In the
Split Screen menu each controller's card in the Players list says "Press R3 to calibrate analog
sticks": R3 on that controller recalibrates only it (leave the sticks centred for about 1.5 s, then
turn both all the way round; the card shows each step, then the dead zones). The console has
`ges_ss_pad_recalibrate [controller]` (all, or one) and `ges_ss_pad_calibration` (prints them). With `ges_ss_pad_deadzone_auto 0`
every stick uses the fixed `ges_ss_pad_deadzone`, as do sticks not measured yet.

## Second screen (players 5 to 8)

The game has one window, so "another screen" means stretching that window across two
monitors side by side. On Windows, `ges_ss_span_monitors` does this: it makes the window
borderless and the size of both monitors. You can also launch with
`-windowed -noborder -w <total width> -h <height>`.

With `ges_ss_dualscreen 2` (the default), a window wider than 2.6:1 is treated as two
screens: the first "players per display" players go on the left half and the rest on the right.

## Console variables

| Cvar | Default | Meaning |
|---|---|---|
| `ges_ss_enable` | 1 | Allow local players to join |
| `ges_ss_host_pad` | 1 | The first controller to press a button is player 1 (0: player 1 uses keyboard/mouse only) |
| `ges_ss_dualscreen` | 2 | 0 never, 1 always, 2 auto-detect two monitors |
| `ges_ss_look_yaw` / `ges_ss_look_pitch` | 200 / 140 | Look speed in degrees per second, before profile sensitivity |
| `ges_ss_look_aim_scale` | 0.5 | Look speed multiplier in aim mode |
| `ges_ss_padmouse` / `ges_ss_padmouse_speed` | 1 / 1100 | Mouse cursor from player 1's controller |
| `ges_ss_pad_deadzone_auto` | 1 | Measure each controller's stick dead zones (see Stick dead zones) |
| `ges_ss_pad_deadzone_extra` | 0.01 | Added to each measured stick dead zone (so a character standing still doesn't creep) |
| `ge_ss_aimassist` | 0 | Aim assist for controller players (set by the Match page): 0 off, 1 classic, 2 sticky, 3 slowdown |
| `ges_ss_aim_sticky` / `ges_ss_aim_slowdown` / `ges_ss_aim_radius` | 0.85 / 0.4 / 1.5 | Sticky aim strength, slowdown speed, degrees around a target that count as on it |
| `ges_ss_pad_deadzone` | 0.22 | Fixed stick dead zone (when not measured, or with auto off) |
| `ges_ss_menu_pad_nav` / `ges_ss_menu_hide_mouse` | 1 / 0 | Controller drives the game's menus / hide the mouse there |
| `ges_ss_pad_backend` | auto | `sdl` or `xinput` |
| `ge_wincondition` | 0 | 0 score, 1 kills, 2 kill/death ratio |
| `ge_killlimit` | 0 | Kills that end the match with `ge_wincondition 1` (0 = none) |
| `sv_ges_splitscreen` | 1 | Server allows split screen guests |
| `sv_ges_splitscreen_maxguests` | 7 | Guests per client |

`ges_ss_status` prints the players, controllers and layout.

## Requirements and limitations

- **Controllers**: the engine only exposes one joystick, so the mod reads controllers itself.
  SDL2 is loaded at runtime for up to 8 controllers. Linux and macOS ship it with the engine.
  On Windows, put `SDL2.dll` in the game or `gesource/bin` folder; without it, XInput is
  used, which supports 4 controllers.
- **Server slots**: guests use player slots, so `maxplayers` must leave room for them.
- **Sound** is heard from player 1's position (the engine has one listener).
- **Screen effects** that the engine applies to the whole screen (fades, flashes) are not per
  viewport. GE's death blood is (it's each player's second viewmodel): it runs down the dying
  player's own view, drawn in front of everything and filling wide views. A faint thin line can
  show along the middle of the blood over very bright backgrounds (the edge of the blood texture).
- **Guests see their own movement through normal entity interpolation** (`cl_interp`, about
  100 ms). Looking around is instant because view angles are kept on the client.
- **Performance**: each extra player renders the whole scene again.
- **Mirrors**: while the screen is split, mirrors and reflective glass don't reflect. Their reflection
  pass (`IsReflectiveGlassInView` in `game/client/viewrender.cpp`) cut away the world behind the
  mirror in the views, leaving black polygons, so it's skipped in split screen.
- **Areaportals**: entities in areas only a guest can see rely on the engine including areas
  added with `AddOriginToPVS` when it checks what to send. Crates and stairs near guests now show
  in their views (distance fades are worked out per view).
- **Map changes**: the engine drops fake clients (the guests) when the map changes, for example
  the next map after a match. Each guest's join request creates them again on the new map.
- **Glass and monitor materials**: GoldenEye's Shader Editor draws glass, monitor screens and
  some walls (`editor_shader` materials). Its 2016 DLLs don't load with the current engine; this
  build ships both rebuilt from source (see [shadereditor/README.md](shadereditor/README.md)). If
  `bin/shadereditor.dll` still fails to load, the client gives those materials standard shaders
  (glass see-through, no reflection, monitors still) instead of crashing.

## Changes (13 November)

- **HUD**: the red/blue bars were replaced by GoldenEye's own health (left) and armour (right) dials
  around each player's crosshair, sized for their view, shown on a hit and fading like the normal game.
- **Death**: the blood runs down each dying player's whole view (it was frozen as a strip at the
  top, also in normal games), in front of everything, and light beams and glows don't shine through.
  The death music plays for guests too.
- **Guns**: guests' first-person guns keep animating when they stand still, and their bob uses a
  smoothed speed (it jittered). Player 1's own weapon no longer fires stray muzzle flashes when
  switching. Tracers and first-person muzzle flashes show from the shooter's own gun: first person
  in their view, the gun in their hand in everyone else's.
- **Aim**: the aim-mode crosshair only shows in the aiming player's view.
- **Controls**: player 1's right stick turns at the same speed as the guests' (it was applied once
  per server tick). Only player 1's controller drives the Split Screen menu.
- **Menu**: the "Local players" text on the Profiles page was replaced by a list in the top right of
  the screen (gamepad with the player number, profile, character, state), shown on both pages.
- **Scoreboard and match report**: guests are no longer shown as bots.
- **Server**: grenade, rocket and mine explosions use GoldenEye's `ge_explosion` (`env_explosion`
  isn't in this build and crashed the server). Testing commands: `ges_ss_debug_kill <player> [damage]`,
  `ges_ss_debug_switch <player>`, client `ges_ss_debug_joined <n>`.

## Changes (14 November)

- **Guests' guns stay put**: a guest's first person gun is now placed with player 1's, before
  entities simulate (`CViewRender::SetUpSplitScreenViews`). It used to be placed only when the
  guest's view was drawn; anything that read the gun's attachments first that frame (firing,
  muzzle flashes, particles) set up its bones at its old position, often the map origin, so the
  gun jumped, showed at the edge of the view turned the wrong way, or vanished. Each guest's
  camera is worked out once per frame, so the gun and the view always agree.
- **Guests' guns keep their animation when firing**: the client played other players' third
  person firing gestures on their weapon, which for a guest reached the guest's first person
  gun: it got the wrong sequence (often its draw, so it dropped out of sight) or another
  weapon's model, every shot. Our guests are skipped there (`hl2mp_playeranimstate.cpp`).
- **Muzzle flashes**: effects created while firing use this frame's camera for the guest, and
  the flash's light is moved back onto the guest's gun model.
- Testing: `ges_ss_debug_vm <player>` logs where that player's gun is relative to their view
  every frame (`before` / `placed` / `bones` / `draw`, and `STALE` if drawn with bones set up
  elsewhere), `ges_ss_debug_look <player> <yaw/s> <pitch/s> [seconds]` turns a guest without a
  controller, `ges_ss_vm_early 0` goes back to the old placement for comparison, and the server's
  `ges_ss_debug_vmanim <entity index>` logs the gun animations it sends.

## Changes (14 November, afternoon)

- **Scoreboard**: the spare cell and the leaderboard page of the START menu show GoldenEye's own
  scoreboard (`CGEScoreBoard`): its green background, Adventure Subtitles and Tahoma fonts, colours,
  "Players (n)" / team sections, Identity, Score, Deaths and Ping columns, dead and developer icons,
  server name, map, time left and game mode, laid out from `resource/ui/ScoreBoard.res` and scaled to
  fit the view. The player's own row is highlighted on their START menu, and their health and
  armour dials stay up while it's open (as holding TAB does), fading out after. The vgui panel
  itself can't be used: it's one full-screen panel and its fonts don't scale. Suicides and the
  kill goal aren't on it any more (the online scoreboard doesn't show them).
- **Ammo**: each view shows GoldenEye's own ammo counter (`CGEHudAmmo`): the clip in AmmoNumbers, the
  ammo type's icon, then the ammo left, in the bottom right, sized for the view. Hidden for weapons
  without ammo; the Moonraker shows only its icon.
- **Death view**: a dead player keeps looking through their body's eyes until they respawn. Player
  1 no longer switches to the third person camera when the body falls against something, and a
  guest whose body hasn't arrived yet keeps their view instead of dropping to the floor.
- **Jumping**: a "Jumping allowed" option on the Match page (`ge_allowjump`, saved as `jump`). Like
  the win condition, it stays set after the match.

## Changes (14 November, evening)

- **Stack crash**: Shader Editor materials fall back to standard shaders (see Requirements and
  limitations).
- **Menus with any controller**: PlayStation and Switch Pro pads (and every Xbox pad) now drive the
  main menu, the pause menu and their dialogs: player 1's D-pad / left stick, A / Cross and B /
  Circle are sent to the menus as an Xbox pad's keys. Nothing did before: the engine's own joystick
  input is off while split screen reads the controllers, and it only knew Xbox pads.
- **Mouse**: (later changed: the mouse stays by default, see the next section) hidden and ignored on the main menu and in the Split Screen menu while a controller is
  connected (`ges_ss_menu_hide_mouse`); dialogs keep it.
- Testing: `ges_ss_debug_menubutton up|down|left|right|a|b` sends a menu key; with no argument it
  says which window has the focus and whether the mouse is hidden. `ges_ss_debug_menunav 1` logs
  the menu keys player 1's controller sends.

## Changes (14 November, night)

- **Mouse back in the menus**: `ges_ss_menu_hide_mouse` now defaults to 0. The cursor and mouse
  work on the main menu and in the Split Screen menu with controllers connected, and the controller
  still navigates. Player 1's left stick navigates; the right stick moves the cursor.
- **Automatic stick dead zones** per controller, saved in `cfg/ges_splitscreen_pads.txt`, with a
  Recalibrate button and readout on the Profiles page (see Stick dead zones).

## Changes (15 November)

- **Radar**: a Radar option on the Match page (`ge_allowradar`, saved as `radar`). Each player's
  view shows GoldenEye's own radar (`CGERadar`, the same element as online, with its contacts kept
  per player): its textures, team backgrounds and colours, blips, tokens, above/below icons, campers,
  range and the game mode's radar rules (`ge_radar_range`, `ge_radar_showenemyteam`, forced radar,
  custom icons). It's centred on that player and turns with their view, where `cl_ge_radarpos` puts
  the normal radar (bottom centre by default), sized for the view. Hidden while dead, in
  intermission, or when the radar isn't allowed. Like jumping, `ge_allowradar` stays set after the
  match.
- **Jump animation**: players now visibly jump (the third person `ACT_MP_JUMP`, which the weapons
  turn into GoldenEye's `ACT_GES_JUMP_*`, crouched `ACT_GES_CJUMP_*`). Nothing used to start it: the
  movement code's jump only called an empty `SetAnimation`. It now sends the jump animation event
  from `CGameMovement::CheckJumpButton`, online and in split screen (guests' jumps reach the other
  views), and landing ends it as before.

## Changes (15 November, afternoon)

- **Shader Editor**: glass reflections and moving monitor screens are back (Stack's
  glass, Bunker's map and screens, Silo's and Aztec's screens, Complex's walls). `shadereditor.dll`
  and `game_shader_generic_procshader.dll` are rebuilt from Biohazard90's Source Shader Editor
  source for the current engine (the 2016 ones failed to load), and the client uses the current
  editor interface. The editor's sky mask now gets each view's rectangle, so it works per split
  screen view. Build steps: [shadereditor/README.md](shadereditor/README.md). The standard-shader
  fallback stays for when the editor doesn't load.

## Changes (15 November, evening)

- **Menus with a controller**: one press now does one thing. Each controller press used to reach
  the game's menus twice (as "pressed" and as "typed"), and the two halves were handled by
  different parts of vgui: one D-pad press moved two highlights on the main menu, and one A press
  chose two items (opening Split Screen also opened Create Server). The main and pause menus now
  get only "typed" (B returns to the game from the pause menu). Dialogs get press and release:
  A presses the focused button, B cancels a confirmation box (and closes other dialogs as
  before), and the D-pad moves between the dialog's buttons like Tab / Shift+Tab, so the quit
  confirmation's Cancel can be reached. Dialogs without a name, like the quit confirmation, now
  count as dialogs (the mouse is never hidden over them), and the Split Screen menu no longer comes
  to the front over a confirmation box. Disconnect has no confirmation in this engine.
- **Stick calibration per controller**: the Profiles page's Recalibrate button and readout are
  gone. In the Split Screen menu each controller's card in the Players list now says "Press R3 to
  calibrate analog sticks". R3 on a controller recalibrates only that controller: the card asks to
  leave the sticks centred (measured for about 1.5 s), then to turn both all the way round (up to
  8 s), then shows the dead zones for a few seconds; the result is saved for that controller. The
  automatic measuring while sticks rest in the menus carries on as before. R3 only does this in the
  Split Screen menu outside a match; in a match it is still jump.

## Changes (16 November)

- **Aim assist**: an "Aim assist" choice on the Match page (Off, Classic GoldenEye, Sticky aim,
  Slowdown near target), saved as `aimassist` and applied as `ge_ss_aimassist` (see the Match page).
  Only for players on controllers in split screen games: every guest, and player 1 when the split
  screen says player 1 is on a controller (the client tells the server through `cl_ges_ss_pad`).
  Classic is done by the server: GoldenEye's weapons already ask the player which way to shoot
  (Source's auto-aim, `GetAutoaimVector`, off in multiplayer), and for those players it now points
  at the enemy nearest the crosshair if they're within the cone and in sight. Sticky aim and
  slowdown work on each player's own view, so every viewport gets them. Teammates are never
  targets in team games. `ges_ss_debug_aimtest` (cheats) and `ges_ss_debug_aim` test it.
- **No game pause menu in split screen matches**: the game's own pause menu (Resume Game,
  Disconnect, Find Servers...) covered every view. It could open from Esc, from a controller the
  engine reads itself (an Xbox pad's START / BACK, or a pad Steam Input presents as one, also with
  `-gamepadui`), or the window regaining focus. While a split screen match is on and player 1 has a
  controller, it's closed as soon as it appears and player 1's START menu opens instead
  (leaderboard, team, profile, exit with a confirmation; player 1 exiting ends the match and shows
  the results). The console (~) still opens. Back at the main menu the menus work as usual.
  `ges_ss_block_pausemenu 0` turns this off.
- **START menu leaderboard**: no "Leaderboard" title, arrows or page dots any more (they sat on the
  round timer; the help line still says Left / Right: more). The other pages keep their titles. The
  scoreboard is centred in the view between the health and armour dials, sized by its visible green
  screen (it used to be fitted by its whole texture, which has empty space above and below), so
  it's much bigger, and the radar is hidden while it's open. Its rows use a larger font in small
  views. The spare-cell scoreboard is fitted the same way, so it's bigger too.
- **Readable START menu scoreboard**: the leaderboard now takes as much of the player's view as
  it can between the round timer and the help line, drawn over the health and armour dials. The
  player rows use a much bigger font (at least a 21st of the board's height: about 22 pixels in a
  quarter of a 1080p screen), the column titles nearly as big, with the rows spaced to fit. The
  Ping column is gone (everyone is on this PC); Name, Identity, Score and Deaths share its room.
  The spare-cell scoreboard uses the same sizes.
- **Fire animation played twice** (seen on player 1's model in the other views, with the PP7 and
  other pistols, the slappers, the throwing knives...): the current engine doesn't register the
  client's `cl_predict`, so the server read it as 0 and treated every player as not predicting.
  It then sent each player the animation events (fire, reload, jump...) their own game had already
  predicted, so they played twice, and it skipped lag compensation for everyone's shots (online
  too). The server now takes a setting that isn't sent as its default (on). Testing:
  `sv_ges_debug_animevent` / `cl_ges_debug_animevent <player>` log the events,
  `ges_debug_clientsettings 1` prints what each player's game sent.
- **Dead zones +0.01**: every measured stick dead zone (automatic or R3 calibration) gets 0.01
  more (`ges_ss_pad_deadzone_extra`), so a character standing still doesn't creep: 0.07 at the
  least. It's part of the saved value, so loading or measuring again doesn't add it twice.
- **Cheats page and Big Head**: a **Cheats...** button on the Match page opens a Cheats page of
  on / off toggles (built from a table in `ge_splitscreen_match.cpp`, so more are easy to add). The
  first is **Big Head** (`ge_cheat_bighead`, replicated): the head bone of every character is drawn
  at twice its size (`C_BaseAnimating::BuildTransformations`), players and ragdolls, in every view;
  hats copy the head bone, so they grow too; first person guns are left alone. Hitboxes don't change
  (like the N64 game, it's only for looks). Like jump and radar, it stays set after the match: set
  `ge_cheat_bighead 0` before hosting online.
