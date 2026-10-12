#pragma once
// Test double for ge_gpu_backend.hpp (tests/platform/fake_gpu_backend.cpp): an active Vulkan-like
// backend that accepts hardware-transform draws and keeps their vertices.
#include "ge_gpu_backend.hpp"

#include <vector>

namespace vcsnx_test {
// Every vertex handed to ge_gpu_backend_accumulate_hardware_triangles, in order.
std::vector<vcs::GeGpuVertex> &fake_hw_vertices();
} // namespace vcsnx_test
