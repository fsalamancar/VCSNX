// Test module for platform/switch/elf_module: the relocation kinds vcs_game.so uses, built by the test
// build with devkitA64 (-shared -nostdlib -Wl,-Bsymbolic).
extern int fixture_import(int);                              // resolved by the loader (JUMP_SLOT / GLOB_DAT)
extern int fixture_weak_missing(int) __attribute__((weak));  // stays 0
int fixture_counter = 7;
int *fixture_counter_address = &fixture_counter;             // R_AARCH64_RELATIVE
int (*fixture_import_address)(int) = &fixture_import;        // symbolic, to the import
int fixture_call(int x) { return fixture_import(x) + 1; }
int fixture_has_weak(void) { return fixture_weak_missing != 0; }
