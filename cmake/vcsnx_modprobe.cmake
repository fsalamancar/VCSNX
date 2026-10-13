# modprobe.nro + modtest.so (v1, Task 3): the game-module loader on the console before the game uses it.
enable_language(ASM)
find_program(VCSNX_NM aarch64-none-elf-nm HINTS "$ENV{DEVKITPRO}/devkitA64/bin" /opt/devkitpro/devkitA64/bin REQUIRED)
set(VCSNX_MODTEST "${CMAKE_BINARY_DIR}/modtest.so")
add_custom_command(OUTPUT "${VCSNX_MODTEST}"
    COMMAND "${CMAKE_CXX_COMPILER}" -march=armv8-a+crc+crypto -mtune=cortex-a57 -fPIC -O2 -std=c++20
            -shared -nostdlib -Wl,-Bsymbolic "${CMAKE_SOURCE_DIR}/tools/modprobe/modtest.cpp"
            "${VCSNX_PLATFORM_DIR}/module_eh_end.S" -o "${VCSNX_MODTEST}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/modprobe/modtest.cpp" "${VCSNX_PLATFORM_DIR}/module_eh_end.S" VERBATIM)
set(VCSNX_MODPROBE_IMPORTS "${CMAKE_BINARY_DIR}/modprobe_imports.S")
add_custom_command(OUTPUT "${VCSNX_MODPROBE_IMPORTS}"
    COMMAND python3 -I "${CMAKE_SOURCE_DIR}/tools/make_module_imports.py" "${VCSNX_NM}" "${VCSNX_MODTEST}" "${VCSNX_MODPROBE_IMPORTS}"
    DEPENDS "${VCSNX_MODTEST}" "${CMAKE_SOURCE_DIR}/tools/make_module_imports.py" VERBATIM)
add_executable(modprobe "${CMAKE_SOURCE_DIR}/tools/modprobe/modprobe.cpp" "${VCSNX_PLATFORM_DIR}/game_module.cpp"
    "${VCSNX_PLATFORM_DIR}/elf_module.cpp" "${VCSNX_MODPROBE_IMPORTS}")
target_include_directories(modprobe PRIVATE "${VCSNX_PLATFORM_DIR}")
nx_generate_nacp(modprobe.nacp NAME "VCSNX module probe" AUTHOR "VCSNX" VERSION "${PROJECT_VERSION}")
nx_create_nro(modprobe NACP modprobe.nacp)
