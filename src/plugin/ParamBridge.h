// The table's parameters as JUCE plugin parameters (AudioProcessorValueTreeState), and conversions between the
// host's representation and the synth's integer values. Everything else in the plugin talks in table indices and
// synth values (peak::Values); only this file knows how JUCE stores them.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "core/Patch.h"

#include <atomic>
#include <vector>

class ParamBridge
{
public:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    explicit ParamBridge (juce::AudioProcessorValueTreeState&);

    size_t size() const { return raw.size(); }
    int value (size_t index) const;    // any thread (reads an atomic)
    peak::Values values() const;

    // Set as a hand move would: the host records it, and the change goes to the synth. Message thread.
    void setByUser (size_t index, int value);
    // Set without a gesture (synth or patch changes). Message thread.
    void setQuietly (size_t index, int value);

    juce::RangedAudioParameter* parameter (size_t index) const { return params[index]; }
    juce::RangedAudioParameter* parameter (const std::string& id) const;

private:
    float toNormalised (size_t index, int value) const;

    std::vector<std::atomic<float>*> raw;
    std::vector<juce::RangedAudioParameter*> params;
};
