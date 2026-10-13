# The Switch build in two (spec v1 §2): vcsnx.nro without the game's translated code, and vcs_game.so with
# it. The fingerprint of the headers the translated code is compiled against goes into both. No -lgcc in the
# module: what it needs of libgcc (_Unwind_Resume) is imported, so one unwinder serves runtime and module.
enable_language(ASM)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(VCSNX_NM aarch64-none-elf-nm HINTS "$ENV{DEVKITPRO}/devkitA64/bin" /opt/devkitpro/devkitA64/bin REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/module_abi.py" "${CMAKE_CXX_COMPILER}" "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE vcsnx_abi OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/module_abi.py" "${CMAKE_CXX_COMPILER}" "${CMAKE_SOURCE_DIR}" --list
    OUTPUT_VARIABLE vcsnx_abi_headers OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${vcsnx_abi_headers})
set(VCSNX_MODULE_ABI "${vcsnx_abi}" CACHE INTERNAL "Game module interface fingerprint (tools/module_abi.py)")
message(STATUS "VCSNX: game module interface ${VCSNX_MODULE_ABI}")

# The module's own entry points, compiled like the translated code.
add_library(vcsnx_module_entry OBJECT "${VCSNX_PLATFORM_DIR}/module_entry.cpp")
target_include_directories(vcsnx_module_entry PRIVATE "${VCSNX_GENERATED_DIR}" "${VCSNX_PROFILE_DIR}/host")
target_link_libraries(vcsnx_module_entry PRIVATE psprecomp_core)
target_compile_definitions(vcsnx_module_entry PRIVATE VCSNX_MODULE_ABI="${VCSNX_MODULE_ABI}")
set_target_properties(vcsnx_module_entry PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Both objects ship in the Builder's kit: without debug info, so the build machine's folders stay out of the
# release (tools/package_builder.py refuses any file that carries them).
target_compile_options(vcsnx_module_entry PRIVATE -g0)
# The end of the module's .eh_frame, linked last.
add_library(vcsnx_module_eh_end OBJECT "${VCSNX_PLATFORM_DIR}/module_eh_end.S")
target_compile_options(vcsnx_module_eh_end PRIVATE -g0)

set(VCSNX_GAME_MODULE_FILE "${CMAKE_BINARY_DIR}/vcs_game.so")
add_custom_command(OUTPUT "${VCSNX_GAME_MODULE_FILE}"
    COMMAND "${CMAKE_CXX_COMPILER}" -march=armv8-a+crc+crypto -mtune=cortex-a57 -fPIC -shared -nostdlib
            -Wl,-Bsymbolic -Wl,--build-id=sha1 $<TARGET_OBJECTS:vcsnx_aot> $<TARGET_OBJECTS:vcsnx_module_entry>
            $<TARGET_OBJECTS:vcsnx_module_eh_end> -o "${VCSNX_GAME_MODULE_FILE}"
    DEPENDS vcsnx_aot vcsnx_module_entry vcsnx_module_eh_end $<TARGET_OBJECTS:vcsnx_aot>
            $<TARGET_OBJECTS:vcsnx_module_entry> $<TARGET_OBJECTS:vcsnx_module_eh_end>
    COMMAND_EXPAND_LISTS VERBATIM)
add_custom_target(vcs_game_module ALL DEPENDS "${VCSNX_GAME_MODULE_FILE}")

set(VCSNX_GAME_MODULE_IMPORTS "${CMAKE_BINARY_DIR}/game_module_imports.S")
add_custom_command(OUTPUT "${VCSNX_GAME_MODULE_IMPORTS}"
    COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/make_module_imports.py" "${VCSNX_NM}"
            "${VCSNX_GAME_MODULE_FILE}" "${VCSNX_GAME_MODULE_IMPORTS}"
    DEPENDS "${VCSNX_GAME_MODULE_FILE}" "${CMAKE_SOURCE_DIR}/tools/make_module_imports.py" VERBATIM)

set(VCSNX_GAME_MODULE_RUNTIME_SOURCES
    "${VCSNX_PLATFORM_DIR}/elf_module.cpp" "${VCSNX_PLATFORM_DIR}/game_module.cpp"
    "${VCSNX_PLATFORM_DIR}/game_module_bridge.cpp" "${VCSNX_GAME_MODULE_IMPORTS}")
