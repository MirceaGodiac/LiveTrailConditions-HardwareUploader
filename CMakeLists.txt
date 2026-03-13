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
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Live-Trail-Conditions-CPP-Uploader C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Load API key from .env (preferred) or process environment.
set(APP_API_KEY "")
set(APP_ENV_FILE "${CMAKE_SOURCE_DIR}/.env")

if(EXISTS "${APP_ENV_FILE}")
    file(STRINGS "${APP_ENV_FILE}" APP_ENV_API_KEY_LINE REGEX "^API_KEY=")
    if(APP_ENV_API_KEY_LINE)
        list(GET APP_ENV_API_KEY_LINE 0 APP_ENV_API_KEY_LINE0)
        string(REGEX REPLACE "^API_KEY=" "" APP_API_KEY "${APP_ENV_API_KEY_LINE0}")
    endif()
endif()

if(APP_API_KEY STREQUAL "" AND DEFINED ENV{API_KEY})
    set(APP_API_KEY "$ENV{API_KEY}")
endif()

if(APP_API_KEY STREQUAL "")
    message(FATAL_ERROR "API key missing. Set API_KEY in .env or environment.")
endif()

string(REPLACE "\"" "\\\"" APP_API_KEY_ESCAPED "${APP_API_KEY}")
configure_file(${CMAKE_SOURCE_DIR}/app_secrets.h.in ${CMAKE_BINARY_DIR}/generated/app_secrets.h @ONLY)

# Add executable. Default name is the project name, version 0.1

add_executable(Live-Trail-Conditions-CPP-Uploader
    Live-Trail-Conditions-CPP-Uploader.c
    modem.c
    sensors.c
    power_sleep.c
    upload_cycle.c
    utils.c
)

pico_set_program_name(Live-Trail-Conditions-CPP-Uploader "Live-Trail-Conditions-CPP-Uploader")
pico_set_program_version(Live-Trail-Conditions-CPP-Uploader "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Live-Trail-Conditions-CPP-Uploader 0)
pico_enable_stdio_usb(Live-Trail-Conditions-CPP-Uploader 1)

# Add the standard library to the build
target_link_libraries(Live-Trail-Conditions-CPP-Uploader
    pico_stdlib
    hardware_adc
    hardware_uart
    hardware_rtc      
    hardware_clocks          
)

# Add the standard include files to the build
target_include_directories(Live-Trail-Conditions-CPP-Uploader PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

pico_add_extra_outputs(Live-Trail-Conditions-CPP-Uploader)

