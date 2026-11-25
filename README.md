# Evo Tournament

A fast arena deathmatch shooter built on the [evogine](https://github.com/Dye-Entertainment/evogine)
engine (included as a git submodule). It's an original game in the classic arena-shooter style:
double jumps, dodges, wall runs, jump pads, power-up control and bots that hunt you through three
multi-level arenas. It runs natively on Linux and in the browser.

## Play

- **Browser:** open the GitHub Pages site for this repository. Pick an arena, then click
  **Instant Action** to play against bots, or **Host Online Match** to play with friends.
- **Linux:** `make run` (see *Building*).

### Controls

| Action | Keys |
| --- | --- |
| Move | WASD / arrow keys |
| Aim / fire | Mouse / left click |
| Jump, double jump | Space (press again in the air) |
| Dodge | Double-tap a direction, or Shift |
| Wall run | Jump alongside a wall while holding forward; Space kicks off it |
| Weapons | 1–6, mouse wheel, Q for the last weapon |
| Throw grenade | G or right click |
| Place proximity mine | F |
| Scoreboard | Tab |
| Menu | Esc (in the browser, Esc releases the mouse and opens the menu) |

On phones and tablets the browser version shows touch controls: drag on the left side to move
(flick the stick twice for a dodge), swipe on the right side to aim, and use the FIRE, JUMP,
DODGE, GREN, MINE, weapon 1–6, SCORES and pause buttons. Tap **PLAY** (or the arena) to start; aim speed follows
the *Mouse sensitivity* option. Joining by typing a code needs a keyboard, but invite links
(`?join=CODE`) work. Add `?touch=1` or `?touch=0` to the address to force the touch controls on
or off.

### Weapons and pickups

- **Pulse Rifle**: rapid hitscan rifle. You spawn with it.
- **Nova Launcher**: slow plasma orbs with splash damage and knockback. Useful for
  rocket-jumping (you take reduced self-damage).
- **Rail Lance**: a slow-firing, hard-hitting beam. Headshots deal 1.75× damage.
- **Scattergun**: twelve hitscan pellets per shot. Devastating up close, useless at range.
  A point-blank kill blows the target apart.
- **Frag Launcher**: lobs bouncing shells that burst on a direct hit or after two seconds.
  Good for clearing corners and hitting players below you.
- **Ripper**: fast saw discs that ricochet off walls up to three times. A disc that has
  bounced can hit you too.
- **Grenades** (G or right click): you spawn with two and can carry four. They bounce, roll
  and burst after about 2.4 seconds, blinking faster as the fuse runs down.
- **Proximity mines** (F): you spawn with one and can carry three. A mine sticks to whatever
  it lands on, arms after a second, and goes off when an enemy comes within two metres. Your
  own mines show a green light, everyone else's red. Each player can have three mines out at
  once; placing a fourth removes the oldest. Explosions set off nearby grenades and mines.
- Health, Mega Health, Armor and ammo packs respawn on timers. Controlling them is half the
  game.

### Hazards

Every arena has environmental hazards. If someone knocked you into one in the last four
seconds, they get the frag; otherwise it counts as a suicide.

- **Lava pits** are sunk into the floor. They burn fast, and you have to jump to climb out.
- **Spike traps** are floor plates that fire every few seconds. The plate glows red and the
  tips rattle just before the spikes shoot up.
- **The Maw** lurks in a stone well. Its jaws open wider as you get close; step in front of
  it and it gapes, growls and snaps shut half a second later, eating anyone in reach.

### Arenas

Pick one in the main menu; the menu backdrop shows it. Online, the host's choice applies.
All three were enlarged by a third horizontally (heights are unchanged, so every jump still
works).

- **Bastion**: a sunset fortress. A central keep holds the Rail Lance, the Mega Health sits
  on the bridge above it, and jump pads throw you up to the north and south decks.
- **Foundry**: a closed forge hall with molten channels. The Mega Health is on the raised
  crucible in the middle; a catwalk ring at 5 m (stairs east and west, jump pads north and
  south) holds the Rail Lance and Armor.
- **Frostspire**: an open snowfield at night under an aurora. Two ice towers are joined by a
  narrow sky bridge with the Rail Lance in the middle; the Mega Health is on the ground
  below it.

### Blood and gore

Hits spray blood that splatters floors and walls, and kills burst players into chunks,
limbs and armor plates (headshots take the head off). Turn it off with **Blood and gore**
under *Options*; hits then throw sparks and kills only scatter armor.

## Multiplayer

The host is authoritative:

- The host simulates combat, pickups, projectiles and bots.
- Each client moves its own player locally for responsive controls and sends its state to
  the host at 30 Hz.
- Fire requests are validated by the host (rate of fire, ammo, origin distance).
- The host sends snapshots at 20 Hz. Game events (kills, shots, explosions, pickups) are
  repeated in every snapshot for one second and de-duplicated by sequence number, so
  unreliable transports don't lose them.
- Up to 12 players. Bots fill the empty slots, and a bot leaves when a human joins a
  full match.

There are two transports with the same game protocol:

- **Browser:** WebRTC data channels via [PeerJS](https://peerjs.com). The host's browser
  registers a short room code on the public PeerJS broker. Friends join with the code or
  the invite link (`?join=CODE`), shown in the Esc menu while hosting. The broker is only
  used to set up the connection; game traffic goes peer-to-peer. A few strict NATs block
  direct WebRTC; that would need a TURN server, which isn't configured.
- **Native:** plain UDP. The default port is 27777; friends join with `your-ip:27777`.

The browser and native versions can't play together; they use different transports.

## Building

Requirements: clang, CMake ≥ 3.28, Ninja. For the web build, also
[emsdk](https://emscripten.org/docs/getting_started/downloads.html) (in `~/emsdk` or `$EMSDK`).

```sh
git clone --recursive git@github.com:Dye-Entertainment/evo-tournament.git
cd evo-tournament
make evogen     # once: builds the engine's component code generator
make linux      # native build -> build-linux/Release/evo-tournament
make test       # headless bot simulation + UDP loopback multiplayer test
make web        # emscripten build -> build-web/
make pages      # web build packaged into ./site for GitHub Pages
```

To try the web build locally, serve `site/` (for example `python3 -m http.server -d site`)
and open it in a browser.

`make test` simulates eight bots for five minutes on every arena and fails if bots get stuck,
end up inside geometry or never reach the high ground.

For screenshots, `EVO_PREVIEW="x y z yaw pitch"` hides the main menu and holds the backdrop
camera at that spot (yaw 0 looks along +z, positive pitch looks up).

Engine fixes that this game needs live in `patches/`. CMake applies them to the submodule
automatically; see `patches/README.md`.

## Graphics

Everything is drawn with one shader (`resources/shaders/arena.fs`). When a level loads, its
boxes are cut into 1.5 m cells and each vertex gets baked ambient occlusion, soft sun
shadows and the light of the level's colored lamps (with shadows); this takes well under a
tenth of a second, even in the browser. On top of that the shader adds sun and sky light,
specular highlights, up to 12 point lights per frame (muzzle flashes, explosions, rail
beams, orbs, nearby lamps), procedural panels and grime, height fog and ACES tone mapping.
The sky (sun, clouds, stars, aurora) is procedural too. Each arena's look is an
`ArenaTheme` in `src/game/arena.cpp` / `arena_levels.cpp`.

## UI

The menus and the HUD use the engine's RmlUi backend. `resources/ui/evo.rml` and `evo.rcss`
describe them; the game fills in a data model every frame and the form controls (name, sliders,
room code) write straight back into it. The UI scales down on small windows such as phones.

## Layout

```
src/game/    match rules, movement, bots, arenas (arena_levels.cpp), game scene; game_scene_ui.cpp fills the UI
src/ui/      the RmlUi data model behind the menus and the HUD
src/net/     transports (UDP, WebRTC bridge) and the host/client session
src/render/  procedural meshes, light baking, renderer wrapper, particles, blood and gore
src/audio/   sound playback on the engine's miniaudio backend
web/         HTML shell and the PeerJS bridge (emscripten JS library)
resources/   shaders, synthesized sounds, font, UI documents (ui/evo.rml, evo.rcss),
             engine component templates
tools/       generate_sounds.py (the sound effects are synthesized by this script)
```

All art and sound is procedural or synthesized for this project. The UI font is DejaVu Sans
(see `resources/fonts/LICENSE-DejaVu.txt`).
