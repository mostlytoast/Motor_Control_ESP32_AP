
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "../shared/LvDisplay/lvdisplay_portable.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// ============================================================
// Emscripten <-> Browser JavaScript bridge
//
// IMPORTANT:
// These EM_JS declarations are deliberately OUTSIDE of any
// C++ class and OUTSIDE of the anonymous namespace.
//
// This prevents EM_JS from being interpreted as a class member.
// ============================================================

#ifdef __EMSCRIPTEN__

// ------------------------------------------------------------
// Send arbitrary JSON from WASM -> ESP32
//
// POST /state
// ------------------------------------------------------------

EM_JS(void, browserSendJson, (const char* json), {
  if (!json) {
    console.error("[LVGL] browserSendJson: null JSON");
    return;
  }

  const body = UTF8ToString(json);

  console.log("[LVGL JSON -> ESP32]", body);

  fetch("/state", {
    method : "POST",

    headers : {"Content-Type" : "application/json"},

    body : body

  })
      .then(function(response) {
        if (!response.ok) {
          throw new Error("HTTP " + response.status);
        }

        return response.text();
      })

      .then(function(responseBody) {
        // console.log(
        //   "[LVGL ESP32 -> JSON]",
        //   responseBody
        // );

        // The ESP32 returns the actual state after applying
        // the requested state. Feed that state back into LVGL.
        if (typeof Module !== "undefined" && typeof Module.ccall === "function") {
          Module.ccall("emscriptenReceiveJson",
                       null, ["string"], [responseBody]);
        }
      })

      .catch(function(error) {
        console.error("[LVGL] JSON request failed:", error);
      });
});

// ------------------------------------------------------------
// Request current ESP32 state
//
// GET /state
// ------------------------------------------------------------

EM_JS(void, browserRequestState, (), {
  fetch("/state", {
    method : "GET",

    headers : {"Accept" : "application/json"}

  })

      .then(function(response) {
        if (!response.ok) {
          throw new Error("HTTP " + response.status);
        }

        return response.text();
      })

      .then(function(json) {
        console.log("[LVGL ESP32 STATE]", json);

        if (typeof Module !== "undefined" && typeof Module.ccall === "function") {
          Module.ccall("emscriptenReceiveJson", null, ["string"], [json]);
        }
      })

      .catch(function(error) {
        console.error("[LVGL] Failed to get /state:", error);
      });
});

// ------------------------------------------------------------
// Start periodic state synchronization
//
// The browser periodically asks the ESP32 for its real state.
// ------------------------------------------------------------

EM_JS(void, browserStartStatePolling, (), {
  if (window.__lvglStatePollingStarted) {
    return;
  }

  window.__lvglStatePollingStarted = true;

  console.log("[LVGL] Starting ESP32 state polling");

  // Initial state immediately.
  browserRequestState();

  // Keep UI synchronized with ESP32.
  window.__lvglStatePollingTimer =
      window.setInterval(function() { browserRequestState(); }, 500);
});

// ============================================================
// Browser pointer state
// ============================================================

