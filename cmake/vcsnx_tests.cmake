# VCSNX's own unit tests (Mac only). Each test is a plain executable using tests/test_support.hpp.
function(vcsnx_add_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE
        "${CMAKE_SOURCE_DIR}/tests" "${VCSNX_PLATFORM_DIR}" "${VCSNX_PROFILE_DIR}/host")
    target_link_libraries(${name} PRIVATE psprecomp_core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

vcsnx_add_test(test_threads tests/platform/test_threads.cpp platform/switch/threads.cpp)
vcsnx_add_test(test_cpu_clock tests/platform/test_cpu_clock.cpp platform/switch/cpu_clock.cpp)
vcsnx_add_test(test_gpu_frame_hold tests/platform/test_gpu_frame_hold.cpp platform/switch/gpu_frame_hold.cpp)
vcsnx_add_test(test_copy_present tests/platform/test_copy_present.cpp platform/switch/copy_present.cpp)
vcsnx_add_test(test_hw_verify tests/platform/test_hw_verify.cpp platform/switch/hw_verify.cpp)
vcsnx_add_test(test_hud_scale tests/platform/test_hud_scale.cpp platform/switch/hud_scale.cpp)
vcsnx_add_test(test_frame_budget tests/platform/test_frame_budget.cpp platform/switch/frame_budget.cpp)
vcsnx_add_test(test_frame_limiter tests/platform/test_frame_limiter.cpp)
vcsnx_add_test(test_present_phases tests/platform/test_present_phases.cpp platform/switch/present_phases.cpp)
vcsnx_add_test(test_ge_timeline tests/platform/test_ge_timeline.cpp platform/switch/ge_timeline.cpp)
vcsnx_add_test(test_async_log tests/platform/test_async_log.cpp platform/switch/async_log.cpp)
vcsnx_add_test(test_crash_log tests/platform/test_crash_log.cpp platform/switch/crash_log.cpp)
vcsnx_add_test(test_guest_io tests/platform/test_guest_io.cpp platform/switch/guest_io.cpp)
vcsnx_add_test(test_bucket_recycler tests/platform/test_bucket_recycler.cpp)
vcsnx_add_test(test_audio_resample tests/platform/test_audio_resample.cpp platform/switch/audio_resample.cpp)
vcsnx_add_test(test_alloc_diag tests/platform/test_alloc_diag.cpp platform/switch/alloc_diag.cpp)
vcsnx_add_test(test_block_cache tests/platform/test_block_cache.cpp)
vcsnx_add_test(test_ram_files tests/platform/test_ram_files.cpp platform/switch/ram_files.cpp)
vcsnx_add_test(test_main_watchdog tests/platform/test_main_watchdog.cpp platform/switch/main_watchdog.cpp)
vcsnx_add_test(test_ge_slow_log tests/platform/test_ge_slow_log.cpp platform/switch/ge_slow_log.cpp)
vcsnx_add_test(test_range_allocator tests/platform/test_range_allocator.cpp platform/switch/range_allocator.cpp)
vcsnx_add_test(test_ge_filters tests/platform/test_ge_filters.cpp platform/switch/ge_filters.cpp)
vcsnx_add_test(test_boot_splash tests/platform/test_boot_splash.cpp platform/switch/boot_splash.cpp)
vcsnx_add_test(test_pad_mapping tests/platform/test_pad_mapping.cpp platform/switch/pad_mapping.cpp)
vcsnx_add_test(test_perf_log tests/platform/test_perf_log.cpp platform/switch/perf_log.cpp)
vcsnx_add_test(test_log_files tests/platform/test_log_files.cpp platform/switch/log_files.cpp)
vcsnx_add_test(test_memory_watch tests/platform/test_memory_watch.cpp platform/switch/memory_watch.cpp)
vcsnx_add_test(test_stderr_tee tests/platform/test_stderr_tee.cpp platform/switch/stderr_tee.cpp
    platform/switch/async_log.cpp platform/switch/perf_log.cpp platform/switch/memory_usage.cpp platform/switch/log_files.cpp)
vcsnx_add_test(test_startup_rules tests/platform/test_startup_rules.cpp platform/switch/startup_rules.cpp)
vcsnx_add_test(test_memory_usage tests/platform/test_memory_usage.cpp platform/switch/memory_usage.cpp)
vcsnx_add_test(test_present_math tests/platform/test_present_math.cpp)
vcsnx_add_test(test_gpu_backend_null tests/platform/test_gpu_backend_null.cpp platform/switch/ge_gpu_backend_null.cpp)
vcsnx_add_test(test_runtime_paths tests/platform/test_runtime_paths.cpp)

# The game module loader's pure part (platform/switch/elf_module) against a devkitA64 shared object.
find_program(VCSNX_A64_GCC aarch64-none-elf-gcc HINTS /opt/devkitpro/devkitA64/bin)
if(VCSNX_A64_GCC)
    set(VCSNX_FIXTURE_MODULE "${CMAKE_BINARY_DIR}/fixture_module.so")
    add_custom_command(OUTPUT "${VCSNX_FIXTURE_MODULE}"
        COMMAND "${VCSNX_A64_GCC}" -O2 -fPIC -shared -nostdlib -Wl,-Bsymbolic
                "${CMAKE_SOURCE_DIR}/tests/fixtures/fixture_module.c" "${CMAKE_SOURCE_DIR}/platform/switch/module_eh_end.S"
                -o "${VCSNX_FIXTURE_MODULE}"
        DEPENDS "${CMAKE_SOURCE_DIR}/tests/fixtures/fixture_module.c" "${CMAKE_SOURCE_DIR}/platform/switch/module_eh_end.S"
        VERBATIM)
    set(VCSNX_FIXTURE_MODULE_UNTERMINATED "${CMAKE_BINARY_DIR}/fixture_module_unterminated.so")
    add_custom_command(OUTPUT "${VCSNX_FIXTURE_MODULE_UNTERMINATED}"
        COMMAND "${VCSNX_A64_GCC}" -O2 -fPIC -shared -nostdlib -Wl,-Bsymbolic
                "${CMAKE_SOURCE_DIR}/tests/fixtures/fixture_module.c" -o "${VCSNX_FIXTURE_MODULE_UNTERMINATED}"
        DEPENDS "${CMAKE_SOURCE_DIR}/tests/fixtures/fixture_module.c" VERBATIM)
    add_custom_target(fixture_module DEPENDS "${VCSNX_FIXTURE_MODULE}" "${VCSNX_FIXTURE_MODULE_UNTERMINATED}")
    vcsnx_add_test(test_elf_module tests/platform/test_elf_module.cpp platform/switch/elf_module.cpp)
    add_dependencies(test_elf_module fixture_module)
    target_compile_definitions(test_elf_module PRIVATE VCSNX_FIXTURE_MODULE="${VCSNX_FIXTURE_MODULE}"
        VCSNX_FIXTURE_MODULE_UNTERMINATED="${VCSNX_FIXTURE_MODULE_UNTERMINATED}")
else()
    message(STATUS "VCSNX: no devkitA64, test_elf_module skipped")
endif()
vcsnx_add_test(test_reflection_map tests/platform/test_reflection_map.cpp platform/switch/reflection_map.cpp)
