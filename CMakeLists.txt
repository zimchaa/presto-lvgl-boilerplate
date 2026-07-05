cmake_minimum_required(VERSION 3.13)

# ─── Board / platform ────────────────────────────────────────────────
# Use the Presto's own board definition (from the pimoroni/presto repo):
# RP2350B with 48 GPIOs and the tuned 200MHz system PLL that the ST7701
# display timing was validated against.
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD presto)
set(PICO_NUM_GPIOS 48)
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/lib/presto/boards/presto)

# ─── Dependencies (submodules under lib/) ────────────────────────────
set(PICO_SDK_PATH        ${CMAKE_CURRENT_LIST_DIR}/lib/pico-sdk)
set(PIMORONI_PICO_PATH   ${CMAKE_CURRENT_LIST_DIR}/lib/pimoroni-pico)
set(PIMORONI_PRESTO_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/presto)

set(NAME presto-lvgl)

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

# ─── LVGL ────────────────────────────────────────────────────────────
set(LV_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/lv_conf.h)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
add_subdirectory(lib/lvgl)

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
)

pico_add_extra_outputs(${NAME})       # produces presto-lvgl.uf2
pico_enable_stdio_uart(${NAME} 0)
pico_enable_stdio_usb(${NAME} 1)      # printf() over USB serial
