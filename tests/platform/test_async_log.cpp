// Log lines are written to their files by a background thread, in order and all of them.
#include "async_log.hpp"
#include "test_support.hpp"

#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string contents(std::FILE *file) {
    std::fflush(file);
    std::rewind(file);
    std::string text;
    char buffer[4096];
    for (std::size_t got; (got = std::fread(buffer, 1, sizeof(buffer), file)) > 0;) text.append(buffer, got);
    return text;
}

} // namespace

int main() {
    using vcsnx_test::require;
    return vcsnx_test::run({
        {"lines reach the file in order once flushed", [] {
             std::FILE *file = std::tmpfile();
             require(file != nullptr, "tmpfile");
             vcsnx::AsyncLineWriter writer;
             for (int i = 0; i < 200; ++i) writer.write(file, "line " + std::to_string(i));
             writer.flush();
             std::string expected;
             for (int i = 0; i < 200; ++i) expected += "line " + std::to_string(i) + "\n";
             require(contents(file) == expected, "200 lines in order");
             std::fclose(file);
         }},
        {"two files keep their own lines", [] {
             std::FILE *log = std::tmpfile();
             std::FILE *perf = std::tmpfile();
             vcsnx::AsyncLineWriter writer;
             writer.write(log, "a");
             writer.write(perf, "summary");
             writer.write(log, "b");
             writer.flush();
             require(contents(log) == "a\nb\n", "log");
             require(contents(perf) == "summary\n", "perf");
             std::fclose(log);
             std::fclose(perf);
         }},
        {"lines from several threads all arrive", [] {
             std::FILE *file = std::tmpfile();
             vcsnx::AsyncLineWriter writer;
             std::vector<std::thread> threads;
             for (int t = 0; t < 4; ++t)
                 threads.emplace_back([&writer, file] {
                     for (int i = 0; i < 250; ++i) writer.write(file, "x");
                 });
             for (std::thread &thread : threads) thread.join();
             writer.flush();
             require(contents(file).size() == 1000u * 2u, "1000 lines of 'x\\n'");
             std::fclose(file);
         }},
        {"what is queued is written when the writer stops", [] {
             std::FILE *file = std::tmpfile();
             {
                 vcsnx::AsyncLineWriter writer;
                 for (int i = 0; i < 50; ++i) writer.write(file, "last");
             }
             require(contents(file).size() == 50u * 5u, "50 lines");
             std::fclose(file);
         }},
        {"no file, no write", [] {
             vcsnx::AsyncLineWriter writer;
             writer.write(nullptr, "dropped");
             writer.flush();
         }},
    });
}
