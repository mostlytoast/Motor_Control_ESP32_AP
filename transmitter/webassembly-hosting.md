# WebAssembly hosting notes for the LVGL controller UI

The hardware-specific code in `display.ino` and the TFT/touch driver logic are not portable.
The UI itself has been separated into a portable controller class in `lvdisplay_portable.h` and `lvdisplay_portable.cpp` so it can be hosted independently.

## What is portable

- UI state model (`LvDisplayState`)
- all button actions and status labels
- the LVGL widget layout and update logic

## What stays device-specific

- physical TFT flush callback
- XPT2046 touch mapping
- ESP-NOW or radio protocol calls
- hardware initialization and board layout

## Integration pattern for a website / WebAssembly host

1. Create a browser-side `ILvDisplayHost` implementation.
2. Implement `sendCommand(const char* command, int value)` so it sends messages to your JS bridge or websocket.
3. Instantiate `LvDisplayController controller(&host);`
4. Call `controller.init(lv_screen_active());` after LVGL initialization.
5. Drive `controller.refresh()` each LVGL tick.

Example:

```cpp
class BrowserHost : public ILvDisplayHost {
public:
  void sendCommand(const char* command, int value) override {
    // forward to JavaScript/WebSocket/backend
  }
};

BrowserHost host;
LvDisplayController controller(&host);

void setupUi() {
  lv_init();
  controller.init(lv_screen_active());
}

void tickUi() {
  controller.refresh();
  lv_timer_handler();
}
```

This pattern makes the UI portable while keeping the actual control protocol implementation separated.

## Recommended deployment

- compile the LVGL UI with Emscripten/wasm
- render into a browser canvas or SDL window
- bridge UI events to the real backend through JavaScript
- keep the radio and controller communication in a backend service, not in the UI runtime

The result is that the same control surface can be reused by a desktop app, web app, or another embedded project without rewriting the button logic.
