# The VCSNX game executable and the vendor's profile test suites (spec §4, §9).
include("${CMAKE_CURRENT_LIST_DIR}/vcsnx_vendor_sources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/vcsnx_splash.cmake")

set(VCSNX_PLATFORM_SOURCES
    "${VCSNX_PLATFORM_DIR}/cpu_clock.cpp"
    "${VCSNX_PLATFORM_DIR}/copy_present.cpp"
    "${VCSNX_PLATFORM_DIR}/display_window_sdl.cpp"
    "${VCSNX_PLATFORM_DIR}/frame_budget.cpp"
    "${VCSNX_PLATFORM_DIR}/ge_filters.cpp"
    "${VCSNX_PLATFORM_DIR}/gpu_frame_hold.cpp"
    "${VCSNX_PLATFORM_DIR}/hud_scale.cpp"
    "${VCSNX_PLATFORM_DIR}/hw_verify.cpp"
    "${VCSNX_PLATFORM_DIR}/memory_usage.cpp"
    "${VCSNX_PLATFORM_DIR}/pad_mapping.cpp"
    "${VCSNX_PLATFORM_DIR}/log_files.cpp"
    "${VCSNX_PLATFORM_DIR}/memory_watch.cpp"
    "${VCSNX_PLATFORM_DIR}/perf_log.cpp"
    "${VCSNX_PLATFORM_DIR}/platform.cpp"
    "${VCSNX_PLATFORM_DIR}/present_phases.cpp"
    "${VCSNX_PLATFORM_DIR}/startup_rules.cpp"
    "${VCSNX_PLATFORM_DIR}/stderr_tee.cpp"
    "${VCSNX_PLATFORM_DIR}/switch_prompts.cpp"
    "${VCSNX_PLATFORM_DIR}/threads.cpp")
if(VCSNX_TARGET_SWITCH)
    list(APPEND VCSNX_PLATFORM_SOURCES
        "${VCSNX_PLATFORM_DIR}/nx_pad_source.cpp" "${VCSNX_PLATFORM_DIR}/nx_startup.cpp")
else()
    list(APPEND VCSNX_PLATFORM_SOURCES "${VCSNX_PLATFORM_DIR}/pad_source_sdl.cpp")
