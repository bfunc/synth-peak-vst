// The FX tab (routing, distortion, chorus, delay, reverb) and the Matrix tab (16 mod slots, 4 FX mod slots).
#pragma once

#include "ui/Graphs.h"
#include "ui/PanelKit.h"

#include <array>

class FxRow : public PanelFrame, public Refreshable
{
public:
    FxRow (UiContext&, const char* key, const char* title, FxGraph::Type);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    std::string key;
    WidgetSet w;
    FxGraph graph;
    int lastSync = -1;
};

class FxPage : public juce::Component, public Refreshable
{
public:
    explicit FxPage (UiContext&);
    void resized() override;
    void refresh() override;

private:
    PanelFrame top;
    WidgetSet w;
    std::array<std::unique_ptr<FxRow>, 4> rows;
};

// A matrix panel: column headers and slot numbers around the rows.
class MatrixFrame : public PanelFrame
{
public:
    MatrixFrame (UiContext& c, const juce::String& titleText, int slotCount, juce::String noteText)
        : PanelFrame (c, titleText), slots (slotCount), note (std::move (noteText)) {}
    void paint (juce::Graphics&) override;
    static constexpr int kRowH = 28, kHeadH = 26;

private:
    int slots;
    juce::String note;
};

class MatrixPage : public juce::Component
{
public:
    explicit MatrixPage (UiContext&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Row
    {
        std::string prefix;
        std::unique_ptr<ParamWidget> a, b, dest;
        std::unique_ptr<ParamBar> depth;
        std::unique_ptr<juce::TextButton> clear;
    };
    void addRow (const std::string& prefix);
    UiContext& ctx;
    MatrixFrame mod, fx;
    std::vector<Row> rows;  // 16 + 4
};
