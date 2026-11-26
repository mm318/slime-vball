# Slime Volleyball

A volleyball game in Zig, C, and SDL2 with the colors and prompts from One Slime.
Tap or click a menu button to start a solo or local match. Win a rally
by landing the ball on the other side. The rally winner serves next, and the first
player to six points wins the match. The ball drops automatically over the server.

| Input | Action |
| --- | --- |
| **Left / Right**, **Up** | Move and jump as player 1 on the left |
| **A / D**, **W** | Move and jump as player 2 on the right in local play |
| **Touch drag left / right** | Move until the finger lifts; reversing the drag reverses direction |
| **Touch drag up** | Jump, including while moving horizontally |
| **Space** or **R button** | Request replay during a point break, or leave replay; button grey when unavailable |
| **P** or **P button** | Pause or resume |
| **Continue button** | Resume a pause, retry the current stage after a loss, or advance after a stage win |
| **One / two player buttons** | Start a new game from any menu |

Mode selection and Continue respond only to mouse clicks or touch taps.
Continue is disabled on the initial screen. Pausing opens the same three-button
menu; a completed campaign
continues into a new solo run, and a local game over continues into a rematch.
In local touch play, the left and right halves control their respective slimes
with separate fingers. In solo mode, dragging anywhere controls the human.
Keyboard and touch can be mixed; releasing a drag restores any held keyboard
direction. Focus loss, pause, and rally resets cancel active gestures.

The three stages reproduce the original court, ball, and opponent colors, with
rally commentary, score and time, level bonuses, and victory and loss screens.
The existing bitmap font keeps its original width and height, with newlines
for long prompts. The floating point physics are retained.
The three computer players port the original `_t`, `ot`, and `pt` controllers,
including serve state, randomized decisions, and the original predictor quirks.
Their decisions use double-precision court units; collision physics still uses
the existing floating point implementation.

## Desktop

Use **Zig 0.17.1**. SDL **2.32.10** is built from the sources in `deps/sdl2`.
On Ubuntu, install the X11 and OpenGL headers:

```sh
sudo apt install libxext-dev libgles-dev libglx-dev
```

Build and play:

```sh
zig build
zig build run
zig build test
```

The tests compare 24,576 AI decisions with fingerprints generated from the
pinned One Slime bundle, covering ordered commands, serve state, and random
call counts. To regenerate those fingerprints from a local bundle:

```sh
node tests/ai_reference.mjs /path/to/OneSlime.min.js
```

The executable is `zig-out/bin/slime-vball` (`.exe` on Windows). For a release
build, add `-Doptimize=fast`.
Desktop and browser builds display the court at 1.5× scale: 1125 × 563 pixels
(height rounded to the nearest whole pixel).

## Browser

```sh
zig build install-emsdk
zig build -Dtarget=wasm32-emscripten -Doptimize=fast run
```

The first command installs the pinned **Emscripten 7.0.0** SDK. Click the court
to focus the keyboard. To serve without launching a browser, append
`-- --no_browser` to the run command.

Serve `index.html`, `index.js`, and `index.wasm` together from `zig-out/web`.
The game draws its graphics in code, so there is no separate asset package.

The source is organized as follows:

- `src/main.zig`: program entry point, SDL initialization, and desktop/browser loops.
- `src/main.c`: replacement physics, SDL drawing, and movement primitives, called by Zig.
- `src/input.zig`: mouse/touch button activation, drag gestures, and keyboard input.
- `src/game.zig`: game phases, progression, scoring, commentary, and pause.
- `src/ai.zig`: computer-player input and interception prediction.
- `src/replay.zig`: position history and replay playback.
- `src/ui.zig` and `src/font.zig`: palettes, bitmap text, menus, results, and HUD.

Shared types and interfaces live in `src/game.h`, `src/app.h`, and `src/ui.h`.
The build translates `src/bridge.h` to Zig bindings, so the C headers define the
shared ABI. The UI checks are implemented in `tests/ui.zig`, exercising the
modules through the same interface used by `main.c`. The checks also retain
optional BMP screenshot output.

See [COPYING](COPYING) for the license.