EM_JS(void, installBrowserPointerHandlers, (), {
  const canvas = document.getElementById("canvas");

  if (!canvas) {
    console.error("[LVGL] Cannot install pointer handlers: #canvas missing");

    return;
  }

  canvas.style.touchAction = "none";

  if (window.__lvglPointerHandlersInstalled) {
    console.log("[LVGL] Pointer handlers already installed");

    return;
  }

  window.__lvglPointerHandlersInstalled = true;

  const state = window.__lvglPointerState = {

    x : 0,
    y : 0,

    pressed : false,
    active : false,

    pointerId : null
  };

  function updatePosition(e) {
    const rect = canvas.getBoundingClientRect();

    if (rect.width <= 0 || rect.height <= 0) {
      return;
    }

    let x = (e.clientX - rect.left) * (canvas.width / rect.width);

    let y = (e.clientY - rect.top) * (canvas.height / rect.height);

    x = Math.max(0, Math.min(canvas.width - 1, x));

    y = Math.max(0, Math.min(canvas.height - 1, y));

    state.x = Math.round(x);
    state.y = Math.round(y);
  }

  // ----------------------------------------------------------
  // Pointer down
  // ----------------------------------------------------------

  canvas.addEventListener("pointerdown",

                          function(e) {
                            e.preventDefault();

                            updatePosition(e);

                            state.pressed = true;
                            state.active = true;
                            state.pointerId = e.pointerId;

                            // console.log(
                            //   "[LVGL INPUT] DOWN",
                            //   state.x,
                            //   state.y,
                            //   e.pointerType
                            // );

                            try {
                              canvas.setPointerCapture(e.pointerId);

                            } catch (_) {
                            }
                          },

                          {passive : false});

  // ----------------------------------------------------------
  // Pointer move
  // ----------------------------------------------------------

  canvas.addEventListener("pointermove",

                          function(e) {
                            if (e.pointerType === "mouse" || state.active) {
                              updatePosition(e);
                            }

                            if (state.active) {
                              e.preventDefault();
                            }
                          },

                          {passive : false});

  // ----------------------------------------------------------
  // Pointer up
  // ----------------------------------------------------------

  canvas.addEventListener("pointerup",

                          function(e) {
                            e.preventDefault();

                            updatePosition(e);

                            state.pressed = false;
                            state.active = false;
                            state.pointerId = null;

                            // console.log(
                            //   "[LVGL INPUT] UP",
                            //   state.x,
                            //   state.y,
                            //   e.pointerType
                            // );

                            try {
                              canvas.releasePointerCapture(e.pointerId);

                            } catch (_) {
                            }
                          },

                          {passive : false});

  // ----------------------------------------------------------
  // Pointer cancel
  // ----------------------------------------------------------

  canvas.addEventListener("pointercancel",

                          function(e) {
                            e.preventDefault();

                            state.pressed = false;
                            state.active = false;
                            state.pointerId = null;

                            // console.log(
                            //   "[LVGL INPUT] CANCEL"
                            // );
                          },

                          {passive : false});

  // ----------------------------------------------------------
  // Lost pointer capture
  // ----------------------------------------------------------

  canvas.addEventListener(
      "lostpointercapture",

      function() {
        state.pressed = false;
        state.active = false;
        state.pointerId = null;
      });

  console.log("[LVGL] Browser mouse/touch input installed");
});

// ------------------------------------------------------------
// Pointer getters
// ------------------------------------------------------------

EM_JS(int, getBrowserPointerX, (), {
  if (!window.__lvglPointerState) {
    return 0;
  }

  return window.__lvglPointerState.x | 0;
});

EM_JS(int, getBrowserPointerY, (), {
  if (!window.__lvglPointerState) {
    return 0;
  }

  return window.__lvglPointerState.y | 0;
});

EM_JS(int, getBrowserPointerPressed, (), {
  if (!window.__lvglPointerState) {
    return 0;
  }

  return window.__lvglPointerState.pressed ? 1 : 0;
});

#endif  // __EMSCRIPTEN__

// ============================================================
// Global LVGL state
//
// These MUST be outside BrowserHost.
// ============================================================

static uint16_t
    g_framebuffer[LV_DISPLAY_PORTABLE_WIDTH * LV_DISPLAY_PORTABLE_HEIGHT];

static lv_display_t* g_display = nullptr;

static lv_obj_t* g_screen = nullptr;

static std::unique_ptr<LvDisplayController> g_controller = nullptr;

#ifdef __EMSCRIPTEN__

static lv_indev_t* g_pointerIndev = nullptr;

#endif

// ============================================================
// BrowserHost
// ============================================================

class BrowserHost : public ILvDisplayHost {
 public:
  // ----------------------------------------------------------
  // Generic JSON -> ESP32
  // ----------------------------------------------------------

  void sendJson(const char* json) override {
    // notifyLocalStateChanged();

#ifdef __EMSCRIPTEN__

    if (!json) {
      return;
    }

    browserSendJson(json);

#else

    (void)json;

#endif
  }

};

// ============================================================
// LVGL pointer input callback
// ============================================================

#ifdef __EMSCRIPTEN__

static void browserPointerReadCallback(lv_indev_t* indev,
                                       lv_indev_data_t* data) {
  (void)indev;

  if (!data) {
    return;
  }

  static int callbackCount = 0;

  callbackCount++;

  if ((callbackCount % 60) == 0) {
    std::printf(
        "[LVGL INPUT CALLBACK] "
        "alive x=%d y=%d pressed=%d\n",

        getBrowserPointerX(), getBrowserPointerY(), getBrowserPointerPressed());
  }

  data->point.x = (lv_coord_t)getBrowserPointerX();

  data->point.y = (lv_coord_t)getBrowserPointerY();

  data->state = getBrowserPointerPressed() ? LV_INDEV_STATE_PRESSED
                                           : LV_INDEV_STATE_RELEASED;

  data->continue_reading = false;
}

