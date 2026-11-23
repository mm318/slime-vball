# Slime Volleyball

A volleyball game in C and SDL2 that starts in solo mode. Win a rally
by landing the ball on the other side. The rally winner serves next, and the first
player to six points wins the match. The ball drops automatically over the server.

| Input | Action |
| --- | --- |
| **Left / Right**, **Up** | Move and jump as player 1 on the left |
| **A / D**, **W** | Move and jump as player 2 on the right in local play |
| **1** / **2** | Start a solo or local match. Winning solo matches raises the CPU through three difficulty tiers. |
| **Space** | Replay up to the last four seconds of a rally during the point break, or skip playback |
| **P** / **R** | Pause or resume, or restart the current match |

![Slime Volleyball screenshot](https://raw.githubusercontent.com/parsiad/slime-vball/gh-pages/slime-vball.png)

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
