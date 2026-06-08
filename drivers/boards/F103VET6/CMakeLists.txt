# ==============================================================================
# CMakeLists for STM32F103VETx board driver
# Clock: HSE -> PLL -> SYSCLK 72MHz, APB1 36MHz, APB2 72MHz
# ==============================================================================

set(BOARD_DIR ${CMAKE_CURRENT_LIST_DIR})

# Paths
set(CMSIS_CORE_DIR   ${BOARD_DIR}/Drivers/CMSIS/Include)
set(CMSIS_DEVICE_DIR ${BOARD_DIR}/Drivers/CMSIS/Device/ST/STM32F1xx/Include)
set(HAL_DIR          ${BOARD_DIR}/Drivers/STM32F1xx_HAL_Driver)
set(CORE_INC_DIR     ${BOARD_DIR}/Core/Inc)
set(CORE_SRC_DIR     ${BOARD_DIR}/Core/Src)
set(STARTUP_DIR      ${BOARD_DIR}/Core/Startup)
set(LINKER_SCRIPT    ${BOARD_DIR}/STM32F103VETX_FLASH.ld)

# ==============================================================================
# CPU / Compile / Link flags for Cortex-M3 (STM32F1xx)
# ==============================================================================
set(CPU_FLAGS
  -mcpu=cortex-m3
  -mthumb
)

set(COMMON_COMPILE_FLAGS
  ${CPU_FLAGS}
  -ffunction-sections
  -fdata-sections
  -Wall
  -Wextra
  -Wno-unused-parameter
)

set(COMMON_LINK_FLAGS
  ${CPU_FLAGS}
  -Wl,--gc-sections
  -Wl,--print-memory-usage
  -Wl,--no-warn-rwx-segments
  -specs=nano.specs
  -lc -lm
)

# ==============================================================================
# 1) board_config  — INTERFACE: CPU flags, defines, include paths
# ==============================================================================
add_library(board_config INTERFACE)

target_compile_definitions(board_config INTERFACE
  STM32F103xE
  USE_HAL_DRIVER
)

target_include_directories(board_config INTERFACE
  ${CMSIS_CORE_DIR}
  ${CMSIS_DEVICE_DIR}
  ${HAL_DIR}/Inc
  ${CORE_INC_DIR}
)

target_compile_options(board_config INTERFACE
  ${COMMON_COMPILE_FLAGS}
  $<$<CONFIG:Debug>:   -Og -g3 -ggdb>
  $<$<CONFIG:Release>: -O2>
)

target_link_options(board_config INTERFACE
  ${COMMON_LINK_FLAGS}
  -T${LINKER_SCRIPT}
  -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map
  -Wl,--cref
  $<$<CONFIG:Debug>: -g>
)

# ==============================================================================
# 2) board_runtime  — OBJECT: syscalls + sysmem (tránh ODR issue với archive)
# ==============================================================================
add_library(board_runtime OBJECT
  ${CORE_SRC_DIR}/syscalls.c
  ${CORE_SRC_DIR}/sysmem.c
)

target_link_libraries(board_runtime PRIVATE board_config)

set_source_files_properties(
  ${CORE_SRC_DIR}/syscalls.c
  ${CORE_SRC_DIR}/sysmem.c
  PROPERTIES
    COMPILE_OPTIONS "-Wno-missing-prototypes;-Wno-unused-parameter"
)

# ==============================================================================
# 3) board_vectors  — OBJECT: startup .s + interrupt handlers
# ==============================================================================
add_library(board_vectors OBJECT
  ${STARTUP_DIR}/startup_stm32f103vetx.s
  ${CORE_SRC_DIR}/stm32f1xx_it.c
)

target_link_libraries(board_vectors PRIVATE board_config)

target_compile_definitions(board_vectors PRIVATE
  STM32F103xE
  USE_HAL_DRIVER
)

# ==============================================================================
# 4) cmsis  — STATIC: system_stm32f1xx.c (clock init tables)
# ==============================================================================
add_library(cmsis STATIC
  ${CORE_SRC_DIR}/system_stm32f1xx.c
)

target_link_libraries(cmsis PUBLIC board_config)

# ==============================================================================
# 5) hal  — STATIC: tất cả HAL source từ CubeMX
# ==============================================================================
file(GLOB HAL_SRC CONFIGURE_DEPENDS
  ${HAL_DIR}/Src/*.c
)

# Loại bỏ template files (không compile trực tiếp)
list(FILTER HAL_SRC EXCLUDE REGEX ".*_template\\.c$")

add_library(hal STATIC ${HAL_SRC})

target_link_libraries(hal PUBLIC board_config cmsis)

# ==============================================================================
# 6) core  — STATIC: GPIO init, HAL MSP, peripherals
# ==============================================================================
add_library(core STATIC
  ${CORE_SRC_DIR}/can.c
  ${CORE_SRC_DIR}/gpio.c
  ${CORE_SRC_DIR}/usart.c
  ${CORE_SRC_DIR}/stm32f1xx_hal_msp.c
)

target_link_libraries(core PUBLIC
  board_config
  cmsis
  hal
)

# ==============================================================================
# 7) board_libs  — INTERFACE aggregator (dùng để link vào app)
# ==============================================================================
add_library(board_libs INTERFACE)
target_link_libraries(board_libs INTERFACE
  core
  hal
  cmsis
)