#endif

// ============================================================
// ESP32 -> WASM JSON receive entry point
//
// JavaScript calls:
//
// Module.ccall(
//   "emscriptenReceiveJson",
//   null,
//   ["string"],
//   [json]
// );
//
// The JSON is passed directly to LvDisplayController.
// ============================================================

#ifdef __EMSCRIPTEN__

extern "C" EMSCRIPTEN_KEEPALIVE void emscriptenReceiveJson(const char* json) {
  if (!json) {
    return;
  }

  // std::printf(
  //     "[LVGL JSON <- ESP32] %s\n",
  //     json
  // );

  if (!g_controller) {
    std::printf("[LVGL JSON <- ESP32] ERROR: controller is null\n");

    return;
  }

  g_controller->getStateFromJsonDisplay(json);
}

#endif

// ============================================================
// LVGL framebuffer -> browser canvas
// ============================================================

static void flushDisplay(lv_display_t* display, const lv_area_t* area,
                         uint8_t* px_map) {
#ifdef __EMSCRIPTEN__

  if (display == nullptr) {
    return;
  }

  if (px_map == nullptr) {
    std::printf("[LVGL] ERROR: flush received null framebuffer\n");

    lv_display_flush_ready(display);

    return;
  }

  const int width = LV_DISPLAY_PORTABLE_WIDTH;

  const int height = LV_DISPLAY_PORTABLE_HEIGHT;

  static int flushCount = 0;

  if (flushCount < 10) {
    std::printf(
        "[LVGL] flush #%d "
        "area=(%d,%d)-(%d,%d) "
        "pixels=%p\n",

        flushCount,

        area != nullptr ? area->x1 : -1,

        area != nullptr ? area->y1 : -1,

        area != nullptr ? area->x2 : -1,

        area != nullptr ? area->y2 : -1,

        px_map);

    flushCount++;
  }

  EM_ASM(
      {
        const width = $0;
        const height = $1;
        const pixels = $2;

        const canvas = document.getElementById("canvas");

        if (!canvas) {
          console.error("[LVGL] ERROR: #canvas does not exist");

          return;
        }

        const ctx = canvas.getContext("2d");

        if (!ctx) {
          console.error("[LVGL] ERROR: canvas 2D context unavailable");

          return;
        }

        if (canvas.width !== width || canvas.height !== height) {
          console.log("[LVGL] resizing canvas", width, height);

          canvas.width = width;
          canvas.height = height;
        }

        const image = ctx.createImageData(width, height);

        const source = HEAPU8.subarray(pixels, pixels + width * height * 2);

        for (let i = 0; i < width * height; i++) {
          const lo = source[i * 2];

          const hi = source[i * 2 + 1];

          const rgb565 = lo | (hi << 8);

          const r = (rgb565 >> 11) & 0x1F;

          const g = (rgb565 >> 5) & 0x3F;

          const b = rgb565 & 0x1F;

          const red = (r << 3) | (r >> 2);

          const green = (g << 2) | (g >> 4);

          const blue = (b << 3) | (b >> 2);

          const p = i * 4;

          image.data[p + 0] = red;

          image.data[p + 1] = green;

          image.data[p + 2] = blue;

          image.data[p + 3] = 255;
        }

        ctx.putImageData(image, 0, 0);
      },

      width, height, px_map);

  lv_display_flush_ready(display);

#else

  (void)display;
  (void)area;
  (void)px_map;

#endif
}

// ============================================================
// LVGL main loop
// ============================================================

static void webLoop() {
  lv_tick_inc(16);
  lv_timer_handler();

#ifdef __EMSCRIPTEN__

  if (g_pointerIndev != nullptr) {
    lv_indev_read(g_pointerIndev);
  }

#endif

  lv_timer_handler();
}

// ============================================================
// Main
// ============================================================

