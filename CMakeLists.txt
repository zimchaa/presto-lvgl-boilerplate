cmake_minimum_required(VERSION 3.13)

# ─── Board / platform ────────────────────────────────────────────────
# Use the Presto's own board definition (from the pimoroni/presto repo):
# RP2350B with 48 GPIOs and the tuned 200MHz system PLL that the ST7701
# display timing was validated against.
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD presto)
set(PICO_NUM_GPIOS 48)
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/lib/presto/boards/presto)

# The Presto has 8MB of QMI PSRAM on CS1 (GPIO 47). Setting this before
# pico_sdk_init() adds the PSRAM region to the linker script, so variables
# can be placed there with __in_psram / __uninitialized_psram.
set(PICO_PSRAM_SIZE_BYTES "(8 * 1024 * 1024)")

# The Presto carries a Raspberry Pi RM2 (CYW43439) wireless module. The board
# header defines the CYW43 pins but not the cmake flag, so declare support
# here — it makes the pico_cyw43_arch_* libraries available to targets that
# want them (the kitchen-sink demo links pico_cyw43_arch_none for WiFi scans).
set(PICO_CYW43_SUPPORTED 1)

# ─── Dependencies (submodules under lib/) ────────────────────────────
set(PICO_SDK_PATH        ${CMAKE_CURRENT_LIST_DIR}/lib/pico-sdk)
set(PIMORONI_PICO_PATH   ${CMAKE_CURRENT_LIST_DIR}/lib/pimoroni-pico)
set(PIMORONI_PRESTO_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/presto)

set(NAME presto-lvgl)

# ─── Display mode ────────────────────────────────────────────────────
# OFF (default): 240x240 logical, pixel-doubled in hardware, all in SRAM.
# ON:  native 480x480 RGB565 — LVGL front buffer and heap live in PSRAM,
#      the scanout buffer stays in SRAM. See src/display_config.hpp.
option(PRESTO_FULL_RES "Run the display at native 480x480 instead of 240x240" OFF)
if (PRESTO_FULL_RES)
    # Directory-scoped so lv_conf.h sees it when LVGL itself is compiled.
    add_compile_definitions(PRESTO_FULL_RES=1)
endif()

include(pimoroni_pico_import.cmake)
include(pico_sdk_import.cmake)

project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ─── Presto display driver ───────────────────────────────────────────
set(PIMORONI_PRESTO_PATH ${PIMORONI_PRESTO_PATH} CACHE PATH "Path to the Presto libraries" FORCE)
include_directories(${PIMORONI_PRESTO_PATH})
list(APPEND CMAKE_MODULE_PATH ${PIMORONI_PRESTO_PATH})

include(common/pimoroni_bus)
include(libraries/pico_graphics/pico_graphics)
include(drivers/st7701/st7701_presto)

# WS2812 driver (the 7 ambient RGB LEDs) for the demo target.
include(${PIMORONI_PICO_PATH}/drivers/plasma/plasma.cmake)

# ─── LVGL ────────────────────────────────────────────────────────────
set(LV_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/lv_conf.h)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
add_subdirectory(lib/lvgl)
# For LV_MEM_POOL_INCLUDE ("lvgl_psram_pool.h") in full-res builds.
target_include_directories(lvgl PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

# ─── Application ─────────────────────────────────────────────────────
add_executable(${NAME}
    src/main.cpp
    src/lvgl_port.cpp
)

target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/lvgl)

target_link_libraries(${NAME}
    st7701_presto
    lvgl
    pico_stdlib
    pico_multicore
    pico_graphics
    hardware_interp
    hardware_i2c
    hardware_psram
)

# With CS pin + size defined, hardware_psram's runtime_init hook brings the
# PSRAM up before main() (and before core1 is launched).
target_compile_definitions(${NAME} PRIVATE
    PICO_PSRAM_CS_PIN=47
    PICO_PSRAM_SIZE_BYTES=8388608
)

pico_add_extra_outputs(${NAME})       # produces presto-lvgl.uf2
pico_enable_stdio_uart(${NAME} 0)
pico_enable_stdio_usb(${NAME} 1)      # printf() over USB serial

# ─── Kitchen-sink demo ───────────────────────────────────────────────
# Second firmware that exercises every bit of Presto hardware (LEDs,
# buzzer, WiFi scan, SD card, temperature sensor, backlight) behind a
# tabbed LVGL interface. Shares the display/touch port with the plain demo.
set(DEMO presto-lvgl-demo)

add_executable(${DEMO}
    src/demo/demo_main.cpp
    src/lvgl_port.cpp
    src/demo/hw_leds.cpp
    src/demo/hw_buzzer.cpp
    src/demo/hw_sdcard.cpp
    src/demo/hw_wifi.cpp
    src/demo/ui_home.cpp
    src/demo/ui_leds.cpp
    src/demo/ui_sound.cpp
    src/demo/ui_paint.cpp
    src/demo/ui_wifi.cpp
    src/demo/ui_system.cpp
)

target_include_directories(${DEMO} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvgl
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(${DEMO}
    st7701_presto
    lvgl
    pico_stdlib
    pico_multicore
    pico_graphics
    hardware_interp
    hardware_i2c
    hardware_psram
    hardware_pwm
    hardware_adc
    hardware_spi
    plasma
    pico_cyw43_arch_none
    pico_unique_id
)

target_compile_definitions(${DEMO} PRIVATE
    PICO_PSRAM_CS_PIN=47
    PICO_PSRAM_SIZE_BYTES=8388608
    CYW43_HOST_NAME="presto-demo"
    PRESTO_DEMO=1     # smaller LVGL stripe buffers at full res (see display_config.hpp)
)

pico_add_extra_outputs(${DEMO})       # produces presto-lvgl-demo.uf2
pico_enable_stdio_uart(${DEMO} 0)
pico_enable_stdio_usb(${DEMO} 1)
