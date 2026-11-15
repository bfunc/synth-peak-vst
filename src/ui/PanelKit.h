// Building blocks every panel uses:
//   WidgetSet   widgets made from layout Items, found by parameter id, registered for modulation rings
//   PanelFrame  a titled panel (cyan top line) with an optional ▶ that opens the panel's pop-out
//   MoreGrid    the pop-out content: every parameter Layout assigns to the panel but no panel shows
//   DragSource  a label or pad that can be dragged onto a knob to create a mod slot
#pragma once

#include "ui/Widgets.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class WidgetSet
{
public:
    explicit WidgetSet (UiContext& c) : ctx (c) {}
    ~WidgetSet();

    // Creates widgets for the items, adds them to `parent` (visible) and registers them.
    void add (juce::Component& parent, const std::vector<peak::ui::Item>& items);
    ParamWidget* get (const std::string& id) const;
    ParamWidget& operator[] (const std::string& id) const;  // asserts in debug if missing
    void setVisible (const std::vector<peak::ui::Item>& items, bool visible) const;

private:
    UiContext& ctx;
    std::map<std::string, std::unique_ptr<ParamWidget>> widgets;
};

class PanelFrame : public juce::Component
{
public:
    // moreKey "" = no ▶ button.
    PanelFrame (UiContext&, const juce::String& title, const std::string& moreKey = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Rectangle<int> content() const;  // below the title row
    static constexpr int kTitleH = 20;

protected:
    UiContext& ctx;
    juce::String title;

private:
    std::string moreKey;
    juce::TextButton more { juce::CharPointer_UTF8 ("\xe2\x96\xb6") };
};

class MoreGrid : public juce::Component
{
public:
    MoreGrid (UiContext&, const std::string& key);
    void resized() override;
    void paint (juce::Graphics&) override;
    bool empty() const { return widgets.empty(); }

private:
    UiContext& ctx;
    std::vector<std::unique_ptr<ParamWidget>> widgets;
    std::vector<juce::String> captions;  // for non-knob widgets
};

class DragSource : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Style { Pad, Meter, Grip };
    DragSource (int modSource, const juce::String& label, Style, juce::String padText = {});
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    int source;
    juce::String label, padText;
    Style style;
};

// Lays children out in equal columns across a row (helper for knob rows).
void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items);
