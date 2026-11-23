# Slime Volleyball

A local two-player volleyball game in C and SDL2. Player 1 moves with **A/D**
and jumps with **W**. Player 2 uses **Left/Right** and **Up**.

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
