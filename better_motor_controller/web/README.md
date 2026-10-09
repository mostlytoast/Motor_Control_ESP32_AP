# WebAssembly UI for the train controller

This folder adds a browser-hosted replacement for the old static HTML/JS website in `WebPage.ino`.
Instead of shipping a custom web page for the ESP32, the same LVGL controller logic is compiled to WebAssembly with Emscripten and runs in the browser.

## Why this matches the project

- `lvdisplay_portable.h` and `lvdisplay_portable.cpp` are already written to be UI-framework agnostic.
- `LvDisplayController` exposes the same state model used by the hardware display.
- The WebAssembly host only needs to provide a browser-side `ILvDisplayHost` implementation and a LVGL window.

## Build steps

1. Install Emscripten:
   - https://emscripten.org/docs/getting_started/download.html
2. From the project root, run:

```bash
./web/build_emscripten.sh
```

3. Serve the generated page locally:

```bash
cd web/dist
python3 -m http.server 8000
```

4. Open `http://localhost:8000`

## Browser integration hook

The browser host uses a window callback named `window.__trainUiCommandHandler(command, value)`. If you want to bind it to a REST API or websocket layer, replace the default console logger in `web/emscripten_ui.cpp`.

Example:

```js
window.__trainUiCommandHandler = function (command, value) {
  fetch('/api/train', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ command, value }),
  });
};
```

## Notes

- This is intended as the browser-side replacement for the old ESP32 website experience.
- The Arduino firmware can still expose the same commands over ESP-NOW / WiFi, while the browser UI is served from the compiled WebAssembly app.
- `WebPage.ino` remains available as the fallback for the direct ESP32 web UI, but this WebAssembly build is the path that keeps the controller UI shared between native and browser targets.
