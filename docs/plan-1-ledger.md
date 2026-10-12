# Plan 1 execution ledger (H0–H3)

Copied from the git-ignored workspace `.superpowers/sdd/2026-10-11-vcsnx-plan-1-h0-h3/progress.md` when Plan 1 closed (2026-10-11). It records every ruling made during execution and the deferred review minors.

```text
# SDD ledger — plan: docs/superpowers/plans/2026-10-11-vcsnx-plan-1-h0-h3.md
Spec: docs/superpowers/specs/2026-10-11-vcsnx-switch-port-design.md (read).
Setup: Ruling: work on branch feat/plan-1-h0-h3 in place, not a separate worktree — the plan's scripts and user steps use ~/Documents/GitHub/VCSNX paths; branch keeps main untouched — cost if wrong: none (branch is local, mergeable).

## Pre-flight (shared interfaces)
| Producer → Consumer | Produced | Consumed | Finding |
|---|---|---|---|
| T2 → T3 | tools/lib/pinned.sh fetch_pinned, UPSTREAM_* | fetch_upstream.sh sources it | consistent |
| T3 → T5 | .cache/upstream/profiles/vcs/generated (237 files) | regen REFERENCE path | consistent |
| T4 → T5 | build/mac/bin/vcs_recomp (CMAKE_RUNTIME_OUTPUT_DIRECTORY) | regen RECOMP path | consistent |
| T5 → T9 | generated/ = 234 units + registry .cpp (=235 .cpp) + hpp + json | vcsnx_game.cmake requires exactly 235 .cpp | consistent (fallback copy also 235) |
| T6 → T9 | vcsnx_hooks.hpp, GuestThread, macros VCSNX/VCSNX_MAIN_THREAD_PUMP/VCSNX_SDL_AUDIO/VCSNX_DATA_DIR | vcsnx_configure_host defines them + include platform/switch | consistent |
| T7 → T9 | map_pad_to_psp, PadSample, kPad* | display_window_sdl.cpp, pad_source_sdl.cpp | consistent |
| T8 → T9 | install_stderr_tee, memory_used_mb | platform.cpp, stderr_tee | consistent |
| T8 → T11 | startup_problem, StartupFacts, parse_env_lines, memory_total_mb | nx_startup.cpp | consistent |
| T9 → T11 | pad_source.hpp API, data_directory(), vcsnx_game.cmake source block | nx_pad_source.cpp, nx_startup.cpp, cmake edit | consistent |
| T9/T10 → T12/T13 | config/VCSNative.ini, build/*/vcsnx(.nro), run_mac.sh | prepare_sd.sh, frame comparison | consistent |

## Tasks
Task 1: Ruling: devkitPro install (Step 4) needs the user's sudo password; Task 1 Steps 5-7 deferred until the user confirms, Tasks 2-10 (Mac only) run meanwhile — they consume nothing from devkitPro — cost if wrong: none, Task 1 commit lands later. Brew done: ffmpeg@7 7.1.5 (libavcodec 61.19.101), cmake 4.4.4.
Task 2: note: import size 6.8M/313 files (plan estimated ~30MB; the estimate included Windows FFmpeg binaries) — not a deviation.
Task 2: complete (commits a911c26..405fcec, tests: sh -c 'test ! -e vendor/vcs-recomp/profiles/vcs/generated && ! find vendor -type f \( -iname "*.dll" -o -iname "*.exe" -o -iname "*.lib" \) | grep -q . && ls vendor/vcs-recomp/profiles/vcs/tools/optimize_generated_*.py | wc -l | grep -q 3 && test -z "$(git status --porcelain vendor)" && echo vendor-guards-ok' → vendor-guards-ok)
Task 1: complete (commits 405fcec..13d94aa, tests: tools/check_toolchain.sh → ok      ff_mpegps_demuxer)
Task 3: Ruling: plan Step 3 snippet relies on bash word splitting (and 'path' is a special zsh variable tied to PATH); run it under bash -c — same commands, correct shell — cost if wrong: none.
Task 3: Ruling: fork's generator lowers MIPS 'break' as a no-op (untested: fork's published generated/ predates it). H1 will also build upstream's generator (f6e7d41) so the regenerated corpus can match upstream's byte-for-byte as the spec requires; the build uses the upstream-identical corpus — cost if wrong: a 'break' trap may halt the game where the fork's no-op would continue (same behavior as both published releases).
Task 3: complete (commits 13d94aa..ee6ee00, tests: sh -c 'tools/fetch_upstream.sh && grep -q "^- \*\*Base:\*\* fork" docs/fork-review.md && echo review-ok' → review-ok)
Task 4: complete (commits ee6ee00..a50c94d, tests: ctest --preset mac → Total Test time (real) =   0.11 sec)
Task 5: Ruling: regen_generated.sh takes the generator from VCSNX_RECOMP (default = vendor build) and a new tools/build_upstream_generator.sh builds upstream f6e7d41's generator, per the Task 3 ruling — cost if wrong: one extra 20-line script.
Task 5: hypothesis 'fork break lowering causes runaway emission' REFUTED — upstream generator also stalls after unit 0000 (killed ~300 s, RSS 1-4.5 GB, vsz ~496 GB, inside emit_function_source string growth).
Task 5: Ruling: spec §13.2 fallback applied — both generators (fork b5a861d, upstream f6e7d41) stall after unit 0000 (killed by memory pressure at ~5-7 min); generated_units.hpp regenerates identical, unit 0000 differs only in 111 local_transfers limits (published 256 vs generator 2048, so the corpus predates upstream HEAD's generator). Plan Step 7 (optimizer passes) is not applicable because generation never completes. generated/ = upstream's published corpus copied locally (still git-ignored) — cost if wrong: none for Plan 1; the reproducible-generation question moves to the future converter spec (§13.5).
Task 5: Ruling: added __pycache__/ to .gitignore (python3 -I still writes bytecode next to compare_trees.py) — cost if wrong: none.
Task 5: complete (commits a50c94d..f96cdd8, tests: sh -c 'python3 -I tools/tests/test_compare_trees.py && python3 -I tools/compare_trees.py .cache/upstream/profiles/vcs/generated generated | head -1' → identical=237 different=0 missing=0 extra=0)
Task 6: note: RED surfaced at configure time ('Cannot find source file platform/switch/threads.cpp') instead of the planned compile error — same cause (implementation missing).
Task 6: Ruling: strengthened the 8 MiB stack test (touch every page top-down) because the plan's version survived a 512 KiB-stack mutation — cost if wrong: none (stricter test).
Task 6: complete (commits f96cdd8..f0fbc80, tests: ctest --preset mac → Total Test time (real) =   0.11 sec)
Task 7: complete (commits f0fbc80..e6772b2, tests: ctest --preset mac → Total Test time (real) =   0.12 sec)
Task 8: complete (commits e6772b2..3853f68, tests: ctest --preset mac → Total Test time (real) =   0.12 sec)
Task 9: Ruling: Mac link uses pkg-config *_LINK_LIBRARIES (absolute paths) instead of bare names + link dirs — bare -lavcodec resolved to Homebrew's stale FFmpeg 8.1.1 in /opt/homebrew/lib (ABI mismatch with the ffmpeg@7 headers; dyld abort on libx265.216) — cost if wrong: none.
Task 9: note: suite has 12 tests (plan said 11): 5 spec suites + 7 own (threads, pad_mapping, perf_log, stderr_tee, startup_rules, memory_usage, present_math) — plan miscounted the own tests. Fullscreen and LogToFile were already true in the vendor ini, so the diff shows 10 changed keys.
Task 9: complete (commits 3853f68..fea1a90, tests: ctest --preset mac → Total Test time (real) =   0.21 sec)
Task 10: Ruling: null backend initialize_ge_gpu_backend returns true (error cleared) — main.cpp:295 throws on false; real backends return true for Software. Plan Task 9 had it return false. Test test_gpu_backend_null RED→GREEN — cost if wrong: none.
Task 10: Ruling: vendor fix (unguarded) — audio_output_shutdown() joins the static SubmitWorker thread; every exit aborted with std::terminate (crash report: SubmitWorker::~SubmitWorker). test_audio_shutdown RED (exit 134) → GREEN; recorded in UPSTREAM.md — cost if wrong: none (fork bug, also hits their macOS build).
Task 10: Ruling: user's Mac playtest (Step 3) runs in parallel with Task 11 (Switch build) — Task 11 consumes only Task 9 code, not the playtest result; Task 10 closes when the user reports the checklist — cost if wrong: a Mac-found bug may need a fix that also touches the Switch build (rebuild).
Task 11: Ruling: switch preset sets DEVKITPRO/DEVKITA64 in its environment — devkitA64 gcc refuses to run without DEVKITPRO and the installer only exports it for login shells — cost if wrong: none.
Task 10: Ruling: my earlier audio shutdown fix latched stop_requested; install_profile() calls audio_output_shutdown() at boot, so the worker died after its first job — the game was silent (user report: sound existed in the run that aborted at exit). Fix re-arms the worker after join. test_audio_shutdown extended (boot-reset case) RED pushed=512/5120 → GREEN; game raw dump peak 0 → 23202 — cost if wrong: none.
Task 10: complete (commits fea1a90..bc8f156, tests: ctest --preset mac → Total Test time (real) =   1.34 sec)
Task 11: Ruling: Switch host sources get _GNU_SOURCE — -std=c++20 sets __STRICT_ANSI__ and newlib hides setenv (vcs_config.cpp:728 error); scoped to vcsnx_configure_host so the 235 generated units are not rebuilt — cost if wrong: none.
Task 11: complete (commits ce9a0b7..819efdf, tests: sh -c 'cmake --build --preset switch >/dev/null && find build/switch -name vcsnx.nro -size +10M | grep -q . && ctest --preset mac | grep "tests passed"' → 100% tests passed out of 14)
Task 12: Ruling: the user transfers over MTP (DBI + OpenMTP), which is not a mounted filesystem; prepare_sd.sh fills a staging root ~/Desktop/VCSNX_SD (with switch/) that the user drags into '1: SD Card' — script unchanged — cost if wrong: one extra 1.2 GB local copy.
Task 12: FTP via DBI (<switch-ip>:5000): 1 of 314 files uploaded (EBOOT.BIN, 1.31 MB/s); the following transfers hit 'FTP response timeout' (likely MKD of nested dirs on DBI's server); upload stopped. Only switch/vcsnx/PSP_DATA/EBOOT.BIN exists on the SD.
Task 12: Ruling: first Switch boot exited before the window: 'cannot make canonical path: Invalid argument [sdmc:/switch/vcsnx/PSP_DATA]' (newlib can't canonicalize device paths). Fixed Runtime::set_game_root (error_code overload + lexically_normal fallback; test_runtime_paths RED→GREEN via an EACCES folder) and vcs_profile normalized_native_path (Switch: skip absolute()). Added a pre-main boot log (stdout/stderr → sdmc:/switch/vcsnx/vcsnx_boot.log when not under nxlink) — cost if wrong: none.
Task 12: Ruling: ROOT CAUSE of the 5 s headless start — main.cpp calls display_window_start() on the main thread before the guest thread exists; the plan's design (start waits for the pump to create the window) made main wait on itself. display_window_start now creates the window synchronously (like the fork). Added a Switch status heartbeat ([vcsnx-status], every 2 s) and fixed boot-log interleaving (both streams append) — cost if wrong: none.
Task 12: Ruling: ROOT CAUSE of the black screen — FFmpeg parsed 'sdmc:' as a URL protocol ('Protocol not found'), MPEGreadThread exited and the boot waited forever for the logo movie (guest stuck at pc 0x08B3AD90 = loader's DelayThread loop, vblank 0). Fix: 'file:' prefix in vcs_media_decoder open_stream. test_media_paths RED→GREEN, suite 16/16 — cost if wrong: none.
Task 12: Ruling: SD transfer did not use tools/prepare_sd.sh directly: Mac MTP failed (Switch not enumerated on USB), DBI FTP failed creating nested dirs; the user moved the staged folder via USB stick → PC → DBI MTP; later .nro updates went over DBI FTP (single-file uploads work, ~0.5 MB/s) — cost if wrong: none.
Task 12: Ruling: nxlink abandoned on this network (nc probes broke the netloader; UDP discovery unanswered); boot diagnosis moved to SD logs fetched over FTP (vcsnx_boot.log) — cost if wrong: slower debug loop.
Task 13: Ruling: H3 measurement taken from the diagnostic run (logos + intro only; 1-3 FPS); city scene impractical at 1-3 FPS; Mac/Switch frame comparison and applet-message check not done; mem_peak_mb from InfoType_UsedMemorySize is the whole pre-reserved heap (3185 MB), not real usage — recorded in docs/h3-results.md as pending for Plan 2 — cost if wrong: H4 decided on partial data, but a 10-30x gap makes the conclusion robust.
Task 13: H4 decision by the user: stage 2 moves forward with Vulkan via mesa-switch (spec updated).
Task 12: complete (commits 819efdf..cd40bc9, tests: sh -c 'ctest --preset mac | grep "tests passed" && cmake --build --preset switch >/dev/null && echo switch-build-ok' → switch-build-ok)
Task 13: complete (commits ae94646..cd40bc9, tests: sh -c 'test -s docs/h3-results.md && grep -q "Decisión H4" docs/h3-results.md && echo h3-doc-ok' → h3-doc-ok)
Plan 2 prep (2026-10-11): mesa-switch 606441c built in Docker (ARM64, 9m52s) at scratchpad/src_untrusted3/mesa-switch/builddir-switch (libvulkan.a 89M, libnvk.a, libnak_rs.a 32M, ...; 439M). Homebrew shaderc/molten-vk/vulkan-loader/vulkan-headers installed; switch-libexpat/libzstd present. Plan 2 doc written (uncommitted): docs/superpowers/plans/2026-10-11-vcsnx-plan-2-vulkan.md. Whole-branch review of Plan 1 was STOPPED (usage limit) before reporting — re-run it next session.
Final review: fresh reviewer (opus, resumed after the usage-limit stop) — verdict "With fixes"; 0 Critical, 4 Important, 8 Minor.
Final: fixed #1 startup data check disagreed with the vendor bootstrap (silent exit on encrypted/partial PSP_DATA) — nx_startup now asks vcs::resolve_bootstrap_paths, classifies the ELF header and checks LOGO/TITLES.PMF; test_startup_rules 4 new cases RED (compile error: no ExecutableState) → GREEN 8/8.
Final: fixed #2 docked output never 1920x1080 — SDL_WINDOW_RESIZABLE on the Switch (SDL2's SWITCH_PumpEvents only resizes resizable windows); no Mac test possible, to verify on the console.
Final: fixed #3 GCC fused a*b+c into fmadd (vendor pins -ffp-contract=off) — added to vcsnx_aot and host sources; verified with devkitA64: 1 fmadd by default, 0 with the flag.
Final: fixed #4 regen_generated.sh rm -rf'd a working generated/ — builds into .cache/regen.XXXXXX and swaps only when identical; trailing-slash guard; new tools/restore_generated.sh (CMake message points to it); verified with a fake partial generator (sentinel kept, exit 1) and the "$ROOT/" guard (exit 5).
Final: minor (deferred): static stderr tee destroyed before late cerr writes at exit (stderr_tee.cpp).
Final: minor (deferred): lines from different threads can interleave in the tee (mutex per xsputn, not per line).
Final: minor (deferred): stderr lines written to SD up to three times without nxlink (boot log unbuffered + tee).
Final: minor (deferred): memory_used_mb() reports the whole reserved heap on the Switch — already Plan 2 Task 6.
Final: minor (deferred): prepare_sd.sh overwrites the user's VCSNative.ini.
Final: minor (deferred): run_mac.sh breaks with a relative PSP_DATA path.
Final: minor (deferred): test_audio_shutdown needs a real audio device (fails headless).
Final: minor (deferred): nits — sockets always initialised, pthread_attr_setstacksize result ignored, vcs_profile patch guarded by __SWITCH__ vs VCSNX.
Final: Ruling: VCSProject2DFX_Lights.bin (vendor data/) — kept: imported from the Project2DFX mod (lights table), not game data — cost if wrong: one tracked binary to drop.
Final: Ruling: equal-priority core-0 contention (main + raster worker 2 + audio) — measure on device in Plan 2 — cost if wrong: lower FPS until measured.
Final: Ruling: forwarder argv / HOME clean exit / save-data paths — verify on the console when the Sphaira forwarder exists (Final milestone) — cost if wrong: discovered later.
Final: Ruling: present_ms ~21 ms on the Switch is spent in display_window_present's decode+repack on the guest thread — measure first in Plan 2 Task 7 — cost if wrong: caps FPS near 48.
Final: fixes committed in 17b5752 (suite 16/16 Mac, Switch build ok).
```
