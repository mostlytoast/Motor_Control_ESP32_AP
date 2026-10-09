to give your esp 32 more memory you need to go to tools->partitions and select large app. if your board does not have that property listed then go into your board.txt and replace its settings with the ones bellow making sure to replace esp32doit-devkit-v1 with your boards name, save it then click tools -> reload boards 


##############################################################

esp32doit-devkit-v1.name=DOIT ESP32 DEVKIT V1

esp32doit-devkit-v1.bootloader.tool=esptool_py
esp32doit-devkit-v1.bootloader.tool.default=esptool_py

esp32doit-devkit-v1.upload.tool=esptool_py
esp32doit-devkit-v1.upload.tool.default=esptool_py
esp32doit-devkit-v1.upload.tool.network=esp_ota

esp32doit-devkit-v1.upload.maximum_size=1310720
esp32doit-devkit-v1.upload.maximum_data_size=327680
esp32doit-devkit-v1.upload.flags=
esp32doit-devkit-v1.upload.erase_cmd=
esp32doit-devkit-v1.upload.extra_flags=

esp32doit-devkit-v1.serial.disableDTR=true
esp32doit-devkit-v1.serial.disableRTS=true

esp32doit-devkit-v1.build.tarch=xtensa
esp32doit-devkit-v1.build.bootloader_addr=0x1000
esp32doit-devkit-v1.build.target=esp32
esp32doit-devkit-v1.build.mcu=esp32
esp32doit-devkit-v1.build.core=esp32
esp32doit-devkit-v1.build.variant=doitESP32devkitV1
esp32doit-devkit-v1.build.board=ESP32_DEV

esp32doit-devkit-v1.build.f_cpu=240000000L
esp32doit-devkit-v1.build.flash_mode=dio
esp32doit-devkit-v1.build.flash_size=4MB
esp32doit-devkit-v1.build.boot=dio
esp32doit-devkit-v1.build.partitions=default
esp32doit-devkit-v1.build.defines=

esp32doit-devkit-v1.menu.FlashFreq.80=80MHz
esp32doit-devkit-v1.menu.FlashFreq.80.build.flash_freq=80m
esp32doit-devkit-v1.menu.FlashFreq.40=40MHz
esp32doit-devkit-v1.menu.FlashFreq.40.build.flash_freq=40m

esp32doit-devkit-v1.menu.PartitionScheme.default=Default with spiffs
esp32doit-devkit-v1.menu.PartitionScheme.default.build.partitions=default
esp32doit-devkit-v1.menu.PartitionScheme.defaultffat=Default with ffat
esp32doit-devkit-v1.menu.PartitionScheme.defaultffat.build.partitions=default_ffat
esp32doit-devkit-v1.menu.PartitionScheme.no_ota=No OTA (Large APP)
esp32doit-devkit-v1.menu.PartitionScheme.no_ota.build.partitions=no_ota
esp32doit-devkit-v1.menu.PartitionScheme.no_ota.upload.maximum_size=2097152
esp32doit-devkit-v1.menu.PartitionScheme.min_spiffs=Minimal SPIFFS (Large APPS with OTA)
esp32doit-devkit-v1.menu.PartitionScheme.min_spiffs.build.partitions=min_spiffs
esp32doit-devkit-v1.menu.PartitionScheme.min_spiffs.upload.maximum_size=1966080

esp32doit-devkit-v1.menu.UploadSpeed.921600=921600
esp32doit-devkit-v1.menu.UploadSpeed.921600.upload.speed=921600
esp32doit-devkit-v1.menu.UploadSpeed.115200=115200
esp32doit-devkit-v1.menu.UploadSpeed.115200.upload.speed=115200
esp32doit-devkit-v1.menu.UploadSpeed.256000.windows=256000
esp32doit-devkit-v1.menu.UploadSpeed.256000.upload.speed=256000
esp32doit-devkit-v1.menu.UploadSpeed.230400.windows.upload.speed=256000
esp32doit-devkit-v1.menu.UploadSpeed.230400=230400
esp32doit-devkit-v1.menu.UploadSpeed.230400.upload.speed=230400
esp32doit-devkit-v1.menu.UploadSpeed.460800.linux=460800
esp32doit-devkit-v1.menu.UploadSpeed.460800.macosx=460800
esp32doit-devkit-v1.menu.UploadSpeed.460800.upload.speed=460800
esp32doit-devkit-v1.menu.UploadSpeed.512000.windows=512000
esp32doit-devkit-v1.menu.UploadSpeed.512000.upload.speed=512000

esp32doit-devkit-v1.menu.DebugLevel.none=None
esp32doit-devkit-v1.menu.DebugLevel.none.build.code_debug=0
esp32doit-devkit-v1.menu.DebugLevel.error=Error
esp32doit-devkit-v1.menu.DebugLevel.error.build.code_debug=1
esp32doit-devkit-v1.menu.DebugLevel.warn=Warn
esp32doit-devkit-v1.menu.DebugLevel.warn.build.code_debug=2
esp32doit-devkit-v1.menu.DebugLevel.info=Info
esp32doit-devkit-v1.menu.DebugLevel.info.build.code_debug=3
esp32doit-devkit-v1.menu.DebugLevel.debug=Debug
esp32doit-devkit-v1.menu.DebugLevel.debug.build.code_debug=4
esp32doit-devkit-v1.menu.DebugLevel.verbose=Verbose
esp32doit-devkit-v1.menu.DebugLevel.verbose.build.code_debug=5

esp32doit-devkit-v1.menu.EraseFlash.none=Disabled
esp32doit-devkit-v1.menu.EraseFlash.all=Enabled
esp32doit-devkit-v1.menu.EraseFlash.all.upload.erase_cmd=-e



need local pio to upload fs 
python3 -m venv ~/.platformio-venv
source ~/.platformio-venv/bin/activate
pip install -U pip
pip install -U platformio



pio run -t uploadfs


 sh ./web/build_emscripten.sh && pio run -t uploadfs



 TODO 
 -find way to prevent vscode from searching in lib and data etc files 
 -speed up emscripten building and normal vs code build 
 -prevent formatting in emscripten_ui file
 -encorperate ui building and upload into main upload function and ensure it does not run unless something changed (make file?)
 - find way to combine transmitter and controller in one repo and have shared lv display files
 - find way for pio to upload to a board based on serial # to avoid confusion? 
 - get map display into lv display
 - reduce clutter and refactor 
 - add automatic navigation features and scanning to track function (includes going to stations line occupied and navigation through switches)
 - have registration and tag writing function built into lv display 
 - make website display nicer and less lag
 - write up build instructions 
 - see if you can install and build this on other platforms mac linux and windows 
 - add automation that builds for few different board types? 
 - add serial output to lv display 
 - change speed control buttons to slider
 - clean up emerscript build script so that we can have display code elsewhere?
 
 - write about how to configure serial port  ls /dev/serial/by-id/
 - need to integrate the sendjson for transmitter working so it can work with new display setup (and receive part too)