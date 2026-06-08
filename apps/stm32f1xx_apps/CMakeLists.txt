# ==============================================================================
# Aggregated CMakeLists for STM32F1xx Apps
# ==============================================================================

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${APP_NAME}/CMakeLists.txt)
  message(FATAL_ERROR "STM32F1xx App '${APP_NAME}' CMakeLists.txt not found in ${CMAKE_CURRENT_SOURCE_DIR}/${APP_NAME}")
endif()

add_subdirectory(${APP_NAME})
