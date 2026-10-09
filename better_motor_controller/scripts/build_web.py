Import("env")

import os
import subprocess


PROJECT_DIR = env.subst("$PROJECT_DIR")
BUILD_SCRIPT = os.path.join(
    PROJECT_DIR,
    "web",
    "build_emscripten.sh",
)


def build_web(source=None, target=None, env=None):
    print("command targets",COMMAND_LINE_TARGETS)
    print("=== Building WebAssembly UI ===")

    print("")
    print("=" * 60)
    print("Building Emscripten Web UI")
    print("=" * 60)
    print("")

    result = subprocess.run(
        ["sh", BUILD_SCRIPT],
        cwd=PROJECT_DIR,
    )

    if result.returncode != 0:
        print("")
        print("Emscripten Web UI build FAILED")
        print("")
        env.Exit(result.returncode)

    print("")
    print("Emscripten Web UI build complete")
    print("")


# Run before the normal PlatformIO build.
env.AddPreAction("$BUILD_DIR/firmware.bin", build_web)