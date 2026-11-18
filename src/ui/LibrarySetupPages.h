// The Setup tab: ports, channel, output mode and options on the left; on the right the connection verdict (the one
// thing to do now, with a button for it), the steps in the order they depend on each other, notes, and a monitor of
// every message from the Peak.
#pragma once

#include "plugin/PeakProcessor.h"
#include "ui/Graphs.h"
#include "ui/PanelKit.h"

#include <array>
#include <deque>
#include <functional>

// The monitor's lines, newest first, drawn in a scrolling viewport (the Setup page owns the lines).
class MonitorList : public juce::Component
{
public:
    static constexpr int kRowH = 17;
    MonitorList (const std::deque<PeakLink::MonitorLine>& l, std::function<int()> pluginChannel)
        : lines (l), channel (std::move (pluginChannel)) {}
    void paint (juce::Graphics&) override;
    static juce::String text (const PeakLink::MonitorLine&);  // one line as plain text (for Copy)

private:
    const std::deque<PeakLink::MonitorLine>& lines;
    std::function<int()> channel;
};

class SetupPage : public juce::Component, public Refreshable
{
public:
    explicit SetupPage (PeakProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    void fillPorts();
    void updateVerdictButtons();
    void paintVerdict (juce::Graphics&);
    void paintSteps (juce::Graphics&);
    void paintMonitor (juce::Graphics&);

    PeakProcessor& proc;
    juce::ComboBox inBox, outBox, channelBox, modeBox;
    juce::ToggleButton follow { "Follow the Peak's channel" };
    juce::ToggleButton notesThru { "Pass notes to the Peak" }, playheads { "LFO sync playhead + envelope gate light" };
    juce::TextButton check { "Check again" }, sendAll { "Send all parameters" }, rescan { "Rescan ports" };
    juce::TextButton testNote { "Play test note" };
    juce::ComboBox watchBox;  // watch the Peak's sound (poll test); its counters are painted under it
    juce::String watchText;
    juce::Rectangle<int> watchTextArea;

    peak::Report report;
    juce::String reportKey;
    std::array<juce::TextButton, 2> verdictButtons;
    std::array<std::function<void()>, 2> verdictActions;

    std::deque<PeakLink::MonitorLine> monitorLines;  // newest first
    uint32_t monitorSeen = 0;
    MonitorList monitorList { monitorLines, [this] { return proc.midiChannel(); } };
    juce::Viewport monitorView;
    juce::TextButton clearMonitor { "Clear" }, copyMonitor { "Copy" };
    void layoutMonitor (int added);

    juce::Rectangle<int> verdictArea, stepsArea, monitorArea;
};