int main() {
  std::printf("[LVGL] ========================================\n");

  std::printf("[LVGL] Starting LVGL WASM application\n");

  std::printf("[LVGL] Display size: %d x %d\n",

              LV_DISPLAY_PORTABLE_WIDTH, LV_DISPLAY_PORTABLE_HEIGHT);

  // ==========================================================
  // LVGL initialization
  // ==========================================================

  lv_init();

  std::printf("[LVGL] lv_init() complete\n");

  // ==========================================================
  // Create display
  // ==========================================================

  g_display =
      lv_display_create(LV_DISPLAY_PORTABLE_WIDTH, LV_DISPLAY_PORTABLE_HEIGHT);

  if (g_display == nullptr) {
    std::printf("[LVGL] ERROR: lv_display_create() failed\n");

    return 1;
  }

  std::printf("[LVGL] Display created\n");

  // ==========================================================
  // RGB565
  // ==========================================================

  lv_display_set_color_format(g_display, LV_COLOR_FORMAT_RGB565);

  std::printf("[LVGL] Color format = RGB565\n");

  // ==========================================================
  // Framebuffer
  // ==========================================================

  lv_display_set_buffers(g_display,

                         g_framebuffer,

                         nullptr,

                         sizeof(g_framebuffer),

                         LV_DISPLAY_RENDER_MODE_FULL);

  std::printf("[LVGL] Framebuffer configured: %u bytes\n",

              static_cast<unsigned>(sizeof(g_framebuffer)));

  // ==========================================================
  // Flush callback
  // ==========================================================

  lv_display_set_flush_cb(g_display, flushDisplay);

  std::printf("[LVGL] Flush callback installed\n");

  // ==========================================================
  // Default display
  // ==========================================================

  lv_display_set_default(g_display);

  std::printf("[LVGL] Default display configured\n");

  // ==========================================================
  // Create root screen
  // ==========================================================

  g_screen = lv_obj_create(nullptr);

  if (g_screen == nullptr) {
    std::printf("[LVGL] ERROR: failed to create screen\n");

    return 1;
  }

  std::printf("[LVGL] Root screen created\n");

  lv_obj_set_size(g_screen,

                  LV_DISPLAY_PORTABLE_WIDTH, LV_DISPLAY_PORTABLE_HEIGHT);

  lv_obj_center(g_screen);

  // ==========================================================
  // Create BrowserHost
  //
  // IMPORTANT:
  // BrowserHost is now a completely independent class.
  // ==========================================================

  BrowserHost browserHost;

  // ==========================================================
  // Create LVGL controller
  // ==========================================================

  std::printf("[LVGL] Initializing LvDisplayController\n");

  g_controller = std::make_unique<LvDisplayController>(&browserHost);

  if (!g_controller) {
    std::printf("[LVGL] ERROR: failed to allocate controller\n");

    return 1;
  }

  g_controller->init(g_screen);

  std::printf("[LVGL] LvDisplayController initialized\n");

  // ==========================================================
  // Load screen
  // ==========================================================

  lv_screen_load(g_screen);

  std::printf("[LVGL] Controller screen loaded\n");

#ifdef __EMSCRIPTEN__

  // ==========================================================
  // Browser pointer handlers
  // ==========================================================

  installBrowserPointerHandlers();

  std::printf("[LVGL] Browser pointer handlers installed\n");

  // ==========================================================
  // LVGL pointer device
  // ==========================================================

  g_pointerIndev = lv_indev_create();

  if (g_pointerIndev == nullptr) {
    std::printf("[LVGL] ERROR: failed to create pointer input device\n");

    return 1;
  }

  lv_indev_set_type(g_pointerIndev, LV_INDEV_TYPE_POINTER);

  lv_indev_set_read_cb(g_pointerIndev, browserPointerReadCallback);

  lv_indev_set_display(g_pointerIndev, g_display);

  std::printf("[LVGL] LVGL pointer input device configured\n");

#endif

  // ==========================================================
  // Initial UI refresh
  // ==========================================================

  g_controller->refresh();

  lv_refr_now(g_display);

  std::printf("[LVGL] Initial refresh requested\n");

#ifdef __EMSCRIPTEN__

  // ==========================================================
  // Get initial ESP32 state
  // ==========================================================

  browserRequestState();

  // ==========================================================
  // Start automatic state synchronization
  // ==========================================================

  browserStartStatePolling();

  // ==========================================================
  // Start Emscripten main loop
  // ==========================================================

  std::printf("[LVGL] Starting Emscripten main loop\n");

  emscripten_set_main_loop(webLoop, 60, 1);

#else

  std::printf("[LVGL] WARNING: not compiled with Emscripten\n");

#endif

  return 0;
}