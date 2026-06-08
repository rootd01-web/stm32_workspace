file(GLOB APP_SOURCES CONFIGURE_DEPENDS "*.c" "*.h")

add_library(app_${APP_NAME} STATIC
  ${APP_SOURCES}
)

target_include_directories(app_${APP_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(app_${APP_NAME} PUBLIC
  board_libs
)

