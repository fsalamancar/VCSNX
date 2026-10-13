// modprobe's test module (built -shared -nostdlib -Wl,-Bsymbolic, like vcs_game.so): calls into the
// runtime, and exceptions both ways across the boundary, with a destructor run while unwinding.
#include <stdexcept>

int modprobe_host_add(int a, int b);  // defined in modprobe.nro
void modprobe_host_throw(int code);   // defined in modprobe.nro: throws std::runtime_error

namespace {
struct Cleanup {
    int &count;
    ~Cleanup() { ++count; }
};
} // namespace

extern "C" __attribute__((visibility("default"))) int modtest_add(int a, int b) { return modprobe_host_add(a, b) * 2; }

extern "C" __attribute__((visibility("default"))) int modtest_catch_host_throw() {
    int cleanups = 0;
    try {
        Cleanup cleanup{cleanups};
        modprobe_host_throw(7);
    } catch (const std::runtime_error &) {
        return 100 + cleanups;
    }
    return -1;
}

extern "C" __attribute__((visibility("default"))) void modtest_throw() { throw std::runtime_error("from the module"); }

// No catch here: after the destructor the cleanup calls _Unwind_Resume, which must be the runtime's
// unwinder (it knows the module's frames), not a private copy from libgcc.a.
extern "C" __attribute__((visibility("default"))) void modtest_cleanup_through(int *cleanups) {
    Cleanup cleanup{*cleanups};
    modprobe_host_throw(9);
}

extern "C" __attribute__((visibility("default"))) const char modtest_abi[] = "modprobe-1";
