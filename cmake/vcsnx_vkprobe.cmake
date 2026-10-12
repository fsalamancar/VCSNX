# vkprobe.nro (Plan 2, Task 3): validates NVK on the console before the game uses it.
add_executable(vkprobe "${CMAKE_SOURCE_DIR}/tools/vkprobe/vkprobe.cpp")
vcsnx_link_mesa_vulkan(vkprobe)
nx_generate_nacp(vkprobe.nacp NAME "VCSNX Vulkan probe" AUTHOR "VCSNX" VERSION "${PROJECT_VERSION}")
nx_create_nro(vkprobe NACP vkprobe.nacp)