endif()
list(APPEND VCSNX_PLATFORM_SOURCES ${VCSNX_SPLASH_SOURCES})
# VCSNX_GPU_BACKEND_SOURCES comes from cmake/vcsnx_vulkan.cmake (vulkan or null).
set(VCSNX_HOST_SOURCES ${VCSNX_VENDOR_HOST_SOURCES} ${VCSNX_PLATFORM_SOURCES} ${VCSNX_GPU_BACKEND_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(VCSNX_SDL2 REQUIRED sdl2)
pkg_check_modules(VCSNX_FFMPEG REQUIRED libavformat libavcodec libswscale libswresample libavutil)
if(APPLE)
    find_library(VCSNX_COREGRAPHICS CoreGraphics REQUIRED) # vcs_config.cpp's Desktop resolution query
endif()
set(VCSNX_SDL2_SWITCH_LDFLAGS ${VCSNX_SDL2_STATIC_LDFLAGS})
if(VCSNX_TARGET_SWITCH AND VCSNX_GPU_BACKEND STREQUAL "vulkan")
    # SDL2 draws through switch-mesa's EGL; use the copy whose Mesa symbols do not clash with mesa-switch.
    list(TRANSFORM VCSNX_SDL2_SWITCH_LDFLAGS REPLACE "^-lEGL$" "${VCSNX_LEGACY_EGL}")
endif()

function(vcsnx_configure_host target)
    target_include_directories(${target} PRIVATE
        "${VCSNX_PROFILE_DIR}/host" "${VCSNX_GENERATED_DIR}" "${VCSNX_PLATFORM_DIR}"
        ${VCSNX_SDL2_INCLUDE_DIRS} ${VCSNX_FFMPEG_INCLUDE_DIRS})
    target_compile_definitions(${target} PRIVATE VCSNX=1 VCSNX_MAIN_THREAD_PUMP=1)
    if(VCSNX_HAS_SPLASH)
        target_compile_definitions(${target} PRIVATE VCSNX_HAS_SPLASH=1)
    endif()
    # Same float semantics as the vendor profile (profiles/vcs/CMakeLists.txt): GCC's C++ default
    # -ffp-contract=fast fuses a*b+c across statements into fmadd, so guest math and the
    # bit-exact rasterizer would diverge from the PSP/MSVC results the corpus was validated with.
    target_compile_options(${target} PRIVATE ${VCSNX_SDL2_CFLAGS_OTHER} ${VCSNX_FFMPEG_CFLAGS_OTHER} -ffp-contract=off)
    target_link_libraries(${target} PRIVATE psprecomp_core)
    if(VCSNX_TARGET_SWITCH)
        # -std=c++20 (no GNU extensions) defines __STRICT_ANSI__, and newlib then hides POSIX
        # functions such as setenv(); _GNU_SOURCE makes them visible again.
        target_compile_definitions(${target} PRIVATE VCSNX_SDL_AUDIO=1 VCSNX_DATA_DIR="sdmc:/switch/vcsnx" _GNU_SOURCE)
        target_link_libraries(${target} PRIVATE ${VCSNX_FFMPEG_STATIC_LDFLAGS} ${VCSNX_SDL2_SWITCH_LDFLAGS})
    else()
        # Absolute paths: with bare names, /opt/homebrew/lib (from SDL2) would win over the keg-only
        # ffmpeg@7 and link a different FFmpeg major than the headers were compiled against.
        target_link_libraries(${target} PRIVATE ${VCSNX_SDL2_LINK_LIBRARIES} ${VCSNX_FFMPEG_LINK_LIBRARIES})
        if(APPLE)
            target_link_libraries(${target} PRIVATE ${VCSNX_COREGRAPHICS})
        endif()
    endif()
endfunction()

file(GLOB VCSNX_GENERATED_SOURCES CONFIGURE_DEPENDS "${VCSNX_GENERATED_DIR}/*.cpp")
list(LENGTH VCSNX_GENERATED_SOURCES VCSNX_GENERATED_COUNT)
if(NOT VCSNX_GENERATED_COUNT EQUAL 235)
    message(FATAL_ERROR "Expected 235 regenerated .cpp files in ${VCSNX_GENERATED_DIR}, found "
                        "${VCSNX_GENERATED_COUNT}. Run tools/restore_generated.sh <your decrypted ELF> first (docs/regen-report.md).")
endif()
set(VCSNX_GENERATED_OPT "2" CACHE STRING "Optimization level for generated AOT units (0-3)")
add_library(vcsnx_aot OBJECT ${VCSNX_GENERATED_SOURCES})
target_include_directories(vcsnx_aot PRIVATE "${VCSNX_GENERATED_DIR}" "${VCSNX_PROFILE_DIR}/host")
target_link_libraries(vcsnx_aot PRIVATE psprecomp_core)
target_compile_options(vcsnx_aot PRIVATE "-O${VCSNX_GENERATED_OPT}" -g0 -w -ffp-contract=off)
option(VCSNX_GAME_MODULE "Switch: the game's translated code in vcs_game.so, loaded by vcsnx.nro (spec v1 §2)" ${VCSNX_TARGET_SWITCH})
if(VCSNX_GAME_MODULE)
    set_target_properties(vcsnx_aot PROPERTIES POSITION_INDEPENDENT_CODE ON)
    include("${CMAKE_CURRENT_LIST_DIR}/vcsnx_game_module.cmake")
endif()

if(VCSNX_GAME_MODULE)
    add_executable(vcsnx "${VCSNX_PROFILE_DIR}/host/main.cpp" ${VCSNX_HOST_SOURCES} ${VCSNX_GAME_MODULE_RUNTIME_SOURCES})
    target_compile_definitions(vcsnx PRIVATE VCSNX_GAME_MODULE=1 VCSNX_MODULE_ABI="${VCSNX_MODULE_ABI}")
else()
    add_executable(vcsnx "${VCSNX_PROFILE_DIR}/host/main.cpp" ${VCSNX_HOST_SOURCES} $<TARGET_OBJECTS:vcsnx_aot>)
endif()
vcsnx_configure_host(vcsnx)
vcsnx_configure_gpu_backend(vcsnx)
if(VCSNX_TARGET_SWITCH AND VCSNX_GPU_BACKEND STREQUAL "vulkan")
    # C11 threads for the legacy EGL copy (tools/build_mesa_switch.sh renames its references).
    target_sources(vcsnx PRIVATE "${VCSNX_PLATFORM_DIR}/legacy_c11_shim.c")
endif()

if(NOT VCSNX_TARGET_SWITCH)
    add_executable(vcs_profile_tests "${VCSNX_PROFILE_DIR}/tests/vcs_profile_tests.cpp" ${VCSNX_HOST_SOURCES})
    vcsnx_configure_host(vcs_profile_tests)
    vcsnx_configure_gpu_backend(vcs_profile_tests)
    add_test(NAME vcs_profile_tests COMMAND vcs_profile_tests)

    # Backlog bugs 1 and 3: thread stacks freed by sceKernelExitDeleteThread (the HLE needs the whole host).
    add_executable(test_thread_stacks "${CMAKE_SOURCE_DIR}/tests/platform/test_thread_stacks.cpp" ${VCSNX_HOST_SOURCES})
    vcsnx_configure_host(test_thread_stacks)
    vcsnx_configure_gpu_backend(test_thread_stacks)
    target_include_directories(test_thread_stacks PRIVATE "${CMAKE_SOURCE_DIR}/tests")
    add_test(NAME test_thread_stacks COMMAND test_thread_stacks)

    add_executable(vcs_config_tests "${VCSNX_PROFILE_DIR}/tests/vcs_config_tests.cpp"
        "${VCSNX_PROFILE_DIR}/host/vcs_config.cpp" "${VCSNX_PROFILE_DIR}/host/vcs_camera_input.cpp"
        "${VCSNX_PROFILE_DIR}/host/vcs_vehicle_input.cpp")
    target_include_directories(vcs_config_tests PRIVATE "${VCSNX_PROFILE_DIR}/host")
    if(APPLE)
        target_link_libraries(vcs_config_tests PRIVATE ${VCSNX_COREGRAPHICS})
    endif()
    add_test(NAME vcs_config_tests COMMAND vcs_config_tests)

    add_executable(test_frame_rate_config "${CMAKE_SOURCE_DIR}/tests/platform/test_frame_rate_config.cpp"
        "${VCSNX_PROFILE_DIR}/host/vcs_config.cpp" "${VCSNX_PROFILE_DIR}/host/vcs_camera_input.cpp"
        "${VCSNX_PROFILE_DIR}/host/vcs_vehicle_input.cpp")
    target_include_directories(test_frame_rate_config PRIVATE "${VCSNX_PROFILE_DIR}/host" "${CMAKE_SOURCE_DIR}/tests")
    target_compile_definitions(test_frame_rate_config PRIVATE VCSNX=1)
    if(APPLE)
        target_link_libraries(test_frame_rate_config PRIVATE ${VCSNX_COREGRAPHICS})
    endif()
    add_test(NAME test_frame_rate_config COMMAND test_frame_rate_config)

    # [Display] HudScale (smaller HUD on the Switch).
    add_executable(test_hud_scale_config "${CMAKE_SOURCE_DIR}/tests/platform/test_hud_scale_config.cpp"
        "${VCSNX_PROFILE_DIR}/host/vcs_config.cpp" "${VCSNX_PROFILE_DIR}/host/vcs_camera_input.cpp"
        "${VCSNX_PROFILE_DIR}/host/vcs_vehicle_input.cpp")
    target_include_directories(test_hud_scale_config PRIVATE "${VCSNX_PROFILE_DIR}/host" "${CMAKE_SOURCE_DIR}/tests")
    target_compile_definitions(test_hud_scale_config PRIVATE VCSNX=1)
    if(APPLE)
        target_link_libraries(test_hud_scale_config PRIVATE ${VCSNX_COREGRAPHICS})
    endif()
    add_test(NAME test_hud_scale_config COMMAND test_hud_scale_config)

    add_executable(audio_resampler_tests "${VCSNX_PROFILE_DIR}/tests/audio_resampler_tests.cpp")
    target_include_directories(audio_resampler_tests PRIVATE "${VCSNX_PROFILE_DIR}/host")
    add_test(NAME audio_resampler_tests COMMAND audio_resampler_tests)

    add_executable(vcs_bootstrap_paths_tests "${VCSNX_PROFILE_DIR}/tests/vcs_bootstrap_paths_tests.cpp"
        "${VCSNX_PROFILE_DIR}/host/vcs_bootstrap_paths.cpp")
    target_include_directories(vcs_bootstrap_paths_tests PRIVATE "${VCSNX_PROFILE_DIR}/host")
    target_link_libraries(vcs_bootstrap_paths_tests PRIVATE psprecomp_core)
    add_test(NAME vcs_bootstrap_paths_tests COMMAND vcs_bootstrap_paths_tests)

    # Needs SDL2 (the SDL audio branch), so it is registered here rather than in vcsnx_tests.cmake.
    add_executable(test_audio_shutdown "${CMAKE_SOURCE_DIR}/tests/platform/test_audio_shutdown.cpp"
        "${VCSNX_PROFILE_DIR}/host/audio_output.cpp" "${VCSNX_PROFILE_DIR}/host/vcs_config.cpp"
        "${VCSNX_PROFILE_DIR}/host/vcs_camera_input.cpp" "${VCSNX_PROFILE_DIR}/host/vcs_vehicle_input.cpp")
    vcsnx_configure_host(test_audio_shutdown)
    target_include_directories(test_audio_shutdown PRIVATE "${CMAKE_SOURCE_DIR}/tests")
    add_test(NAME test_audio_shutdown COMMAND test_audio_shutdown)

    # The control prompts name the Switch buttons (fork ENGLISH.GXT rewrite with the VCSNX table).
    add_executable(test_switch_prompts "${CMAKE_SOURCE_DIR}/tests/platform/test_switch_prompts.cpp"
        "${VCSNX_PROFILE_DIR}/host/vcs_key_prompts.cpp" "${VCSNX_PLATFORM_DIR}/switch_prompts.cpp")
    vcsnx_configure_host(test_switch_prompts)
    target_include_directories(test_switch_prompts PRIVATE "${CMAKE_SOURCE_DIR}/tests")
    add_test(NAME test_switch_prompts COMMAND test_switch_prompts)

    # Drives the GE renderer's hardware-transform path against a fake backend, so it links the
    # vendor host sources without a real GPU backend (Plan 3, Task 1).
    add_executable(test_hw_parallel_decode "${CMAKE_SOURCE_DIR}/tests/platform/test_hw_parallel_decode.cpp"
        "${CMAKE_SOURCE_DIR}/tests/platform/fake_gpu_backend.cpp" ${VCSNX_VENDOR_HOST_SOURCES} ${VCSNX_PLATFORM_SOURCES})
    vcsnx_configure_host(test_hw_parallel_decode)
    target_include_directories(test_hw_parallel_decode PRIVATE "${CMAKE_SOURCE_DIR}/tests" "${CMAKE_SOURCE_DIR}/tests/platform")
    add_test(NAME test_hw_parallel_decode COMMAND test_hw_parallel_decode)

    add_executable(test_media_paths "${CMAKE_SOURCE_DIR}/tests/platform/test_media_paths.cpp"
        "${VCSNX_PROFILE_DIR}/host/vcs_media_decoder.cpp")
    vcsnx_configure_host(test_media_paths)
    target_include_directories(test_media_paths PRIVATE "${CMAKE_SOURCE_DIR}/tests")
    add_test(NAME test_media_paths COMMAND test_media_paths)
endif()
