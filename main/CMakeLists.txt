# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.5)

if(DEFINED ENV{VERSION_TAG})
    set(VERSION_TAG $ENV{VERSION_TAG})
else()
    set(VERSION_TAG "v1.0.0-unknown")
endif()

# Define the source files
set(SOURCES 
    "main.c" 
    "door_handler.c"
    "mqtt_custom_handler.c"
)

# Specify the directory containing the header files
set(INCLUDES ".")

# Get all the component include directories
file(GLOB_RECURSE COMPONENT_SOURCES "components/*/*.c")

file(GLOB_RECURSE COMPONENT_INCLUDES "components/*/include")

set(AWS_SOURCE "certs/src/root_ca.c")
set(CERT_SOURCE "certs/src/certificate.c")
set(KEY_SOURCE "certs/src/private_key.c")

idf_component_register(
    SRCS 
        ${SOURCES}
        ${COMPONENT_SOURCES}
        ${AWS_SOURCE}
        ${CERT_SOURCE}
        ${KEY_SOURCE}
    INCLUDE_DIRS 
        ${INCLUDES}
        ${COMPONENT_INCLUDES}
    REQUIRES 
        nvs_flash 
        mbedtls 
        app_update 
        esp_https_ota 
        mqtt 
        driver 
        esp_wifi 
        json
        unity
    PRIV_REQUIRES 
        gecl-ota-manager
        gecl-wifi-manager
        gecl-time-sync-manager
        gecl-mqtt-manager
        gecl-misc-util-manager
        gecl-nvs-manager
        gecl-rgb-led-manager
)