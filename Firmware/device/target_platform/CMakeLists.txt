cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME target_platform)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
  
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/modules/fifo.c
    Core/Src/modules/fsm.c
    Core/Src/modules/ir_adc.c
    Core/Src/modules/ir_processing.c
    Core/Src/modules/logger.c
    Core/Src/modules/rs485.c

    # BSP Devices
    BSP/Dev/Src/bmi323.c
    BSP/Dev/Src/bq76925.c
    BSP/Dev/Src/led_pwm_control.c 
    BSP/Dev/Src/photodiodes.c

    # BSP wrapper for STM32G0 HAL
    BSP/Port/Src/stm32g0/bsp_pwm.c
    BSP/Port/Src/stm32g0/bsp_spi.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    BSP/Dev/Inc/
    BSP/Port/Inc/
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
