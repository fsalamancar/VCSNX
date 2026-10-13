#include "crash_log.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace vcsnx {
namespace {

// Appends to a fixed buffer; keeps it NUL-terminated and stops at its end.
class Text {
public:
    Text(char *out, std::size_t capacity) noexcept : out_(out), capacity_(capacity) {
        if (capacity_ != 0u) out_[0] = '\0';
    }
    void add(const char *format, ...) noexcept __attribute__((format(printf, 2, 3))) {
        if (capacity_ == 0u || length_ + 1u >= capacity_) return;
        va_list args;
        va_start(args, format);
        const int wrote = std::vsnprintf(out_ + length_, capacity_ - length_, format, args);
        va_end(args);
        if (wrote < 0) return;
        length_ += static_cast<std::size_t>(wrote);
        if (length_ >= capacity_) length_ = capacity_ - 1u;
    }
    [[nodiscard]] std::size_t length() const noexcept { return length_; }

private:
    char *out_;
    std::size_t capacity_;
    std::size_t length_{};
};

using ull = unsigned long long;

void add_address(Text &text, const CrashFacts &facts, std::uint64_t address) noexcept {
    if (address >= facts.runtime_base && address - facts.runtime_base < facts.runtime_size)
        text.add("vcsnx+0x%llx", static_cast<ull>(address - facts.runtime_base));
    else if (address >= facts.module_base && address - facts.module_base < facts.module_size)
        text.add("vcs_game.so+0x%llx", static_cast<ull>(address - facts.module_base));
    else
        text.add("0x%llx", static_cast<ull>(address));
}

void add_region(Text &text, const char *name, const CrashRegion &region) noexcept {
    if (!region.known) {
        text.add("%s: unknown\n", name);
        return;
    }
    text.add("%s: base=0x%llx size=0x%llx type=0x%x attr=0x%x perm=0x%x ipc=%u device=%u\n", name,
             static_cast<ull>(region.base), static_cast<ull>(region.size), region.type, region.attr, region.perm,
             region.ipc_refs, region.device_refs);
}

} // namespace

std::size_t format_crash_report(const CrashFacts &facts, char *out, std::size_t capacity) noexcept {
    Text text(out, capacity);
    text.add("[vcsnx-crash] thread=0x%llx desc=0x%x esr=0x%x far=0x%llx\n", static_cast<ull>(facts.thread_id),
             facts.error_desc, facts.esr, static_cast<ull>(facts.far));
    text.add("pc=");
    add_address(text, facts, facts.pc);
    text.add(" lr=");
    add_address(text, facts, facts.lr);
    text.add(" sp=0x%llx fp=0x%llx\n", static_cast<ull>(facts.sp), static_cast<ull>(facts.fp));
    for (int i = 0; i < 29; ++i)
        text.add("x%d=0x%llx%s", i, static_cast<ull>(facts.x[i]), (i % 4 == 3 || i == 28) ? "\n" : " ");
    add_region(text, "fault page", facts.fault_page);
    add_region(text, "page before", facts.page_before);
    add_region(text, "stack", facts.stack);
    text.add("frames:");
    for (std::uint32_t i = 0; i < facts.frame_count && i < 16u; ++i) {
        text.add(" ");
        add_address(text, facts, facts.frames[i]);
    }
    text.add("\nruntime=0x%llx module=0x%llx\n", static_cast<ull>(facts.runtime_base),
             static_cast<ull>(facts.module_base));
    return text.length();
}

