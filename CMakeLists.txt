cmake_minimum_required(VERSION 3.13)

# Pico SDK をインポート
include(pico_sdk_import.cmake)

project(PicoVSyncSeparator C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# メイン実行ファイル（V-Sync Separator専用）
add_executable(PicoVSyncSeparator
    vsync_main.c
    vsync_separator.c
)

# PIOファイルを生成（add_executableの後に配置）
pico_generate_pio_header(PicoVSyncSeparator ${CMAKE_CURRENT_LIST_DIR}/vsync_separator.pio)

# PicoRapidX2: VSync検出機能統合版
add_executable(PicoRapidX2
    PicoRapidX.c
    usb_msc_device.c
    usb_descriptors.c
    vsync_separator.c
)

# PIOファイルを生成（PicoRapidX2用）
pico_generate_pio_header(PicoRapidX2 ${CMAKE_CURRENT_LIST_DIR}/vsync_separator.pio)

# 既存のPicoRapidXも別ターゲットとして保持
add_executable(PicoRapidX_Original
    PicoRapidX.c
    usb_msc_device.c
    usb_descriptors.c
)

# 追加ヘッダディレクトリが存在する場合のみ使用（無ければコメントアウト可）
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/header)
    target_include_directories(PicoVSyncSeparator PRIVATE ${CMAKE_CURRENT_LIST_DIR}/header)
endif()

# 標準IO設定
pico_enable_stdio_usb(PicoVSyncSeparator 0)
pico_enable_stdio_uart(PicoVSyncSeparator 1)

pico_enable_stdio_usb(PicoRapidX2 0)
pico_enable_stdio_uart(PicoRapidX2 0)

# TinyUSB needs access to tusb_config.h from this dir
add_library(tinyusb_config INTERFACE)
target_include_directories(tinyusb_config INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(PicoVSyncSeparator
    pico_stdlib
    hardware_flash
    hardware_sync
    hardware_pwm
    hardware_pio
    pico_multicore
    hardware_i2c
)

target_link_libraries(PicoRapidX2
    pico_stdlib
    hardware_flash
    hardware_sync
    hardware_pwm
    hardware_pio
    pico_multicore
    hardware_i2c
    tinyusb_device
    tinyusb_board
    tinyusb_config
)

target_link_libraries(PicoRapidX_Original
    pico_stdlib
    hardware_flash
    hardware_sync
    hardware_pwm
    pico_multicore
    hardware_i2c
    tinyusb_device
    tinyusb_board
    tinyusb_config
)

# 追加生成: map/bin/hex/uf2
pico_add_extra_outputs(PicoVSyncSeparator)
pico_add_extra_outputs(PicoRapidX2)
pico_add_extra_outputs(PicoRapidX_Original)
