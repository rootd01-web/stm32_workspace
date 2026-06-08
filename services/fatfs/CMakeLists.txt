# ==============================================================================
# CMakeLists for FatFs Middleware + SPI SD Card Driver
# ==============================================================================

add_library(services_fatfs STATIC
  src/ff.c
  src/diskio.c
  src/fatfs_sd.c
)

target_include_directories(services_fatfs PUBLIC
  src
)

target_link_libraries(services_fatfs PUBLIC
  board_libs
)
