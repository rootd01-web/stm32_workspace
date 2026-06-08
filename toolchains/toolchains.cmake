# ==============================================================================
#  ARM GCC Toolchain — Cortex-M4 (STM32F4xx)
#  Đường dẫn toolchain được override từ CMakePresets.json qua TOOLCHAIN_BIN
# ==============================================================================

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

# TOOLCHAIN_BIN được truyền vào từ CMakePresets.json
if(NOT DEFINED TOOLCHAIN_BIN)
  set(TOOLCHAIN_BIN "")
endif()

# Trên Windows dùng .exe, Linux/Mac không cần
if(WIN32)
  set(_EXE ".exe")
else()
  set(_EXE "")
endif()

# Compilers — đường dẫn tuyệt đối với extension đúng
set(CMAKE_C_COMPILER   "${TOOLCHAIN_BIN}/arm-none-eabi-gcc${_EXE}"   CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN}/arm-none-eabi-g++${_EXE}"   CACHE FILEPATH "C++ compiler")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_BIN}/arm-none-eabi-gcc${_EXE}"   CACHE FILEPATH "ASM compiler")
set(CMAKE_AR           "${TOOLCHAIN_BIN}/arm-none-eabi-ar${_EXE}"    CACHE FILEPATH "Archiver")
set(CMAKE_OBJCOPY      "${TOOLCHAIN_BIN}/arm-none-eabi-objcopy${_EXE}" CACHE FILEPATH "objcopy")
set(CMAKE_SIZE         "${TOOLCHAIN_BIN}/arm-none-eabi-size${_EXE}"  CACHE FILEPATH "size")

# Không chạy test compile trên host machine
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Tắt auto-detection compiler ABI (tránh lỗi cross-compile)
set(CMAKE_C_COMPILER_WORKS   1 CACHE INTERNAL "")
set(CMAKE_CXX_COMPILER_WORKS 1 CACHE INTERNAL "")
