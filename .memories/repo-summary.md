# Steam TV Repo Context

Current repo state:
- Documentation-only prototype repo.
- Project pitch lives in `doc/pitch.md`.
- MVP plan lives in `doc/MVP.md`.
- No `.git` repository metadata is present yet.

Product direction:
- Paid Steam app for Steam-powered PCs, Steam Deck, and future Steam Machine-style devices.
- Controller-first smart TV/media hub shell with embedded Chromium/CEF web playback.
- Core differentiator is 10-foot UX plus input abstraction, not owning streaming content or promising full DRM support.

MVP priorities:
- Fullscreen controller-first shell.
- Service launcher for YouTube, Twitch, Netflix, and Prime Video.
- Embedded Chromium/CEF browser with persistent cookies/login.
- Controller-to-mouse pointer mode.
- Optional service JSON config and disabled-by-default CSS/JS hooks.
- Avoid gray-market plugins and keep DRM/site-specific fixes outside the core promise.

Current documentation status:
- `doc/MVP.md` has been cleaned up into valid Markdown with clear MVP, post-MVP, UX, monetization, risks, stack, and prototype sections.

Implementation direction:
- Favor a low-level/game-style architecture with the app owning the main window, event loop, input model, and render system.
- Use CEF inside the app window via off-screen rendering so browser output can become an OpenGL texture.
- Keep CEF as a subsystem that produces pixels and receives input events, not as the top-level UI framework.
- Preferred rendering/window/input layer for the baseline is raylib 6, with the project using raylib's window, OpenGL-backed drawing, gamepad input, and texture APIs around a CEF off-screen browser surface.

External dependencies:
- `external/raylib` contains raylib 6.0 downloaded from `https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz`.
- `external/cef` contains CEF `147.0.10+gd58e84d+chromium-147.0.7727.118` Linux x64 standard distribution downloaded from `https://cef-builds.spotifycdn.com/cef_binary_147.0.10%2Bgd58e84d%2Bchromium-147.0.7727.118_linux64.tar.bz2`.

Coding style:
- Follow `doc/Orthodoxcpp.md`: simple C-like C++, no exceptions, no RTTI, no iostreams/stringstreams, prefer C runtime headers/functions such as `stdio.h`, and avoid STL containers that allocate unless there is a deliberate reason.
- Baseline implementation should use explicit ownership/lifetime, plain structs, clear init/shutdown paths, and return-code style error handling where practical.
