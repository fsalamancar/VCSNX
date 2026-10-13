// A GE list that takes long is logged with what it did meanwhile; pipeline compiles are counted.
#include "ge_slow_log.hpp"
#include "test_support.hpp"

#include <chrono>
#include <string>

namespace {
bool has(const std::string &text, const std::string &part) { return text.find(part) != std::string::npos; }
} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"only lists over the threshold are slow", [] {
             require(!vcsnx::is_slow_list(49'999u), "under 50 ms");
             require(vcsnx::is_slow_list(50'000u), "50 ms");
         }},
        {"the line says how long and what happened inside", [] {
             vcsnx::SlowListWork work;
             work.list_us = 214'000u;
             work.pipelines = 2u;
             work.pipeline_us = 180'500u;
             work.texture_uploads = 37u;
             work.evictions = 3u;
             work.flush_wait_us = 4'200u;
             work.texture_us = 150'300u;
             work.decode_us = 6'000u;
             work.alloc_us = 120'000u;
             work.objects_us = 9'500u;
             char out[256];
             const std::string text(out, vcsnx::format_slow_list(work, out, sizeof out));
             require(has(text, "[ge-slow] list_ms=214.0"), text);
             require(has(text, "pipelines=2 pipeline_ms=180.5"), text);
             require(has(text, "tex_uploads=37 evictions=3 flush_wait_ms=4.2"), text);
             require(has(text, "tex_ms=150.3 decode_ms=6.0 alloc_ms=120.0 objects_ms=9.5"), text);
         }},
        {"pipeline compiles add up", [] {
             const vcsnx::PipelineStats before = vcsnx::pipeline_stats();
             vcsnx::count_pipeline_compile(120'000u);
             vcsnx::count_pipeline_compile(30'000u);
             const vcsnx::PipelineStats after = vcsnx::pipeline_stats();
             require(after.count - before.count == 2u, "two compiles");
             require(after.total_us - before.total_us == 150'000u, "their time");
         }},
        {"texture step times add up per step", [] {
             const vcsnx::TextureTimes before = vcsnx::texture_times();
             vcsnx::count_texture_step(vcsnx::TextureStep::Block, 2'000'000u);
             vcsnx::count_texture_step(vcsnx::TextureStep::Decode, 100'000u);
             vcsnx::count_texture_step(vcsnx::TextureStep::Alloc, 1'500'000u);
             vcsnx::count_texture_step(vcsnx::TextureStep::Alloc, 500'000u);
             vcsnx::count_texture_step(vcsnx::TextureStep::Objects, 300'000u);
             const vcsnx::TextureTimes after = vcsnx::texture_times();
             require(after.block_ns - before.block_ns == 2'000'000u, "block");
             require(after.decode_ns - before.decode_ns == 100'000u, "decode");
             require(after.alloc_ns - before.alloc_ns == 2'000'000u, "alloc, twice");
             require(after.objects_ns - before.objects_ns == 300'000u, "objects");
         }},
        {"a step timer counts the time it lives", [] {
             const vcsnx::TextureTimes before = vcsnx::texture_times();
             {
                 const vcsnx::StepTimer step(vcsnx::TextureStep::Objects);
                 const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
                 while (std::chrono::steady_clock::now() < until) {}
             }
             const vcsnx::TextureTimes after = vcsnx::texture_times();
             require(after.objects_ns - before.objects_ns >= 2'000'000u, "at least 2 ms");
             require(after.block_ns == before.block_ns, "only its own step");
         }},
    });
}
