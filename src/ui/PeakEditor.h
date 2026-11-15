// The editor window: a fixed 1360x800 canvas (top bar + one page) scaled to the window size.
// Pages are independent components; adding a page means adding it to the list in the constructor.
#pragma once

#include "plugin/PeakProcessor.h"
#include "ui/PeakLook.h"
#include "ui/Widgets.h"

#include <memory>
#include <vector>

class TopBar;

class PeakEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit PeakEditor (PeakProcessor&);
    ~PeakEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void showPage (int index);
    void say (const juce::String& message);  // a short message in the patch display
    void refreshNow();                        // what the timer does, at once (snapshots, tests)

private:
    void timerCallback() override;
    void updateModRings();
    void sourceDropped (int modSource, const std::string& paramId);

    PeakProcessor& proc;
    PeakLook look;
    UiContext ctx;
    juce::Component canvas;
    std::unique_ptr<TopBar> top;
    struct Page
    {
        juce::String name;
        std::unique_ptr<juce::Component> comp;
    };
    std::vector<Page> pages;
    int current = 0;
    peak::Values lastValues;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeakEditor)
};
