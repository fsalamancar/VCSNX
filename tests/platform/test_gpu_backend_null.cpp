#include "ge_gpu_backend.hpp"
#include "test_support.hpp"

#include <string>

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"initialisation succeeds in software mode (main.cpp throws on false)", [] {
             std::string error;
             require(vcs::initialize_ge_gpu_backend(error), "initialize must succeed: " + error);
             require(error.empty(), "no error text on success: " + error);
             require(!vcs::ge_gpu_backend_active(), "no GPU is ever active");
         }},
        {"report says software was requested and is active", [] {
             const vcs::GeGpuBackendReport report = vcs::ge_gpu_backend_report();
             require(report.requested == vcs::GeGpuBackendKind::Software, "requested");
             require(report.active == vcs::GeGpuBackendKind::Software, "active");
             require(!report.message.empty(), "status message for VCSNative.log");
         }},
    });
}