#if defined(__SWITCH__)
namespace {

FsFileSystem *crash_fs = nullptr;
char crash_path[FS_MAX_PATH] = {};
std::uint64_t crash_module_base = 0, crash_module_size = 0;
char report_text[8192];

CrashRegion query(std::uint64_t address) noexcept {
    MemoryInfo info{};
    u32 page = 0;
    if (R_FAILED(svcQueryMemory(&info, &page, address))) return {};
    return {info.addr, info.size, info.type, info.attr, info.perm, info.ipc_refcount, info.device_refcount, true};
}

void write_report(const char *text, std::size_t length) noexcept {
    if (crash_fs == nullptr || crash_path[0] == '\0') return;
    fsFsDeleteFile(crash_fs, crash_path);
    if (R_FAILED(fsFsCreateFile(crash_fs, crash_path, static_cast<s64>(length), 0))) return;
    FsFile file;
    if (R_FAILED(fsFsOpenFile(crash_fs, crash_path, FsOpenMode_Write, &file))) return;
    fsFileWrite(&file, 0, text, length, FsWriteOption_Flush);
    fsFileClose(&file);
}

} // namespace

void set_crash_log_context(const char *data_dir, std::uint64_t module_base, std::uint64_t module_size) noexcept {
    crash_module_base = module_base;
    crash_module_size = module_size;
    crash_fs = fsdevGetDeviceFileSystem("sdmc");
    const char *path = std::strncmp(data_dir, "sdmc:", 5) == 0 ? data_dir + 5 : data_dir;
    std::snprintf(crash_path, sizeof crash_path, "%s/vcsnx_crash.log", path);
}

} // namespace vcsnx

extern "C" {
// libnx runs __libnx_exception_handler on this stack when a thread of the process faults.
alignas(16) u8 __nx_exception_stack[0x8000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void __libnx_exception_handler(ThreadExceptionDump *ctx) {
    // libnx calls this after the kernel has returned from the exception: returning would rerun the
    // faulting instruction. A second fault (here, or on another thread meanwhile) skips the report.
    static bool reporting = false;
    if (reporting) diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_ShouldNotHappen));
    reporting = true;
    vcsnx::CrashFacts facts;
    facts.error_desc = ctx->error_desc;
    facts.esr = ctx->esr;
    facts.far = ctx->far.x;
    facts.pc = ctx->pc.x;
    facts.lr = ctx->lr.x;
    facts.sp = ctx->sp.x;
    facts.fp = ctx->fp.x;
    for (int i = 0; i < 29; ++i) facts.x[i] = ctx->cpu_gprs[i].x;
    const vcsnx::CrashRegion runtime = vcsnx::query(reinterpret_cast<std::uint64_t>(&__libnx_exception_handler));
    facts.runtime_base = runtime.base;
    facts.runtime_size = runtime.size;
    facts.module_base = vcsnx::crash_module_base;
    facts.module_size = vcsnx::crash_module_size;
    u64 thread_id = 0;
    if (R_SUCCEEDED(svcGetThreadId(&thread_id, CUR_THREAD_HANDLE))) facts.thread_id = thread_id;
    const std::uint64_t page = facts.far & ~std::uint64_t{0xFFF};
    facts.fault_page = vcsnx::query(page);
    facts.page_before = vcsnx::query(page - 1u);
    facts.stack = vcsnx::query(facts.sp);
    // Frame records: [fp] = the caller's fp, [fp + 8] = the return address. Only readable stack is followed.
    std::uint64_t fp = facts.fp;
    while (facts.frame_count < 16u && fp != 0u && (fp & 0xFu) == 0u) {
        const vcsnx::CrashRegion region = vcsnx::query(fp);
        if (!region.known || (region.perm & Perm_R) == 0u || fp + 16u > region.base + region.size) break;
        const auto *record = reinterpret_cast<const std::uint64_t *>(fp);
        facts.frames[facts.frame_count++] = record[1];
        if (record[0] <= fp) break;
        fp = record[0];
    }
    const std::size_t length = vcsnx::format_crash_report(facts, vcsnx::report_text, sizeof vcsnx::report_text);
    vcsnx::write_report(vcsnx::report_text, length);
    // End the process: Atmosphère writes its crash report (a user break here; the fault itself is in
    // vcsnx_crash.log) and the console says the software closed, as before.
    diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_ShouldNotHappen));
}
}
#else
void set_crash_log_context(const char *, std::uint64_t, std::uint64_t) noexcept {}

} // namespace vcsnx
#endif
