#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

if ! command -v emcc >/dev/null 2>&1; then
  echo "Emscripten is not installed or not on PATH."
  echo "Install it from https://emscripten.org/docs/getting_started/download.html"
  exit 1
fi

if ! command -v em++ >/dev/null 2>&1; then
  echo "Emscripten C++ compiler (em++) is not installed or not on PATH."
  exit 1
fi

if ! [ -d ".pio/libdeps/esp32dev/ArduinoJson/src" ]; then
  echo "ArduinoJson was not found."
  echo "Expected:"
  echo "  $ROOT_DIR/.pio/libdeps/esp32dev/ArduinoJson/src"
  echo
  echo "Run:"
  echo "  pio run"
  echo "to install the PlatformIO dependencies first."
  exit 1
fi

if ! [ -d "vendor/lvgl" ]; then
  echo "Cloning LVGL into vendor/lvgl..."
  mkdir -p vendor
  git clone --depth 1 https://github.com/lvgl/lvgl.git vendor/lvgl
fi

# --------------------------------------------------------------------
# LVGL object cache
#
# LVGL is compiled only when web/dist does not exist.
#
# Delete web/dist manually when you want to force a complete LVGL
# rebuild.
# --------------------------------------------------------------------

DIST_DIR="web/dist"
OBJ_DIR="$DIST_DIR/.emscripten_objs"
SHARED_DIR="../shared"
REBUILD_LVGL=0

if [ ! -d "$DIST_DIR" ]; then
  echo
  echo "============================================================"
  echo "web/dist does not exist."
  echo "LVGL will be compiled."
  echo "============================================================"
  echo

  mkdir -p "$DIST_DIR"
  REBUILD_LVGL=1
else
  echo
  echo "============================================================"
  echo "web/dist exists."
  echo "Using cached LVGL object files."
  echo "Delete web/dist to force an LVGL rebuild."
  echo "============================================================"
  echo
fi

# ArduinoJson include directory
ARDUINOJSON_DIR=".pio/libdeps/esp32dev/ArduinoJson/src"

CFLAGS="-O2 \
-I. \
-Iweb \
-Ivendor/lvgl \
-Ivendor/lvgl/src \
-DLV_CONF_PATH=\"web/lv_conf.h\""

CPPFLAGS="-O2 \
-std=c++17 \
-I. \
-Iweb \
-Ivendor/lvgl \
-Ivendor/lvgl/src \
-I$ARDUINOJSON_DIR \
-DLV_CONF_PATH=\"web/lv_conf.h\""

# --------------------------------------------------------------------
# Compile LVGL only when web/dist was deleted
# --------------------------------------------------------------------

if [ "$REBUILD_LVGL" -eq 1 ]; then

  # Start with a completely clean LVGL object directory.
  rm -rf "$OBJ_DIR"
  mkdir -p "$OBJ_DIR"

  echo
  echo "Compiling LVGL..."
  echo

  find vendor/lvgl/src -name '*.c' \
    ! -name 'lv_port_disp_template.c' \
    ! -path '*/draw/nxp/*' \
    ! -path '*/draw/renesas/*' \
    ! -path '*/draw/vg_lite/*' \
    ! -path '*/draw/sdl/*' \
    ! -path '*/draw/arm2d/*' \
    ! -path '*/draw/ccanvas/*' \
    -print | while IFS= read -r src; do

    rel="${src#vendor/lvgl/src/}"
    obj="$OBJ_DIR/${rel%.c}.o"

    mkdir -p "$(dirname "$obj")"

    echo "Compiling LVGL: $src"

    emcc $CFLAGS -c "$src" -o "$obj"
  done

  echo
  echo "LVGL compilation complete."
  echo

else

  # Make sure the cached object directory actually exists.
  if [ ! -d "$OBJ_DIR" ]; then
    echo
    echo "ERROR: web/dist exists, but LVGL object cache is missing:"
    echo "  $OBJ_DIR"
    echo
    echo "Delete web/dist and run this script again."
    echo
    exit 1
  fi

  echo "Using cached LVGL objects from:"
  echo "  $OBJ_DIR"
  echo
fi

# --------------------------------------------------------------------
# Compile WebAssembly UI
# --------------------------------------------------------------------

echo "Compiling WebAssembly UI..."

em++ $CPPFLAGS \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s ASSERTIONS=0 \
  -O2 \
  -sEXPORTED_FUNCTIONS='["_main","_emscriptenReceiveJson"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
  --shell-file web/shell.html \
  web/emscripten_ui.cpp \
  web/Arduino.cpp \
  $(find "$SHARED_DIR" -type f -name '*.cpp' ! -name 'espnow_controller.cpp' ! -name 'ControllerStorage.cpp' -print) \
  $(find "$OBJ_DIR" -name '*.o' -print) \
  -o "$DIST_DIR/index.html"
  
echo
echo "Built WebAssembly UI:"
echo "  $ROOT_DIR/$DIST_DIR/index.html"
echo

# --------------------------------------------------------------------
# Copy generated WebAssembly files to ESP32 LittleFS data directory
# --------------------------------------------------------------------

cp "$DIST_DIR/index.html" data/
cp "$DIST_DIR/index.js" data/
cp "$DIST_DIR/index.wasm" data/

echo "Copied WebAssembly files to data/:"
echo "  data/index.html"
echo "  data/index.js"
echo "  data/index.wasm"
