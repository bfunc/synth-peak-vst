// The Main tab: source rail | oscillators, mixer, filter | envelopes, LFOs, voice, arp (docs/panel-mockup.html).
// Each panel is its own class; what it shows comes from ui/Layout.cpp, so panels only do layout.
#pragma once

#include "plugin/UiFeed.h"
#include "ui/Graphs.h"
#include "ui/PanelKit.h"

#include <array>
#include <functional>

// Wave choice across two parameters: Sine..Pulse, then the 60 wavetables ("More" + wavetable).
class WaveSelector : public juce::Component
{
public:
    WaveSelector (UiContext&, int osc);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int position() const;          // 0..3 basic waves, 4.. wavetables
    void setPosition (int pos);
    UiContext& ctx;
    int waveIdx, tableIdx;
    juce::TextButton prev { juce::CharPointer_UTF8 ("\xe2\x80\xb9") }, next { juce::CharPointer_UTF8 ("\xe2\x80\xba") };
    std::unique_ptr<juce::ParameterAttachment> waveAtt, tableAtt;
};

class OscPanel : public PanelFrame, public Refreshable
{
public:
    OscPanel (UiContext&, int osc);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    int n;
    int lastSource = -1;
    WidgetSet w;
    WaveSelector wave;
    WaveGraph graph;
};

class MixerPanel : public PanelFrame
{
public:
    explicit MixerPanel (UiContext&);
    void resized() override;

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
};

// Vertical tabs on the left of the envelope and LFO panels; each tab also carries a drag grip for its mod source.
class SideTabs : public juce::Component
{
public:
    SideTabs (const juce::StringArray& names, const std::vector<int>& modSources);
    std::function<void (int)> onSelect;
    int selected() const { return sel; }
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::Rectangle<int> extraArea() const;  // free space below the tabs

private:
    juce::StringArray names;
    juce::OwnedArray<DragSource> grips;
    int sel = 0;
    static constexpr int kTabH = 32;
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
    SideTabs tabs;
    EnvGraph graph;
    const UiFeed& feed;
    std::function<bool()> playheadsOn;
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
    void refresh() override;

private:
    void show (int tab);
    WidgetSet w;
    SideTabs tabs;
    LfoGraph graph;
    int lastRange = -1;
};

class VoicePanel : public PanelFrame
{
public:
    explicit VoicePanel (UiContext&);
    void resized() override;

private:
    WidgetSet w;
};

class ArpPanel : public PanelFrame
{
public:
    explicit ArpPanel (UiContext&);
    void resized() override;

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
    std::array<std::unique_ptr<OscPanel>, 3> oscs;
    MixerPanel mixer;
    FilterPanel filter;
    EnvPanel env;
    LfoPanel lfo;
    VoicePanel voice;
    ArpPanel arp;
};
