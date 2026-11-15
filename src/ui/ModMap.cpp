#include "ModMap.h"

namespace peak::ui
{

namespace
{
// Index = destination number in the Mod Destination list.
const char* kModDestParam[] = {
    "", "osc1_coarse", "osc2_coarse", "osc3_coarse",                  // pitch
    "osc1_vsync", "osc2_vsync", "osc3_vsync",
    "osc1_shape", "osc2_shape", "osc3_shape",
    "mix_osc1", "mix_osc2", "mix_osc3", "mix_noise", "mix_ring", "vca_gain",
    "flt_overdrive", "flt_postdrive", "flt_freq", "flt_res",
    "lfo1_rate", "lfo2_rate",
    "amp_attack", "amp_decay", "amp_release",
    "mod1_attack", "mod1_decay", "mod1_release",
    "mod2_attack", "mod2_decay", "mod2_release",
    "fm_1to2", "fm_2to3", "fm_3to1", "",                              // FM Ns>O1 has no knob
    "flt_osc3", "",                                                   // Ns>FiltF
    "pan", "",                                                        // O123Shpe moves three
    "", "", "amp_sustain", "", "", "mod1_sustain", "", "", "mod2_sustain"  // Delay / Hold have no knob
};

const char* kFxDestParam[] = {
    "dist_level", "cho_level", "cho_rate", "cho_depth", "cho_feedback", "dly_level", "dly_time", "dly_feedback",
    "rev_level", "rev_time", "", "",  // Rev LPF / HPF (1.x names): which knob is unclear
    "rev_size", "rev_lowpass", "rev_highpass", "rev_damp_lp", "rev_damp_hp", "dly_slew", "dly_lp_damp",
    "dly_hp_damp", "dly_width", "", "", "", "noise_lpf", "noise_hpf"
};

constexpr int kModDests = (int) (sizeof (kModDestParam) / sizeof (kModDestParam[0]));
constexpr int kFxDests = (int) (sizeof (kFxDestParam) / sizeof (kFxDestParam[0]));

int valueOf (const Values& v, const std::string& id)
{
    const int i = indexOfId (id);
    return i < 0 || (size_t) i >= v.size() ? -1 : v[(size_t) i];
}
} // namespace

std::string paramForModDest (int d) { return d >= 0 && d < kModDests ? kModDestParam[d] : ""; }
std::string paramForFxDest (int d) { return d >= 0 && d < kFxDests ? kFxDestParam[d] : ""; }

int modDestForParam (const std::string& id)
{
    if (id.empty())
        return -1;
    for (int d = 0; d < kModDests; ++d)
        if (id == kModDestParam[d])
            return d;
    return -1;
}

int fxDestForParam (const std::string& id)
{
    if (id.empty())
        return -1;
    for (int d = 0; d < kFxDests; ++d)
        if (id == kFxDestParam[d])
            return d;
    return -1;
}

int fxSourceForModSource (int modSource)
{
    const auto& src = paramTable()[(size_t) indexOfId ("mod1_src_a")].choices;
    const auto& fx = paramTable()[(size_t) indexOfId ("fxmod1_src_a")].choices;
    if (modSource < 0 || (size_t) modSource >= src.size())
        return -1;
    for (size_t i = 0; i < fx.size(); ++i)
        if (fx[i] == src[(size_t) modSource])
            return (int) i;
    return -1;
}

double modAmountFor (const std::string& id, const Values& v)
{
    double sum = 0;
    const int d = modDestForParam (id);
    if (d >= 0)
        for (int s = 1; s <= 16; ++s)
        {
            const auto k = "mod" + std::to_string (s) + "_";
            if (valueOf (v, k + "dest") == d)
                sum += (valueOf (v, k + "depth") - 64) / 64.0;
        }
    const int f = fxDestForParam (id);
    if (f >= 0)
        for (int s = 1; s <= 4; ++s)
        {
            const auto k = "fxmod" + std::to_string (s) + "_";
            if (valueOf (v, k + "dest") == f)
                sum += (valueOf (v, k + "depth") - 64) / 64.0;
        }
    return sum < -1 ? -1 : (sum > 1 ? 1 : sum);
}

SlotTarget slotForDrop (int modSource, const std::string& id, const Values& v)
{
    SlotTarget t;
    auto empty = [&] (const std::string& k)
    {
        return valueOf (v, k + "src_a") == 0 && valueOf (v, k + "src_b") == 0 && valueOf (v, k + "depth") == 64;
    };
    const int d = modDestForParam (id);
    if (d >= 0)
    {
        t.dest = d;
        t.source = modSource;
        for (int s = 1; s <= 16; ++s)
            if (empty ("mod" + std::to_string (s) + "_"))
            {
                t.slot = s;
                return t;
            }
        return t;
    }
    const int f = fxDestForParam (id);
    const int fs = fxSourceForModSource (modSource);
    if (f >= 0 && fs >= 0)
    {
        t.fx = true;
        t.dest = f;
        t.source = fs;
        for (int s = 1; s <= 4; ++s)
            if (empty ("fxmod" + std::to_string (s) + "_"))
            {
                t.slot = s;
                return t;
            }
    }
    return t;
}

} // namespace peak::ui
