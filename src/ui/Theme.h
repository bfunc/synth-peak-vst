// Colours and fonts of the editor, from docs/panel-mockup.html (one dark look in Peak colours).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
// Fixed design canvas; the editor scales it to the window.
constexpr int kWidth = 1360;
constexpr int kHeight = 800;

inline const juce::Colour voidBg { 0xff07090b };
inline const juce::Colour chassis { 0xff0f1215 };
inline const juce::Colour panel { 0xff161a1f };
inline const juce::Colour panelHi { 0xff1d2229 };
inline const juce::Colour well { 0xff090b0e };
inline const juce::Colour edge { 0xff252c34 };
inline const juce::Colour edgeHi { 0xff323a44 };
inline const juce::Colour ink { 0xffe6ebf0 };
inline const juce::Colour ink2 { 0xff9ba7b3 };
inline const juce::Colour ink3 { 0xff5e6a76 };
inline const juce::Colour accent { 0xff35b6e8 };
inline const juce::Colour led { 0xffff7a1a };
inline const juce::Colour knob { 0xff23292f };
inline const juce::Colour knobHi { 0xff2f363e };
inline const juce::Colour woodA { 0xff5a3520 };
inline const juce::Colour woodB { 0xff3b2214 };
inline const juce::Colour good { 0xff5fd38a };
inline const juce::Colour bad { 0xffff5a5a };

// Condensed UI face, body face, numbers. Fall back to the system's sans / mono when the bundled fonts are missing.
juce::Font fontUi (float height, bool bold = true);
juce::Font fontBody (float height);
juce::Font fontMono (float height);
} // namespace theme
