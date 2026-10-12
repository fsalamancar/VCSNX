#pragma once
#include <cstddef>
#include <cstdint>

namespace vcsnx {

// Geometry one GE frame bucket may collect between two vblanks. Buckets are emptied only when the
// game waits for a vblank, so a stretch of drawing without one (Switch crash on 2026-10-11, right
// after leaving a menu) grew a bucket's vertex vector until operator new failed and the GE thread
// called std::terminate. The hardware-transform buffers hold 8 MiB each and a frame past that skips
// all its hardware draws anyway; the screen-space buffer grows, so it gets a generous ceiling.
inline constexpr std::size_t kHwBucketBytes = std::size_t{8} << 20;
inline constexpr std::size_t kScreenBucketBytes = std::size_t{64} << 20;

// Whether `add_bytes` more fit under `limit_bytes` next to `used_bytes`, without overflowing.
[[nodiscard]] bool bucket_has_room(std::size_t used_bytes, std::size_t add_bytes, std::size_t limit_bytes) noexcept;

// Logs the first few dropped draws (stderr), with what was full: the next crash report has its cause.
void note_bucket_full(const char *kind, std::uint32_t target_address, std::size_t used_bytes,
                      std::size_t add_bytes) noexcept;

} // namespace vcsnx
