I want to prototype a Steam/SteamOS media-center app MVP.

Core idea:
A paid Steam app that turns a Steam-powered PC, Steam Deck, or future Steam Machine into a controller-first smart-TV/media hub. It should use a native shell UI with an embedded browser for web playback, persistent sessions, ad blocking where feasible, and strong input abstraction via Steam Input/controller/mouse/gyro-style pointer support.

MVP priorities:
1. Fullscreen 10-foot UI, controller-first.
2. Service launcher: YouTube, Twitch, Netflix/Prime as browser targets.
3. Embedded Chromium/CEF browser.
4. Controller-to-mouse pointer mode.
5. Persistent cookies/login.
6. Optional per-service config JSON.
7. Optional CSS/JS injection hooks, disabled by default.
8. Keep DRM/site-specific fixes out of the core promise.
9. Avoid gray-market plugins in the official repo, but design a safe plugin boundary later.

Help me create an initial repo structure and a minimal prototype.
