# Slime Volleyball

A volleyball game in C and SDL2 with the colors and prompts from One Slime.
Click a menu button to start a solo or local match, or press **1** or **2**. Win a rally
by landing the ball on the other side. The rally winner serves next, and the first
player to six points wins the match. The ball drops automatically over the server.

| Input | Action |
| --- | --- |
| **Left / Right**, **Up** | Move and jump as player 1 on the left |
| **A / D**, **W** | Move and jump as player 2 on the right in local play |
| **1** / **2** | Start a new solo or local match from the first stage |
| **Space** | Request a replay during the point break; press again during playback to continue |
| **P** / **R** (or **K**) | Pause or resume, or restart the current match |
| **Mouse click** | Choose a mode, or continue from the match result |
| **C** | Continue the current solo stage from the menu |

The five stages reproduce the original court, ball, and opponent colors, with
rally commentary, score and time, level bonuses, and victory and loss screens.
The existing bitmap font and floating point physics are retained. Stages four
and five reuse the strongest existing CPU controller.

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

The executable is `zig-out/bin/slime-vball` (`.exe` on Windows). For a release
build, add `-Doptimize=fast`.

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

See [COPYING](COPYING) for the license.
