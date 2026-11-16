#include "Layout.h"

#include <set>

namespace peak::ui
{

namespace
{

using W = Widget;

bool startsWith (const std::string& s, const std::string& prefix) { return s.compare (0, prefix.size(), prefix) == 0; }

bool hasOscPrefix (const std::string& id)
{
    return startsWith (id, "osc1_") || startsWith (id, "osc2_") || startsWith (id, "osc3_");
}

std::function<bool (const ParamDef&)> prefix (std::string p)
{
    return [p] (const ParamDef& d) { return startsWith (d.id, p); };
}

std::function<bool (const ParamDef&)> section (Section s)
{
    return [s] (const ParamDef& d) { return d.section == s; };
}

std::vector<PanelSpec> build()
{
    std::vector<PanelSpec> v;

    for (int n = 1; n <= 3; ++n)
    {
        const auto k = "osc" + std::to_string (n);
        v.push_back ({ k, "Osc " + std::to_string (n), oscItems (n), prefix (k + "_") });
    }

    v.push_back ({ "mixer", "Mixer",
                   { { W::Knob, "mix_osc1", "Osc 1" }, { W::Knob, "mix_osc2", "Osc 2" }, { W::Knob, "mix_osc3", "Osc 3" },
                     { W::Knob, "mix_ring", "Ring 1x2" }, { W::Knob, "mix_noise", "Noise" }, { W::Knob, "vca_gain", "VCA Gain" } },
                   // the mixer, plus the oscillator settings common to all three (drift, divergence, noise filters, FM)
                   [] (const ParamDef& d)
                   {
                       return d.section == Section::Mixer || (d.section == Section::Oscillators && ! hasOscPrefix (d.id));
                   } });

    v.push_back ({ "filter", "Filter",
                   { { W::Segmented, "flt_shape", "" }, { W::Segmented, "flt_slope", "" }, { W::Segmented, "flt_env_sel", "Env Src" },
                     { W::BigKnob, "flt_freq", "Frequency" }, { W::Knob, "flt_res", "Resonance" },
                     { W::Knob, "flt_overdrive", "Overdrive" }, { W::Knob, "flt_postdrive", "Post Drive" },
                     { W::Knob, "flt_keytrack", "Key Track" }, { W::Knob, "flt_ampenv", "Env Depth" },
                     { W::Knob, "flt_modenv1", "Env Depth" }, { W::Knob, "flt_lfo1", "LFO 1" },
                     { W::Knob, "flt_osc3", "Osc 3 Mod" }, { W::Knob, "flt_divergence", "Diverge" } },
                   section (Section::Filter) });

    {
        std::vector<Item> env;
        for (const char* k : { "amp", "mod1", "mod2" })
            for (auto& it : envItems (k))
                env.push_back (it);
        v.push_back ({ "env", "Envelopes", env, section (Section::Envelopes) });
    }
    {
        std::vector<Item> lfo;
        for (int n = 1; n <= 4; ++n)
            for (auto& it : lfoItems (n))
                lfo.push_back (it);
        v.push_back ({ "lfo", "LFOs", lfo, section (Section::Lfo) });
    }

    v.push_back ({ "voice", "Voice",
                   { { W::Stepper, "voice_mode", "Mode" }, { W::Toggle, "glide_on", "Glide" },
                     { W::SmallKnob, "unison", "Unison" }, { W::SmallKnob, "unison_detune", "Detune" },
                     { W::SmallKnob, "spread", "Spread" }, { W::SmallKnob, "glide_time", "Glide" } },
                   section (Section::Voice) });

    v.push_back ({ "arp", "Arp",
                   { { W::Toggle, "arp_on", "On" }, { W::Toggle, "arp_latch", "Latch" }, { W::Toggle, "arp_keysync", "Key Sync" },
                     { W::Choice, "arp_type", "" }, { W::Choice, "arp_sync", "" },
                     { W::SmallKnob, "arp_gate", "Gate" }, { W::SmallKnob, "arp_swing", "Swing" },
                     { W::SmallKnob, "arp_octaves", "Octaves" }, { W::SmallKnob, "arp_chance", "Chance" } },
                   section (Section::Arp) });

    // FX page
    v.push_back ({ "fx", "Effects",
                   { { W::Toggle, "fx_bypass", "Bypass" }, { W::Stepper, "fx_routing", "Routing" },
                     { W::Knob, "fx_dry", "FX Dry" }, { W::Knob, "fx_wet", "FX Wet" } },
                   [] (const ParamDef& d) { return startsWith (d.id, "fx_"); } });
    v.push_back ({ "dist", "Distortion", { { W::Knob, "dist_level", "Level" } }, prefix ("dist_") });
    v.push_back ({ "chorus", "Chorus",
                   { { W::Segmented, "cho_mode", "" }, { W::Segmented, "cho_type", "" }, { W::Knob, "cho_rate", "Rate" },
                     { W::Knob, "cho_depth", "Depth" }, { W::Knob, "cho_feedback", "Feedback" },
                     { W::Knob, "cho_lp", "Low Pass" }, { W::Knob, "cho_hp", "High Pass" }, { W::Knob, "cho_level", "Level" } },
                   prefix ("cho_") });
    v.push_back ({ "delay", "Delay",
                   { { W::Choice, "dly_time_mode", "" }, { W::Toggle, "dly_sync", "Sync" }, { W::Knob, "dly_time", "Time" },
                     { W::Knob, "dly_sync_time", "Time" }, { W::Knob, "dly_feedback", "Feedback" },
                     { W::Knob, "dly_width", "Width" }, { W::Knob, "dly_lp_damp", "LP Damp" },
                     { W::Knob, "dly_hp_damp", "HP Damp" }, { W::Knob, "dly_slew", "Slew" }, { W::Knob, "dly_level", "Level" } },
                   prefix ("dly_") });
    v.push_back ({ "reverb", "Reverb",
                   { { W::Segmented, "rev_type", "" }, { W::Knob, "rev_time", "Time" }, { W::Knob, "rev_size", "Size" },
                     { W::Knob, "rev_predelay", "Pre-Delay" }, { W::Knob, "rev_damp_lp", "Damp LP" },
                     { W::Knob, "rev_damp_hp", "Damp HP" }, { W::Knob, "rev_lowpass", "Low Pass" },
                     { W::Knob, "rev_highpass", "High Pass" }, { W::Knob, "rev_level", "Level" } },
                   prefix ("rev_") });

    // Matrix page
    {
        std::vector<Item> m, f;
        for (int s = 1; s <= 16; ++s)
            for (auto& it : modSlotItems (s))
                m.push_back (it);
        for (int s = 1; s <= 4; ++s)
            for (auto& it : fxSlotItems (s))
                f.push_back (it);
        v.push_back ({ "matrix", "Mod Matrix", m, section (Section::ModMatrix) });
        v.push_back ({ "fxmatrix", "FX Mod Matrix", f, section (Section::FxMatrix) });
    }
    return v;
}

} // namespace

std::vector<Item> oscItems (int n)
{
    const auto k = "osc" + std::to_string (n) + "_";
    static const char* fm[] = { "fm_3to1", "fm_1to2", "fm_2to3" };
    static const char* fmLabel[] = { "FM 3>1", "FM 1>2", "FM 2>3" };
    return { { W::Segmented, k + "range", "Range" },
             { W::Knob, k + "coarse", "Coarse" },
             { W::Knob, k + "fine", "Fine" },
             { W::Stepper, k + "wave", "Wave" },
             { W::Stepper, k + "wavetable", "Wavetable" },
             { W::Knob, k + "shape", "Shape" },          // shown when Shape Source = Manual
             { W::Knob, k + "env1_shape", "Shape" },     // ... = ModEnv1 (the Peak's Shape knob sets this then)
             { W::Knob, k + "lfo1_shape", "Shape" },     // ... = LFO1
             { W::SegmentedV, k + "shape_src", "" },
             { W::Knob, k + "env2_pitch", "Env 2" },
             { W::Knob, k + "lfo2_pitch", "LFO 2" },
             { W::Knob, k + "vsync", "V-Sync" },
             { W::Knob, k + "saw_density", "Saw Dens" },
             { W::Knob, fm[(n - 1) % 3], fmLabel[(n - 1) % 3] } };
}

std::vector<Item> envItems (const std::string& key)
{
    const auto k = key + "_";
    return { { W::Knob, k + "attack", "Attack" },     { W::Knob, k + "decay", "Decay" },
             { W::Knob, k + "sustain", "Sustain" },   { W::Knob, k + "release", "Release" },
             { W::SmallKnob, k + "velocity", "Velocity" }, { W::Segmented, k + "trigger", "" } };
}

std::vector<Item> lfoItems (int n)
{
    const auto k = "lfo" + std::to_string (n) + "_";
    if (n >= 3)  // LFO 3/4: only phase, slew and fade have a published number
        return { { W::Knob, k + "fade", "Fade" }, { W::Knob, k + "phase", "Phase" }, { W::Knob, k + "slew", "Slew" } };
    return { { W::Segmented, k + "wave", "" },  { W::Segmented, k + "range", "" },
             { W::Knob, k + "rate", "Rate" },    { W::Knob, k + "sync", "Sync" },
             { W::Knob, k + "fade", "Fade" },    { W::Knob, k + "phase", "Phase" },
             { W::Knob, k + "slew", "Slew" },    { W::Choice, k + "fade_mode", "Fade Mode" },
             { W::Toggle, k + "oneshot", "One Shot" }, { W::Toggle, k + "common", "Common" } };
}

std::vector<Item> modSlotItems (int slot)
{
    const auto k = "mod" + std::to_string (slot) + "_";
    return { { W::Choice, k + "src_a", "" }, { W::Choice, k + "src_b", "" }, { W::Knob, k + "depth", "" },
             { W::Choice, k + "dest", "" } };
}

std::vector<Item> fxSlotItems (int slot)
{
    const auto k = "fxmod" + std::to_string (slot) + "_";
    return { { W::Choice, k + "src_a", "" }, { W::Choice, k + "src_b", "" }, { W::Knob, k + "depth", "" },
             { W::Choice, k + "dest", "" } };
}

const std::vector<PanelSpec>& panels()
{
    static const std::vector<PanelSpec> p = build();
    return p;
}

const PanelSpec* panel (const std::string& key)
{
    for (const auto& p : panels())
        if (p.key == key)
            return &p;
    return nullptr;
}

std::vector<int> moreParams (const std::string& key)
{
    std::set<std::string> shown;
    for (const auto& p : panels())
        for (const auto& it : p.items)
            shown.insert (it.id);

    std::vector<int> out;
    const auto& t = paramTable();
    for (int i = 0; i < (int) t.size(); ++i)
    {
        const auto& d = t[(size_t) i];
        if (shown.count (d.id))
            continue;
        for (const auto& p : panels())
            if (p.owns (d))
            {
                if (p.key == key)
                    out.push_back (i);
                break;  // the first owner takes it
            }
    }
    return out;
}

} // namespace peak::ui
