#pragma once
#include <string>
#include <utility>
#include <vector>

namespace vcsnx {

using PromptTable = std::vector<std::pair<std::u16string, std::u16string>>;

// What the game's control prompts say on the Switch, for the fork's ENGLISH.GXT rewrite
// (vcs_key_prompts.cpp). The pad mapping follows Nintendo's A-confirms layout (pad_mapping.cpp):
// cross is A, circle is B, square is Y, triangle is X.
// Button glyph tokens (~X~ ~O~ ~S~ ~T~), replaced in every string.
[[nodiscard]] const PromptTable &switch_prompt_glyphs();
// PSP control names, replaced in the control-name strings only; longest phrase first.
[[nodiscard]] const PromptTable &switch_prompt_phrases();
// Accelerate and brake: in a vehicle ZR and ZL press the PSP's cross and square (pad_mapping.cpp),
// so the control names that are exactly those buttons say the triggers. `key` is the GXT key
// without its configuration prefix (C0..C3), `text` the name after the tables above. Returns
// true and rewrites `text` when it applies.
bool apply_switch_trigger_names(const std::string &key, std::u16string &text);

} // namespace vcsnx
