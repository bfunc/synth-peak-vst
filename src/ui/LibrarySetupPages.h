// The Presets tab (the synth's 4 x 128 patches: read, search, load, files) and the Setup tab (ports, channel,
// output mode, options, connection checklist).
#pragma once

#include "plugin/PeakProcessor.h"
#include "ui/Graphs.h"
#include "ui/PanelKit.h"

class PresetsPage : public juce::Component, public Refreshable, private juce::TableListBoxModel
{
public:
    explicit PresetsPage (PeakProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int col, int w, int h, bool selected) override;
    void cellDoubleClicked (int row, int col, const juce::MouseEvent&) override;
    void rebuild();
    const peak::Library::Entry* selectedEntry() const;
    void importFile();
    void exportBank();

    PeakProcessor& proc;
    juce::TextButton readSel { "Read bank" }, readAll { "Read all banks" }, stop { "Stop" }, load { "Load" },
        importBtn { "Import .syx" }, exportBtn { "Export bank" };
    juce::ComboBox bankBox, categoryBox;
    juce::TextEditor search;
    juce::ToggleButton loadedOnly { "Hide unread" };
    juce::TableListBox table;
    double progress = 0;
    juce::ProgressBar bar { progress };
    std::vector<const peak::Library::Entry*> rows;
    uint32_t seenRev = 0;
    std::unique_ptr<juce::FileChooser> chooser;
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
    PeakProcessor& proc;
    juce::ComboBox inBox, outBox, channelBox, modeBox;
    juce::ToggleButton follow { "Follow the Peak's channel" };
    juce::ToggleButton notesThru { "Pass notes to the Peak" }, playheads { "LFO sync playhead + envelope gate light" };
    juce::TextButton check { "Check again" }, sendAll { "Send all parameters" }, rescan { "Rescan ports" };
    std::vector<peak::CheckLine> lines;
    juce::String linesKey;
};
