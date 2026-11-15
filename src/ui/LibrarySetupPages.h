// The Setup tab (ports, channel, output mode, options, connection checklist).
#pragma once

#include "plugin/PeakProcessor.h"
#include "ui/Graphs.h"
#include "ui/PanelKit.h"

class SetupPage : public juce::Component, public Refreshable
{
public:
    explicit SetupPage (PeakProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    void fillPorts();
    PeakProcessor& proc;
    juce::ComboBox inBox, outBox, channelBox, modeBox;
    juce::ToggleButton follow { "Follow the Peak's channel" };
    juce::ToggleButton notesThru { "Pass notes to the Peak" }, playheads { "LFO sync playhead + envelope gate light" };
    juce::TextButton check { "Check again" }, sendAll { "Send all parameters" }, rescan { "Rescan ports" };
    std::vector<peak::CheckLine> lines;
    juce::String linesKey;
};
