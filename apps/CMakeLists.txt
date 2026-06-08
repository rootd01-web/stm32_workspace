# ==============================================================================
# Top-level Apps CMakeLists — Selects correct category directory based on board
# ==============================================================================

if(DRIVER_BOARD STREQUAL "F411CEU6")
  add_subdirectory(stm32f4xx_apps)
elseif(DRIVER_BOARD STREQUAL "F103VET6")
  add_subdirectory(stm32f1xx_apps)
elseif(DRIVER_BOARD STREQUAL "G474RET6")
  add_subdirectory(stm32g4xx_apps)
else()
  message(FATAL_ERROR "DRIVER_BOARD '${DRIVER_BOARD}' is not supported in the apps directory structure.")
endif()
