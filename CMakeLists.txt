# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ctrl C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(ctrl main.c usb_descriptors.c bt_hid_xbox.c)

pico_set_program_name(ctrl "ctrl")
pico_set_program_version(ctrl "0.1")

# stdio: route to UART only. USB is the gamepad, so USB-CDC stdio MUST be
# off or it fights the HID device.
pico_enable_stdio_uart(ctrl 1)
pico_enable_stdio_usb(ctrl 0)

# Add the standard library + TinyUSB device stack to the build.
# Linking tinyusb_device is what makes pico-sdk inject CFG_TUSB_MCU /
# CFG_TUSB_OS as compile definitions.
target_link_libraries(ctrl PRIVATE
        pico_stdlib
        tinyusb_device
        pico_btstack_classic # btstack classic
        pico
        pico_btstack_cyw43 # connects bstack to the actual hardware drivers
        pico_cyw43_arch_none # bluetooth chip driver; no networking stack
    )

# Pick up tusb_config.h / usb_descriptors.c from the project root
target_include_directories(ctrl PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_SOURCE_DIR} 
)

pico_add_extra_outputs(ctrl)
