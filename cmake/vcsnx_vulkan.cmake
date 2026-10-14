# GE GPU backend selection (Plan 2) and the mesa-switch (NVK) static Vulkan link for the Switch.
# VCSNX_GPU_BACKEND=vulkan builds the fork's ge_gpu_backend_vulkan.cpp with platform/generic's surface
# stub (CPU readback present into our SDL window): MoltenVK on the Mac, mesa-switch on the Switch.
# VCSNX_GPU_BACKEND=null keeps Plan 1's CPU-only build (platform/switch/ge_gpu_backend_null.cpp).
set(VCSNX_GPU_BACKEND "vulkan" CACHE STRING "GE GPU backend: vulkan or null (CPU rasterizer only)")
set_property(CACHE VCSNX_GPU_BACKEND PROPERTY STRINGS vulkan null)
if(NOT VCSNX_GPU_BACKEND MATCHES "^(vulkan|null)$")
    message(FATAL_ERROR "VCSNX_GPU_BACKEND must be vulkan or null, not '${VCSNX_GPU_BACKEND}'")
endif()

# tools/build_mesa_switch.sh stages the mesa-switch SDK.
set(VCSNX_MESA_SWITCH_SDK "${CMAKE_SOURCE_DIR}/.cache/mesa-switch-sdk" CACHE PATH "Staged mesa-switch SDK")

if(NOT DEFINED CMAKE_LINK_GROUP_USING_RESCAN_SUPPORTED)
    set(CMAKE_LINK_GROUP_USING_RESCAN "LINKER:--start-group" "LINKER:--end-group")
    set(CMAKE_LINK_GROUP_USING_RESCAN_SUPPORTED TRUE)
endif()

# Links mesa-switch: libvulkan.a FIRST (it bundles most of NVK, WSI, NIR and Mesa's utilities), then
# the component archives only for what it lacks (Rust core/alloc in libnak_rs.a, NVIDIA class
# tables, part of libnvk.a); alphabetical order pulled libnvk.a's copies first and gave duplicate
# wsi_* symbols. Plus the portlibs it needs, as one rescanned group, keeping the static ICD entry points.
function(vcsnx_link_mesa_vulkan target)
    set(VCSNX_MESA_LIBDIR "${VCSNX_MESA_SWITCH_SDK}/lib")
    if(NOT EXISTS "${VCSNX_MESA_LIBDIR}/libvulkan.a")
        message(FATAL_ERROR "No mesa-switch SDK in ${VCSNX_MESA_SWITCH_SDK}: run tools/build_mesa_switch.sh")
    endif()
    file(GLOB VCSNX_MESA_COMPONENTS "${VCSNX_MESA_LIBDIR}/*.a")
    list(REMOVE_ITEM VCSNX_MESA_COMPONENTS "${VCSNX_MESA_LIBDIR}/libvulkan.a")
    set(VCSNX_MESA_ARCHIVES "${VCSNX_MESA_LIBDIR}/libvulkan.a" ${VCSNX_MESA_COMPONENTS})
    find_library(VCSNX_EXPAT_LIBRARY expat REQUIRED)
    find_library(VCSNX_ZSTD_LIBRARY zstd REQUIRED)
    find_library(VCSNX_Z_LIBRARY z REQUIRED)
    string(JOIN "," VCSNX_MESA_GROUP ${VCSNX_MESA_ARCHIVES} ${VCSNX_EXPAT_LIBRARY} ${VCSNX_ZSTD_LIBRARY}
        ${VCSNX_Z_LIBRARY} nx stdc++ m)
    target_include_directories(${target} PRIVATE "${VCSNX_MESA_SWITCH_SDK}/include")
    target_link_libraries(${target} PRIVATE "$<LINK_GROUP:RESCAN,${VCSNX_MESA_GROUP}>")
    target_link_options(${target} PRIVATE "-Wl,-u,vk_icdGetInstanceProcAddr"
        "-Wl,-u,vk_icdNegotiateLoaderICDInterfaceVersion" -pthread)
endfunction()

if(VCSNX_GPU_BACKEND STREQUAL "null")
    set(VCSNX_GPU_BACKEND_SOURCES "${VCSNX_PLATFORM_DIR}/ge_gpu_backend_null.cpp")
    function(vcsnx_configure_gpu_backend target)
    endfunction()
    return()
endif()

set(VCSNX_GPU_BACKEND_SOURCES "${VCSNX_PROFILE_DIR}/host/ge_gpu_backend_vulkan.cpp"
    "${VCSNX_PROFILE_DIR}/platform/generic/vulkan_platform_stub.cpp")

# glslc runs on the Mac for both targets, so it is searched outside the Switch sysroot.
find_program(VCSNX_GLSLC glslc HINTS /opt/homebrew/bin NO_CMAKE_FIND_ROOT_PATH)
if(NOT VCSNX_GLSLC OR NOT EXISTS "${VCSNX_GLSLC}")
    message(FATAL_ERROR "glslc not found: brew install shaderc (needed by VCSNX_GPU_BACKEND=vulkan)")
endif()
if(VCSNX_TARGET_SWITCH)
    # switch-sdl2's EGL with the Mesa symbols mesa-switch also defines renamed (tools/build_mesa_switch.sh).
    set(VCSNX_LEGACY_EGL "${VCSNX_MESA_SWITCH_SDK}/legacy/libEGL.a")
    if(NOT EXISTS "${VCSNX_LEGACY_EGL}")
        message(FATAL_ERROR "No isolated legacy EGL at ${VCSNX_LEGACY_EGL}: run tools/build_mesa_switch.sh")
    endif()
