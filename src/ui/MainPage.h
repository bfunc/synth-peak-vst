// The Main tab, as in the design canvas mock:
//   source rail | oscillators (3 rows) | mixer (lined up with the rows) | filter
//   source rail | envelopes | LFOs | voice + arp, drive | analog
// What each panel shows comes from ui/Layout.cpp; the classes here only do layout and drawing.
#pragma once

#include "plugin/UiFeed.h"
#include "ui/Graphs.h"
#include "ui/PanelKit.h"

#include <array>
#include <functional>

// ‹ name › over the wave icon. Click the name: the wave picker (basic waves + 60 wavetables).
class WaveCell : public juce::Component
{
public:
    WaveCell (UiContext&, int osc);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    WaveIcon& iconRef() { return icon; }
    int position() const;          // 0..3 basic waves, 4.. wavetables (choice index + 4)
    void setPosition (int pos);

private:
    juce::Rectangle<int> nameArea() const;
    UiContext& ctx;
    int n, waveIdx, tableIdx;
    juce::TextButton prev { juce::CharPointer_UTF8 ("\xe2\x80\xb9") }, next { juce::CharPointer_UTF8 ("\xe2\x80\xba") };
    std::unique_ptr<juce::ParameterAttachment> waveAtt, tableAtt;
    WaveIcon icon;
};

// The small gear next to "OSC n": opens Fixed Note and Bend Range. Lit when either is not at its default.
class CogButton : public juce::Button
{
public:
    CogButton() : juce::Button ("Oscillator settings") {}
    bool lit = false;

protected:
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class OscRow : public juce::Component, public Refreshable
{
public:
    OscRow (UiContext&, int osc);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    UiContext& ctx;
    int n;
    int lastSource = -1, lastWave = -1, lastCog = -1, lastDensity = -1;
    WidgetSet w;
    WaveCell wave;
    CogButton cog;
};

class OscSection : public PanelFrame, public Refreshable
{
public:
    explicit OscSection (UiContext&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;
    // Columns of a row: Range | Pitch | Pitch Mod | Wave | Shape | Sync·FM (shared with the header).
    static std::array<juce::Rectangle<int>, 6> columns (juce::Rectangle<int> row);
    // Row i of a 3-row panel below its 24 px header (the mixer uses the same rows).
    static juce::Rectangle<int> rowArea (juce::Rectangle<int> panel, int i);

private:
    std::array<std::unique_ptr<OscRow>, 3> rows;
};

class MixerPanel : public PanelFrame
{
public:
    explicit MixerPanel (UiContext&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    WidgetSet w;
};

class FilterPanel : public PanelFrame, public Refreshable
{
public:
    explicit FilterPanel (UiContext&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    int lastEnv = -1;
    WidgetSet w;
    FilterGraph graph;
    juce::Rectangle<int> cutoffCap, modCap, envCap;
};

// Tabs along a panel header; each tab carries a drag grip for its mod source.
class HeaderTabs : public juce::Component
{
public:
    HeaderTabs (const juce::StringArray& names, const std::vector<int>& modSources);
    std::function<void (int)> onSelect;
    int selected() const { return sel; }
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int preferredWidth() const;

private:
    juce::Rectangle<int> tabRect (int i) const;
    juce::StringArray names;
    juce::OwnedArray<DragSource> grips;
    int sel = 0;
};

class EnvPanel : public PanelFrame, public Refreshable
{
public:
    EnvPanel (UiContext&, const UiFeed&, std::function<bool()> playheadsOn);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    void show (int tab);
    WidgetSet w;
    HeaderTabs tabs;
    EnvGraph graph;
    const UiFeed& feed;
    std::function<bool()> playheadsOn;
    juce::Rectangle<int> noteArea, trigCap;
    int held = -1;
    uint32_t ons = 0;
    double flashUntil = 0;
    bool gateVisible = false;
};

class LfoPanel : public PanelFrame, public Refreshable
{
public:
    LfoPanel (UiContext&, const UiFeed&, std::function<bool()> playheadsOn);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    void show (int tab);
    WidgetSet w;
    HeaderTabs tabs;
    LfoGraph graph;
    juce::Rectangle<int> noteArea, fadeCap;
    int lastRange = -1;
};

class VoicePanel : public PanelFrame
{
public:
    explicit VoicePanel (UiContext&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    WidgetSet w;
    juce::Rectangle<int> modeCap, octCap, spreadCap;
};

class ArpPanel : public PanelFrame
{
public:
    explicit ArpPanel (UiContext&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    WidgetSet w;
    juce::Rectangle<int> typeCap, rateCap, rhythmCap;
};

// Drive stages in signal order | analogue character (diverge, drift).
class DrivePanel : public PanelFrame
{
public:
    explicit DrivePanel (UiContext&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    WidgetSet w;
};

class SourceRail : public juce::Component
{
public:
    SourceRail();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::OwnedArray<DragSource> sources;
};

class MainPage : public juce::Component, public Refreshable
{
public:
    MainPage (UiContext&, const UiFeed&, std::function<bool()> playheadsOn);
    void resized() override;
    void refresh() override;

private:
    SourceRail rail;
    OscSection oscs;
    MixerPanel mixer;
    FilterPanel filter;
    EnvPanel env;
    LfoPanel lfo;
    VoicePanel voice;
    ArpPanel arp;
    DrivePanel drive;
};
