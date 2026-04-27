# Steam Media Center - MVP Plan

## Goal

Turn a Steam-powered PC, Steam Deck, or future Steam Machine-style device into a controller-first media hub for web-based content.

Primary focus:
- UX
- Input
- Performance

Explicitly not the goal:
- A full smart TV replacement
- Guaranteed compatibility with every streaming service
- DRM workarounds or site-specific gray-market integrations

---

## MVP Scope (v1)

### 1. Controller-First UI Shell

Requirements:
- Fullscreen 10-foot interface
- Controller-first navigation
- Keyboard support
- Optional mouse support
- Smooth, low-latency transitions

Core screens:
- Home: recent and favorite services
- Services: list of supported platforms
- Settings: minimal app and input preferences

Design principle:

> Feels like Steam Big Picture, not a browser.

---

### 2. Embedded Browser

Technology:
- Chromium Embedded Framework (CEF)

Features:
- Launch predefined services
- Always fullscreen
- Persistent sessions for cookies and login state
- No visible browser chrome: no URL bar, tabs, or browser-style controls

Constraints:
- Do not make DRM compatibility part of the core product promise.
- Do not build fragile service-specific hacks into the core browser layer.

---

### 3. Input Abstraction Layer

Primary input:
- Steam Input

Default behavior:
- Left stick: shell navigation
- Right stick: cursor movement
- Trigger: click
- Shoulder buttons: scroll

Optional if easy:
- Gyro pointer support on Steam Deck

Goal:

> Make web interfaces usable from the couch.

---

### 4. Basic Ad Blocking

MVP approach:
- Integrate Chromium-compatible filter lists
- Provide lightweight request blocking
- Keep behavior conservative and user-configurable

Not in MVP:
- Pi-hole integration
- Aggressive anti-adblock bypass work
- Service-specific breakage fixes as a core promise

---

### 5. Service Launcher System

Initial services:
- YouTube
- Twitch
- Netflix
- Prime Video

Config example:

```json
{
  "name": "YouTube",
  "url": "https://youtube.com",
  "input_mode": "mouse"
}
```

MVP behavior:
- Services may be hardcoded initially.
- JSON service definitions should be supported or easy to add shortly after.
- Per-service configuration should avoid gray-market integrations.

---

### 6. Fast App Switching

Requirements:
- Keep browser instances warm where practical.
- Avoid unnecessary reloads when switching services.
- Make transitions feel instant.

Goal:

> Faster than typical smart TV app switching.

---

## Post-MVP Scope (v1.5+)

### Plugin System

Approach:
- JSON-based service definitions
- Optional lightweight JS hooks
- Clear separation between official core features and community extensions

Purpose:
- Allow community customization
- Avoid maintaining fragile or legally gray integrations in the official repo

---

### Optional CSS / JS Injection

Features:
- Per-service customization
- Disabled by default
- Explicitly user-controlled

Use cases:
- Hide UI clutter
- Improve controller navigation

Warning:
- Not part of the core product stability promise.

---

### Steam Integration

MVP:
- Run as a standalone Steam app.

Later:
- Explore Steam overlay integration where technically feasible.
- Keep expectations conservative because overlay control is limited.

---

### HDMI-CEC Support

Not MVP.

Potential features:
- Power TV on/off
- Navigate via TV remote

Value:
- Useful quality-of-life feature, but not essential to proving the product.

---

## UX Principles

### Instant Responsiveness

- Avoid loading screens where possible.
- Prefer smooth transitions.
- Preserve service state during switching.

### Unified Input Model

- Controls should behave consistently across shell screens and web playback.
- Users should not need to understand whether they are controlling the shell or a browser.

### Hide Browser Complexity

- No tabs
- No address bar
- No technical browser UI

---

## Monetization

Model:
- One-time purchase
- Target price range: EUR 4.99-9.99

Possible future options:
- Paid upgrades
- Advanced features
- Plugin-related extras

---

## Risks to Avoid

- Trying to fully support every streaming service.
- Promising DRM compatibility.
- Overengineering the plugin system too early.
- Building the product around fragile JS hacks.
- Overcomplicating the UI.

---

## Core Differentiator

The value is:

> Controller-first UX + input abstraction + PC performance.

The value is not:
- Owning streaming services
- Browser technology by itself
- Ad blocking alone

---

## Suggested Tech Stack

- Language: C++ or Rust
- UI: Custom UI, SDL, or ImGui
- Browser: CEF
- Input: Steam Input
- Distribution: Steam

---

## First Prototype (Week 1-2)

Build:
- Launch fullscreen app.
- Implement controller navigation.
- Open YouTube in an embedded browser.
- Add controller-to-mouse mapping.

Success criteria:
- Feels smooth.
- Usable from the couch.
- No major interaction friction.

---

## Summary

This is not a mass-market smart TV replacement.

It is a fast, controller-native media layer for Steam devices.

If the UX feels great early, continue. If it feels clunky, pivot quickly.
