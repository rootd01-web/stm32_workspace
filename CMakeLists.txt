cmake_minimum_required(VERSION 3.21)

# ==============================================================================
# Root CMakeLists — STM32F411CEUx LED Blink Project
# Board   : STM32F411CEUx (Cortex-M4, 100MHz)
# App     : LED Blink (PC13)
# Toolchain: arm-none-eabi-gcc via CMakePresets.json
# ==============================================================================

# -------------------------------------------------------
# Build type (default Debug)
# -------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -------------------------------------------------------
# Board selection
# -------------------------------------------------------
set(DRIVER_BOARD "F411CEU6" CACHE STRING "Selected board")
set(SUPPORTED_BOARDS F411CEU6 F103VET6 G474RET6)
set_property(CACHE DRIVER_BOARD PROPERTY STRINGS ${SUPPORTED_BOARDS})

if(NOT DRIVER_BOARD IN_LIST SUPPORTED_BOARDS)
  message(FATAL_ERROR "DRIVER_BOARD must be one of: ${SUPPORTED_BOARDS}")
endif()

# -------------------------------------------------------
# App selection
# -------------------------------------------------------
set(APP_NAME "sd_card_basic" CACHE STRING "Application name")

# -------------------------------------------------------
# Project declaration (after toolchain is loaded via preset)
# -------------------------------------------------------
project(${APP_NAME}_${DRIVER_BOARD} C ASM)

# -------------------------------------------------------
# Board driver subdirectory
# CubeMX output is now directly in drivers/boards/F411CEU6/
# (no longer inside stm32f411ceu6_cfg_ver1 subfolder)
# -------------------------------------------------------
set(BOARD_BSP_DIR
  ${CMAKE_SOURCE_DIR}/drivers/boards/${DRIVER_BOARD}
)

if(NOT EXISTS ${BOARD_BSP_DIR}/CMakeLists.txt)
  message(FATAL_ERROR "Board CMakeLists not found: ${BOARD_BSP_DIR}/CMakeLists.txt")
endif()

add_subdirectory(${BOARD_BSP_DIR} board_bsp)

if(DRIVER_BOARD STREQUAL "F411CEU6")
  add_subdirectory(${CMAKE_SOURCE_DIR}/services/fatfs fatfs)
endif()



# -------------------------------------------------------
# App library
# -------------------------------------------------------
add_subdirectory(apps)


# -------------------------------------------------------
# Final executable
# Entry point: board Core/Src/main.c (HAL init + super loop)
# App logic  : app_${APP_NAME} library (blinky.c)
# -------------------------------------------------------
set(BOARD_MAIN_C ${BOARD_BSP_DIR}/Core/Src/main.c)

add_executable(${PROJECT_NAME}
  ${BOARD_MAIN_C}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  OUTPUT_NAME "${PROJECT_NAME}"
  SUFFIX ".elf"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  app_${APP_NAME}                     # blinky.c (includes blinky.h via PUBLIC)
  board_libs                          # core + hal + cmsis
  $<TARGET_OBJECTS:board_runtime>     # syscalls, sysmem
  $<TARGET_OBJECTS:board_vectors>     # startup + it
)

# -------------------------------------------------------
# Post-build: generate .hex, .bin, print size
# -------------------------------------------------------
find_program(OBJCOPY arm-none-eabi-objcopy HINTS ${TOOLCHAIN_BIN} REQUIRED)
find_program(SIZE    arm-none-eabi-size    HINTS ${TOOLCHAIN_BIN} REQUIRED)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${OBJCOPY} -O ihex  $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex
  COMMAND ${OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
  COMMAND ${SIZE} --format=berkeley $<TARGET_FILE:${PROJECT_NAME}>
  COMMENT "==> Generating ${PROJECT_NAME}.hex / .bin and printing size"
)

# -------------------------------------------------------
# Flash target: cmake --build build/... --target flash
# Requires ST-Link connected to board
# -------------------------------------------------------
set(OPENOCD_EXE "${OPENOCD_BIN}/openocd.exe")

add_custom_target(flash
  COMMAND ${OPENOCD_EXE}
    -s "${OPENOCD_SCRIPTS}"
    -f interface/stlink.cfg
    -c "transport select swd"
    -f "${OPENOCD_TARGET_CFG}"
    -c "debug_level 1"
    -c "program $<TARGET_FILE:${PROJECT_NAME}> verify reset exit"
  DEPENDS ${PROJECT_NAME}
  COMMENT "==> Flashing ${PROJECT_NAME}.elf via ST-Link + OpenOCD"
  USES_TERMINAL
)

# -------------------------------------------------------
# Summary
# -------------------------------------------------------
message(STATUS "========================================")
message(STATUS "Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "Board        : ${DRIVER_BOARD}")
message(STATUS "App          : ${APP_NAME}")
message(STATUS "Output       : ${PROJECT_NAME}.elf")
message(STATUS "OpenOCD bin  : ${OPENOCD_BIN}")
message(STATUS "OpenOCD cfg  : ${OPENOCD_TARGET_CFG}")
message(STATUS "OpenOCD scripts: ${OPENOCD_SCRIPTS}")
message(STATUS "========================================")