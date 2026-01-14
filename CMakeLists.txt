cmake_minimum_required(VERSION 3.13)

# Initialize SDK Config
include(pico_sdk_import.cmake)

# ✅ ARCHITECTURE: FreeRTOS (Required for Project)
set(PICO_BOARD pico_w)
set(PICO_CYW43_ARCH "lwip_sys_freertos")

project(led_control_webserver C CXX ASM)

# Initialize SDK
pico_sdk_init()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(led_control_webserver main.c)

# STDIO via USB
pico_enable_stdio_uart(led_control_webserver 0)
pico_enable_stdio_usb(led_control_webserver 1)

# FreeRTOS Includes
target_include_directories(led_control_webserver PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel/include
    ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/include
)

target_link_libraries(led_control_webserver
    pico_stdlib
    hardware_i2c
    hardware_pio
    hardware_pwm
    hardware_dma
    hardware_clocks
    hardware_adc
    pico_cyw43_arch_lwip_sys_freertos
)

target_sources(led_control_webserver PRIVATE
    ssd1306.c
    ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel/tasks.c
    ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel/queue.c
    ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel/list.c
    ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel/timers.c
    ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel/event_groups.c
    ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel/stream_buffer.c
    ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/port.c
    ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel/portable/MemMang/heap_4.c
)

# Compile Definitions
target_compile_definitions(led_control_webserver PRIVATE
    CYW43_LWIP=1
    LWIP_PROVIDE_ERRNO=1
    NO_SYS=0 # OS Mode
)

pico_generate_pio_header(led_control_webserver ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
pico_add_extra_outputs(led_control_webserver)
