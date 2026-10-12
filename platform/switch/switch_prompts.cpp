#include "switch_prompts.hpp"

namespace vcsnx {

const PromptTable &switch_prompt_glyphs() {
    static const PromptTable table = {
        {u"~X~", u"A"}, {u"~O~", u"B"}, {u"~S~", u"Y"}, {u"~T~", u"X"},
    };
    return table;
}

// The fork's controller table (vcs_key_prompts.cpp pad_phrases) with Switch names. "L button" and
// "R button" stay as they are: the Switch has L and R in the same place.
const PromptTable &switch_prompt_phrases() {
    static const PromptTable table = {
        {u"analog stick left and analog stick right", u"Left stick left and right"},
        {u"analog stick up and analog stick down", u"Left stick up and down"},
        {u"left button and right button", u"D-pad left and right"},
        {u"up button and down button", u"D-pad up and down"},
        {u"analog stick left", u"Left stick left"},
        {u"analog stick right", u"Left stick right"},
        {u"analog stick up", u"Left stick up"},
        {u"analog stick down", u"Left stick down"},
        {u"analog stick", u"Left stick"},
        {u"directional buttons", u"D-pad"},
        {u"the down button", u"D-pad down"},
        {u"the up button", u"D-pad up"},
        {u"down button", u"D-pad down"},
        {u"up button", u"D-pad up"},
        {u"left button", u"D-pad left"},
        {u"right button", u"D-pad right"},
        {u"SELECT button", u"- button"},
        {u"D-PAD LEFT/RIGHT", u"D-pad left/right"},
        {u"CROSS", u"A"},
        {u"CIRCLE", u"B"},
        {u"SQUARE", u"Y"},
        {u"TRIANGLE", u"X"},
    };
    return table;
}

bool apply_switch_trigger_names(const std::string &key, std::u16string &text) {
    static const struct {
        const char *key;
        const char16_t *face_buttons;
        const char16_t *triggers;
    } kTriggers[] = {
        {"VEACC", u"A", u"ZR"},          // accelerate (cross), also pedal
        {"VEBRK", u"Y", u"ZL"},          // brake or reverse (square)
        {"VEHB", u"Y + A", u"ZL + ZR"},  // brake and accelerate together
    };
    for (const auto &entry : kTriggers) {
        if (key == entry.key && text == entry.face_buttons) {
            text = entry.triggers;
            return true;
        }
    }
    return false;
}

} // namespace vcsnx
