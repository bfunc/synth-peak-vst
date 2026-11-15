// LookAndFeel for the whole editor: arc knobs, flat dark buttons with an orange "on" state, dark combo boxes and
// pop-up menus. Widgets only set colours / properties; all drawing of standard JUCE controls lives here.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PeakLook : public juce::LookAndFeel_V4
{
public:
    PeakLook();

    // Slider property "bipolar" (bool): the value arc starts at the centre.
    // Slider property "mod" (double, -1..1): orange modulation ring from the value.
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart,
                        int thumbSize, bool over, bool down) override;
};
