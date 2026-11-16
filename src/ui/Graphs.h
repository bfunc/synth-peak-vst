// Drawings in the "wells": oscillator wave, filter curve, envelope, LFO (+ sync playhead), small FX pictures.
// They are illustrations from the current values (the Peak publishes no curves), redrawn when their inputs change.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/ParamBridge.h"
#include "plugin/UiFeed.h"

#include <functional>
#include <string>
#include <vector>

// Called by the editor's timer (~30 Hz); repaint only when something changed.
class Refreshable
{
public:
    virtual ~Refreshable() = default;
    virtual void refresh() = 0;
};

// Base: a dark well with a grid; subclasses draw a path and say which values they depend on.
class GraphBase : public juce::Component, public Refreshable
{
public:
    explicit GraphBase (ParamBridge& p) : params (p) {}
    void refresh() override;
    void paint (juce::Graphics&) override;

protected:
    int v (const std::string& id) const;  // current table value of a parameter
    virtual std::vector<int> inputs() const = 0;
    virtual void draw (juce::Graphics&, juce::Rectangle<float>) = 0;
    void strokeFilled (juce::Graphics&, const juce::Path&, juce::Rectangle<float>, float baseline);
    ParamBridge& params;
    std::vector<int> last;
};

class WaveGraph : public GraphBase
{
public:
    WaveGraph (ParamBridge& p, int osc) : GraphBase (p), k ("osc" + std::to_string (osc) + "_") {}

protected:
    std::vector<int> inputs() const override;
    void draw (juce::Graphics&, juce::Rectangle<float>) override;

private:
    std::string k;
};

// The small wave icon of an oscillator: one ideal cycle (rising saw; pulse width follows Shape). Wavetables can't be
// read from the Peak, so they show a greyed placeholder curve and their number. With Saw Density up, two faint
// copies show the thickening (an illustration).
class WaveIcon : public GraphBase
{
public:
    WaveIcon (ParamBridge& p, int osc) : GraphBase (p), k ("osc" + std::to_string (osc) + "_") {}
    void paint (juce::Graphics&) override;

protected:
    std::vector<int> inputs() const override;
    void draw (juce::Graphics&, juce::Rectangle<float>) override {}

private:
    std::string k;
};

class FilterGraph : public GraphBase
{
public:
    using GraphBase::GraphBase;

protected:
    std::vector<int> inputs() const override;
    void draw (juce::Graphics&, juce::Rectangle<float>) override;
};

// The envelope shown is chosen by the panel (key "amp" / "mod1" / "mod2").
class EnvGraph : public GraphBase
{
public:
    EnvGraph (ParamBridge& p) : GraphBase (p) {}
    void setKey (const std::string& key) { k = key + "_"; last.clear(); refresh(); }

protected:
    std::vector<int> inputs() const override;
    void draw (juce::Graphics&, juce::Rectangle<float>) override;

private:
    std::string k = "amp_";
};

// LFO wave; with playheads on and the LFO in Sync range, a dot at the host position, labelled "approx".
class LfoGraph : public GraphBase
{
public:
    LfoGraph (ParamBridge& p, const UiFeed& f, std::function<bool()> playheadsOn)
        : GraphBase (p), feed (f), enabled (std::move (playheadsOn)) {}
    void setLfo (int n) { lfo = n; last.clear(); refresh(); }
    void refresh() override;

protected:
    std::vector<int> inputs() const override;
    void draw (juce::Graphics&, juce::Rectangle<float>) override;

private:
    double waveAt (double phase) const;
    const UiFeed& feed;
    std::function<bool()> enabled;
    int lfo = 1;
    double playPhase = -1;  // -1 = no playhead
};

class FxGraph : public GraphBase
{
public:
    enum class Type { Distortion, Chorus, Delay, Reverb };
    FxGraph (ParamBridge& p, Type t) : GraphBase (p), type (t) {}

protected:
    std::vector<int> inputs() const override;
    void draw (juce::Graphics&, juce::Rectangle<float>) override;

private:
    Type type;
};
