#include "guest_io.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace vcsnx {
namespace {

constexpr std::size_t kPage = 0x1000u;
// Whole pages: no other object shares a page with the buffer, so the pages the FS service borrows
// are this thread's alone. 1 MiB keeps a world-streaming read to a few FS calls.
constexpr std::size_t kBounceBytes = std::size_t{1} << 20;

struct FreeDeleter {
    void operator()(std::uint8_t *memory) const noexcept { std::free(memory); }
};

std::uint8_t *bounce_buffer() {
    static thread_local std::unique_ptr<std::uint8_t, FreeDeleter> buffer(
        static_cast<std::uint8_t *>(std::aligned_alloc(kPage, kBounceBytes)));
    return buffer.get();
}

} // namespace

std::size_t read_into_guest(std::istream &input, std::uint8_t *destination, std::size_t size) {
    std::uint8_t *bounce = bounce_buffer();
    if (bounce == nullptr) { // no memory for the buffer: read as before rather than fail the game's read
        input.read(reinterpret_cast<char *>(destination), static_cast<std::streamsize>(size));
        return static_cast<std::size_t>(input.gcount());
    }
    std::size_t total = 0u;
    while (total < size) {
        const std::size_t wanted = std::min(kBounceBytes, size - total);
        input.read(reinterpret_cast<char *>(bounce), static_cast<std::streamsize>(wanted));
        const auto got = static_cast<std::size_t>(input.gcount());
        std::memcpy(destination + total, bounce, got);
        total += got;
        if (got != wanted) break;
    }
    return total;
}

} // namespace vcsnx
