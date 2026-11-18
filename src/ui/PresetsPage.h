// The Presets tab: one list, two sources (never mixed).
//   Peak  the PC copy of the synth's 4 x 128 patches. Read bank / Read all (saves a dated backup), Load, Export.
//         A slot saved "on PC only" is marked, and Write to Peak puts it on the synth.
//   File  a .syx opened from disk. Try (edit buffer only), Copy to Peak (one slot), Send bank to Peak.
// Every write to the synth's memory asks first and runs with a backup and a read-back (LibraryService).
#pragma once

#include "plugin/PeakProcessor.h"
#include "ui/Graphs.h"

class PresetsPage : public juce::Component, public Refreshable, private juce::TableListBoxModel
{
public:
    explicit PresetsPage (PeakProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;
    void showFile();  // a .syx with many patches was opened elsewhere (the top bar's Load)

private:
    enum class Source { Peak, File };

    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int col, int w, int h, bool selected) override;
    void cellDoubleClicked (int row, int col, const juce::MouseEvent&) override;
    void selectedRowsChanged (int) override { updateButtons(); }

    void setSource (Source);
    void rebuild();
    void updateButtons();
    int selectedRow() const;

    void loadSelected();      // Peak: program change; File: try in the edit buffer
    void saveToFile();
    void openFile();
    void copyToPeak();
    void sendBank();

    PeakProcessor& proc;
    Source source = Source::Peak;

    juce::TextButton peakTab { "Peak" }, fileTab { "File" };
    // Peak
    juce::ComboBox bankBox;
    juce::TextButton readBank { "Read bank" }, readAll { "Read all" }, saveFile { "Export..." };
    juce::TextButton writeBtn { "Write to Peak" };   // a slot whose newest version is on the PC only
    // First run: offer to copy all 512 patches to the PC.
    juce::TextButton copyAllBtn { "Copy all" }, notNow { "Not now" };
    bool offerShown = false;
    juce::Rectangle<int> offerArea;
    void writeSelected();
    // File
    juce::TextButton openBtn { "Open file..." }, copyBtn { "Copy to Peak..." }, sendBtn { "Send bank to Peak..." };
    juce::ComboBox fileBankBox;
    // both
    juce::TextButton stop { "Stop" }, loadBtn { "Load" };
    juce::ComboBox categoryBox;
    juce::TextEditor search;
    juce::ToggleButton hideUnread { "Hide unread" };
    juce::TableListBox table;
    double progress = 0;
    juce::ProgressBar bar { progress };

    std::vector<int> rows;  // Peak: bank * 128 + slot (bank 1-based); File: index into the file's entries
    uint32_t seenRev = 0;
    std::string seenOrigin;
    bool wasBusy = false;
    std::unique_ptr<juce::FileChooser> chooser;
};