else()
    find_package(Vulkan REQUIRED) # Homebrew vulkan-loader + vulkan-headers; MoltenVK is its ICD
endif()

# GLSL -> SPIR-V -> C header, as the fork's profiles/vcs/CMakeLists.txt does. The
# (stem, stage, symbol) triples follow the #includes in ge_gpu_backend_vulkan.cpp.
set(VCSNX_SHADER_DIR "${VCSNX_PROFILE_DIR}/host/shaders")
set(VCSNX_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/vcs_shaders")
file(MAKE_DIRECTORY "${VCSNX_SHADER_OUTPUT_DIR}")
set(VCSNX_SHADER_STEMS psp_ge psp_ge psp_ge_hw psp_ge_hw_packed0115 bloom bloom present fxaa
    color_grade cloud_march cloud_resolve cloud_composite smaa_edge smaa_weights smaa_blend sky_palette bloom_add)
set(VCSNX_SHADER_EXTS vert frag vert vert vert frag frag frag frag frag frag frag frag frag frag frag frag)
set(VCSNX_SHADER_SYMBOLS kPspGeVertSpv kPspGeFragSpv kPspGeHwVertSpv kPspGeHwPacked0115VertSpv
    kBloomVertSpv kBloomFragSpv kPresentFragSpv kFxaaFragSpv kColorGradeFragSpv
    kCloudMarchFragSpv kCloudResolveFragSpv kCloudCompositeFragSpv
    kSmaaEdgeFragSpv kSmaaWeightsFragSpv kSmaaBlendFragSpv kSkyPaletteFragSpv kBloomAddFragSpv)
set(VCSNX_SHADER_HEADERS)
foreach(stem ext symbol IN ZIP_LISTS VCSNX_SHADER_STEMS VCSNX_SHADER_EXTS VCSNX_SHADER_SYMBOLS)
    set(source "${VCSNX_SHADER_DIR}/${stem}.${ext}")
    set(header "${VCSNX_SHADER_OUTPUT_DIR}/${stem}_${ext}_spv.h")
    add_custom_command(
        OUTPUT "${header}"
        COMMAND "${VCSNX_GLSLC}" -O -mfmt=c --target-env=vulkan1.1 -I "${VCSNX_SHADER_DIR}"
                -o "${header}.inc" "${source}"
        # -mfmt=c emits a bare {0x07230203, ...} list; wrap it so the backend can #include it.
        COMMAND ${CMAKE_COMMAND} -E echo "#pragma once" > "${header}"
        COMMAND ${CMAKE_COMMAND} -E echo "#include <cstdint>" >> "${header}"
        COMMAND ${CMAKE_COMMAND} -E echo "static const std::uint32_t ${symbol}[] =" >> "${header}"
        COMMAND ${CMAKE_COMMAND} -E cat "${header}.inc" >> "${header}"
        COMMAND ${CMAKE_COMMAND} -E echo ";" >> "${header}"
        DEPENDS "${source}" "${VCSNX_SHADER_DIR}/SMAA.h"
        COMMENT "Compiling ${stem}.${ext} to SPIR-V"
        VERBATIM)
    list(APPEND VCSNX_SHADER_HEADERS "${header}")
endforeach()
# psp_ge.frag again for the hardware-transform pipelines, with their push-constant layout.
set(header "${VCSNX_SHADER_OUTPUT_DIR}/psp_ge_hw_frag_spv.h")
add_custom_command(
    OUTPUT "${header}"
    COMMAND "${VCSNX_GLSLC}" -O -mfmt=c --target-env=vulkan1.1 -DVCS_HW_PUSH_LAYOUT -I "${VCSNX_SHADER_DIR}"
            -o "${header}.inc" "${VCSNX_SHADER_DIR}/psp_ge.frag"
    COMMAND ${CMAKE_COMMAND} -E echo "#pragma once" > "${header}"
    COMMAND ${CMAKE_COMMAND} -E echo "#include <cstdint>" >> "${header}"
    COMMAND ${CMAKE_COMMAND} -E echo "static const std::uint32_t kPspGeHwFragSpv[] =" >> "${header}"
    COMMAND ${CMAKE_COMMAND} -E cat "${header}.inc" >> "${header}"
    COMMAND ${CMAKE_COMMAND} -E echo ";" >> "${header}"
    DEPENDS "${VCSNX_SHADER_DIR}/psp_ge.frag" "${VCSNX_SHADER_DIR}/SMAA.h"
    COMMENT "Compiling psp_ge.frag (hardware-transform push layout) to SPIR-V"
    VERBATIM)
list(APPEND VCSNX_SHADER_HEADERS "${header}")
add_custom_target(vcsnx_ge_shaders DEPENDS ${VCSNX_SHADER_HEADERS})

# Every target compiled with VCSNX_GPU_BACKEND_SOURCES calls this.
function(vcsnx_configure_gpu_backend target)
    target_compile_definitions(${target} PRIVATE VCSNX_GPU_VULKAN=1)
    target_include_directories(${target} PRIVATE "${VCSNX_SHADER_OUTPUT_DIR}")
    add_dependencies(${target} vcsnx_ge_shaders)
    if(VCSNX_TARGET_SWITCH)
        vcsnx_link_mesa_vulkan(${target})
    else()
        target_link_libraries(${target} PRIVATE Vulkan::Vulkan)
    endif()
endfunction()
