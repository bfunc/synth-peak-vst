// One line of help per control, shown in the top bar's help display when the mouse is over it.
// Texts follow the Peak 1.2 user guide and the 2.x addendum. Plain C++ (UTF-8), no JUCE.
#pragma once

#include <string>

namespace peak::ui
{

// Help for a table parameter id (e.g. "osc2_shape", "amp_decay", "lfo3_fade"); "" if there is none.
std::string helpFor (const std::string& paramId);

} // namespace peak::ui
