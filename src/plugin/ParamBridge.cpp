#include "ParamBridge.h"

using namespace peak;

juce::AudioProcessorValueTreeState::ParameterLayout ParamBridge::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& p : paramTable())
    {
        const juce::ParameterID id { p.id, 1 };
        const auto* def = &p;
        switch (p.kind)
        {
            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (id, p.name, p.def != 0));
                break;
            case Kind::Choice:
            {
                juce::StringArray names;
                for (const auto& c : p.choices)
                    names.add (c);
                layout.add (std::make_unique<juce::AudioParameterChoice> (id, p.name, names, p.def - p.min));
                break;
            }
            case Kind::Bipolar:
            case Kind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (
                    id, p.name, p.min, p.max, p.def,
                    juce::AudioParameterIntAttributes().withStringFromValueFunction (
                        [def] (int v, int) { return juce::String (formatValue (*def, v)); })));
                break;
        }
    }
    return layout;
}

ParamBridge::ParamBridge (juce::AudioProcessorValueTreeState& apvts)
{
    for (const auto& p : paramTable())
    {
        raw.push_back (apvts.getRawParameterValue (p.id));
        params.push_back (apvts.getParameter (p.id));
        jassert (raw.back() != nullptr && params.back() != nullptr);
    }
}

int ParamBridge::value (size_t i) const
{
    const auto& p = paramTable()[i];
    const int x = (int) std::lround (raw[i]->load());
    return p.kind == Kind::Choice ? p.min + x : x;  // a choice is stored as its index
}

Values ParamBridge::values() const
{
    Values v (raw.size());
    for (size_t i = 0; i < raw.size(); ++i)
        v[i] = value (i);
    return v;
}

float ParamBridge::toNormalised (size_t i, int v) const
{
    const auto& p = paramTable()[i];
    v = clampValue (p, v);
    const float plain = (float) (p.kind == Kind::Choice ? v - p.min : v);
    return params[i]->convertTo0to1 (plain);
}

void ParamBridge::setByUser (size_t i, int v)
{
    auto* prm = params[i];
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (toNormalised (i, v));
    prm->endChangeGesture();
}

void ParamBridge::setQuietly (size_t i, int v)
{
    if (value (i) != clampValue (paramTable()[i], v))
        params[i]->setValueNotifyingHost (toNormalised (i, v));
}

juce::RangedAudioParameter* ParamBridge::parameter (const std::string& id) const
{
    const int i = indexOfId (id);
    return i < 0 ? nullptr : params[(size_t) i];
}
