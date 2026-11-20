# Evo Tournament

A fast arena deathmatch shooter built on the [evogine](https://github.com/Dye-Entertainment/evogine)
engine (included as a git submodule). It's an original game in the classic arena-shooter style:
double jumps, dodges, jump pads, power-up control and bots that hunt you through a
multi-level arena. It runs natively on Linux and in the browser.

## Play

- **Browser:** open the GitHub Pages site for this repository. Click **Instant Action** to
  play against bots, or **Host Online Match** to play with friends.
- **Linux:** `make run` (see *Building*).

### Controls

| Action | Keys |
| --- | --- |
| Move | WASD / arrow keys |
| Aim / fire | Mouse / left click |
| Jump, double jump | Space (press again in the air) |
| Dodge | Double-tap a direction, or Shift |
| Weapons | 1 2 3, mouse wheel, Q for the last weapon |
| Scoreboard | Tab |
| Menu | Esc (in the browser, Esc releases the mouse and opens the menu) |

### Weapons and pickups

- **Pulse Rifle**: rapid hitscan rifle. You spawn with it.
- **Nova Launcher**: slow plasma orbs with splash damage and knockback. Useful for
  rocket-jumping (you take reduced self-damage).
- **Rail Lance**: a slow-firing, hard-hitting beam. Headshots deal 1.75× damage.
- Health, Mega Health (on the north balcony), Armor (south balcony) and ammo packs
  respawn on timers. Controlling them is half the game.

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

Engine fixes that this game needs live in `patches/`. CMake applies them to the submodule
automatically; see `patches/README.md`.

## Layout

```
src/game/    match rules, movement, bots, arena, game scene, HUD and menus
src/net/     transports (UDP, WebRTC bridge) and the host/client session
src/render/  procedural meshes, renderer wrapper, particle effects
src/audio/   sound playback on the engine's miniaudio backend
web/         HTML shell and the PeerJS bridge (emscripten JS library)
resources/   shaders, synthesized sounds, font, engine component templates
tools/       generate_sounds.py (the sound effects are synthesized by this script)
```

All art and sound is procedural or synthesized for this project. The UI font is DejaVu Sans
(see `resources/fonts/LICENSE-DejaVu.txt`).
