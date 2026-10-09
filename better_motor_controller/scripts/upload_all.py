Import("env")

import subprocess


def upload_all(source, target, env):

    print("=== Uploading filesystem ===")

    print("")
    print("=" * 60)
    print("Uploading LittleFS filesystem")
    print("=" * 60)
    print("")

    result = subprocess.run(
        [
            env.subst("$PYTHONEXE"),
            "-m",
            "platformio",
            "run",
            "-t",
            "uploadfs",
            "-e",
            env.subst("$PIOENV"),
        ],
        cwd=env.subst("$PROJECT_DIR"),
    )

    if result.returncode != 0:
        print("LittleFS upload failed")
        env.Exit(result.returncode)

    print("")
    print("Firmware + LittleFS upload complete")
    print("")
env.AddPostAction(
    "upload",
    upload_all,
)