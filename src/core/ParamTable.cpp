#include "ParamTable.h"

#include <algorithm>
#include <map>

namespace peak
{

const char* sectionName (Section s)
{
    switch (s)
    {
        case Section::Voice:       return "Voice";
        case Section::Oscillators: return "Oscillators";
        case Section::Mixer:       return "Mixer";
        case Section::Filter:      return "Filter";
        case Section::Envelopes:   return "Envelopes";
        case Section::Lfo:         return "LFO";
        case Section::Effects:     return "Effects";
        case Section::Arp:         return "Arp";
        case Section::ModMatrix:   return "Mod Matrix";
        case Section::FxMatrix:    return "FX Matrix";
    }
    return "?";
}

namespace
{

using Names = std::vector<std::string>;

// ---- value lists -------------------------------------------------------------------------------------------
// Provisional ones (order not yet read off the synth's display) are marked.

const Names kOscRange { "16'", "8'", "4'", "2'" };
const Names kOscWave { "Sine", "Triangle", "Sawtooth", "Pulse", "More" };
const Names kShapeSource { "Manual", "ModEnv1", "LFO1" };
const Names kFilterSlope { "12 dB", "24 dB" };
const Names kFilterShape { "LP", "BP", "HP" };
const Names kFilterEnv { "Amp Env", "Mod Env 1" };
const Names kTrigger { "Legato", "Re-Trig" };
const Names kModEnvSelect { "Mod Env 1", "Mod Env 2" };
const Names kLfoRange { "Low", "High", "Sync" };
const Names kLfoWave { "Triangle", "Sawtooth", "Square", "S&H" };
const Names kLfoFade { "Fade In", "Fade Out", "Gate In", "Gate Out" };
const Names kSpreadMode { "Diverge", "Alternate", "Diverge 2", "NoteVal" };
const Names kChorusMode { "Chorus", "Phlanger", "Flanger" };
const Names kDelayTimeMode { "Normal", "Double", "Treble", "QuadLoFi", "HexVLoFi" };
const Names kFxType { "Type 1", "Type 2", "Type 3" };
// 2.x adds Chord2 as value 7 (provisional; the 1.2 table prints 0-6).
const Names kArpType { "Up", "Down", "UpDown", "UpDown2", "Played", "Random", "Chord", "Chord2" };

// Provisional: category / genre names, 0-14 and 0-9.
const Names kCategory { "None", "Arp", "Bass", "Bell", "Classic", "Drum", "Keyboard", "Lead", "Movement", "Pad",
                        "Poly", "SFX", "String", "User", "Voc/Tune" };
const Names kGenre { "None", "Classic", "D&B/Breaks", "House", "Industrial", "Jazz", "R&B/HHop", "Rock/Pop",
                     "Techno", "Dubstep" };

// Wave More 4-63: the manual's wavetable table read down its four columns (confirmed by the list in
// docs/WAVETABLES.md; read across, the order was wrong). Still to check on the OLED.
const Names kWavetables {
    "BS Sine", "Random", "Zing", "Tubey", "Octaves", "Wobbler", "Chords", "Didgery", "Harsh", "Organ", "E.Piano",
    "VoxOooEe", "VoxYahEe", "Winds", "SoftClav", "String", "BassOrgan", "Acid", "Buzzy", "Carousel", "Choral",
    "Climbing", "CoinFlip", "Deep", "Dub", "Eee", "Eris", "Flame", "Further", "GlassSaw", "Glassy", "Granular",
    "Grime", "Grow", "Heavy", "Hedge", "Hungry", "Ladders", "Lead", "Modeling", "Modem", "Monster", "Screech",
    "SeaBase", "Shmorgan", "Spirals", "Steels", "Sunrise", "Swell", "Thicker", "Thinner", "Tides", "Tokyo", "Tops",
    "V.Chord", "Variance", "Vocaloid", "Vowelled", "WeirdVox", "Yeah" };

const Names kLfoSync { "64 beats", "48 beats", "42 beats", "36 beats", "32 beats", "30 beats", "28 beats",
    "24 beats", "21+1/3", "20 beats", "18+2/3", "18 beats", "16 beats", "13+1/3", "12 beats", "10+2/3", "8 beats",
    "6 beats", "5+1/3", "4 beats", "3 beats", "2+2/3", "2nd", "4th D", "1+1/3", "4th", "8th D", "4th T", "8th",
    "16th D", "8th T", "16th", "16th T", "32nd", "32nd T" };

// Delay Sync Time: 16 values, the arp list from "4 beats" (confirmed: the Peak sends NRPN 0:94 = 0..15).
const Names kDelaySync { "4 beats", "3 beats", "2+2/3", "2nd", "4th D", "1+1/3", "4th", "8th D", "4th T", "8th",
    "16th D", "8th T", "16th", "16th T", "32nd", "32nd T" };

const Names kArpSync { "8 beats", "6 beats", "5+1/3", "4 beats", "3 beats", "2+2/3", "2nd", "4th D", "1+1/3",
    "4th", "8th D", "4th T", "8th", "16th D", "8th T", "16th", "16th T", "32nd", "32nd T" };

// Mod matrix lists (provisional, 1.2 order + 2.x additions at the end). The order agrees with "Perc Up GEO":
// ModWheel (1) -> FiltFreq (18), Animate2 (15) -> Osc1VSnc (4).
const Names kModSource { "Direct", "ModWheel", "AftTouch", "ExprPED1", "BrthPED2", "Velocity", "Keyboard", "Lfo1+",
    "Lfo1+/-", "Lfo2+", "Lfo2+/-", "AmpEnv", "ModEnv1", "ModEnv2", "Animate1", "Animate2", "CV +/-", "Lfo3 +",
    "Lfo3 +/-", "Lfo4 +", "Lfo4 +/-", "BndWhl+", "BndWhl-", "Noise" };
const Names kModDest { "O123Ptch", "Osc1Ptch", "Osc2Ptch", "Osc3Ptch", "Osc1VSnc", "Osc2VSnc", "Osc3VSnc",
    "Osc1Shpe", "Osc2Shpe", "Osc3Shpe", "Osc1 Lev", "Osc2 Lev", "Osc3 Lev", "NoiseLev", "Ring Lev", "VcaLevel",
    "Filt Drv", "FiltDist", "FiltFreq", "Filt Res", "Lfo1Rate", "Lfo2Rate", "AmpEnv A", "AmpEnv D", "AmpEnv R",
    "ModEnv1A", "ModEnv1D", "ModEnv1R", "ModEnv2A", "ModEnv2D", "ModEnv2R", "FM O1>O2", "FM O2>O3", "FM O3>O1",
    "FM Ns>O1", "O3>FiltF", "Ns>FiltF", "PanPosn", "O123Shpe", "AEnvDel", "AEnvHld", "AEnvSus", "MEnv1Del",
    "MEnv1Hld", "MEnv1Sus", "MEnv2Del", "MEnv2Hld", "MEnv2Sus" };
const Names kFxSource { "Direct", "ModWheel", "AftTouch", "ExprPED1", "BrthPED2", "Velocity", "Keyboard",
    "Animate1", "Animate2", "CV +/-", "Lfo3 +", "Lfo3 +/-", "Lfo4 +", "Lfo4 +/-", "BndWhl+", "BndWhl-", "Noise" };
const Names kFxDest { "Dist Lev", "Chor Lev", "ChorRate", "Chor Dep", "Chor FB", "Del Lev", "Del Time", "Del FB",
    "Rev Lev", "Rev Time", "Rev LPF", "Rev HPF", "Rev Size", "RevLpass", "RevHpass", "RevLdamp", "RevHdamp",
    "Del Slew", "DelLdamp", "DelHdamp", "DelWidth", "DelRatio", "Lfo3Rate", "Lfo4Rate", "NoiseLPF", "NoiseHPF" };

// ---- builders ----------------------------------------------------------------------------------------------

struct Builder
{
    std::vector<ParamDef> out;
    Section section = Section::Voice;

    ParamDef& add (std::string id, std::string name, Addr a, int n1, int n2, int mn, int mx, int def)
    {
        ParamDef p;
        p.id = std::move (id);
        p.name = std::move (name);
        p.section = section;
        p.addr = a;
        p.num = n1;
        p.num2 = n2;
        p.min = mn;
        p.max = mx;
        p.def = def;
        out.push_back (std::move (p));
        return out.back();
    }
    ParamDef& cc (std::string id, std::string name, int c, int mn = 0, int mx = 127, int def = 0)
    {
        return add (std::move (id), std::move (name), Addr::Cc, c, -1, mn, mx, def);
    }
    ParamDef& pair (std::string id, std::string name, int msb, int lsb, int mn = 0, int mx = 255, int def = 0)
    {
        return add (std::move (id), std::move (name), Addr::CcPair, msb, lsb, mn, mx, def);
    }
    ParamDef& nrpn (std::string id, std::string name, int msb, int lsb, int mn = 0, int mx = 127, int def = 0)
    {
        return add (std::move (id), std::move (name), Addr::Nrpn, msb, lsb, mn, mx, def);
    }
    ParamDef& patch (std::string id, std::string name, int offset, int mn = 0, int mx = 127, int def = 0)
    {
        auto& p = add (std::move (id), std::move (name), Addr::PatchOnly, -1, -1, mn, mx, def);
        p.offset = offset;
        return p;
    }
};

ParamDef& bip (ParamDef& p, int center)
{
    p.kind = Kind::Bipolar;
    p.center = center;
    return p;
}
ParamDef& choice (ParamDef& p, const Names& names)
{
    p.kind = Kind::Choice;
    p.choices = names;
    return p;
}
ParamDef& onOff (ParamDef& p)
{
    p.kind = Kind::Bool;
    return p;
}
ParamDef& at (ParamDef& p, int offset)
{
    p.offset = offset;
    return p;
}

// Bytes 346..391 of the patch hold the NRPN MSB 25 parameters at 346 + LSB (community; Arp Chance 25:33 = 100
// sits at byte 379 in "Perc Up GEO").
constexpr int kMsb25Base = 346;
ParamDef& msb25 (Builder& b, std::string id, std::string name, int lsb, int mn = 0, int mx = 127, int def = 0)
{
    return at (b.nrpn (std::move (id), std::move (name), 25, lsb, mn, mx, def), kMsb25Base + lsb);
}

// Mod matrix: 16 slots x 4 bytes from byte 250; FX matrix: 4 slots x 4 bytes from byte 314.
constexpr int kModMatrixBase = 250;
constexpr int kFxMatrixBase = 314;

std::vector<ParamDef> build()
{
    Builder b;

    // ---- Voice / Glide (bytes 32-39 follow the manual table's order: likely) ----
    b.section = Section::Voice;
    at (choice (b.nrpn ("category", "Patch Category", 0, 0, 0, 14, 0), kCategory), 16);
    at (choice (b.nrpn ("genre", "Patch Genre", 0, 1, 0, 9, 0), kGenre), 17);
    at (b.nrpn ("voice_mode", "Voice Mode", 0, 2, 0, 4, 3), 32);
    at (b.nrpn ("unison", "Unison", 0, 3, 0, 4, 0), 33);
    at (b.nrpn ("unison_detune", "Unison Detune", 0, 4, 0, 127, 25), 34);
    at (b.nrpn ("spread", "Spread", 0, 5, 0, 127, 0), 35);
    at (bip (b.nrpn ("kbd_octave", "Keyboard Octave", 0, 6, 61, 67, 64), 64), 36);
    at (b.cc ("glide_time", "Glide Time", 5, 0, 127, 60), 37);
    at (bip (b.nrpn ("pre_glide", "Pre-Glide", 0, 7, 52, 76, 64), 64), 38);
    at (onOff (b.cc ("glide_on", "Glide On", 35, 0, 1, 0)), 39);
    b.nrpn ("pan", "Pan Position", 0, 51, 0, 127, 0);
    choice (b.nrpn ("spread_mode", "Spread Mode", 0, 52, 0, 3, 0), kSpreadMode);

    // ---- Oscillators ----
    b.section = Section::Oscillators;
    b.nrpn ("osc_divergence", "Osc Divergence", 0, 9);
    b.nrpn ("osc_drift", "Osc Drift", 0, 10);
    b.nrpn ("noise_lpf", "Noise LPF", 0, 11, 0, 127, 127);
    b.nrpn ("noise_hpf", "Noise HPF", 0, 12);
    msb25 (b, "fm_3to1", "FM Osc3>1", 13);
    msb25 (b, "fm_1to2", "FM Osc1>2", 17);
    msb25 (b, "fm_2to3", "FM Osc2>3", 21);

    struct OscNums
    {
        int range, coarse, coarseL, fine, fineL, env2Pitch, lfo2Pitch, lfo2PitchL;
        int wave, waveMore, shapeSrc, manualShape, env1Shape, lfo1Shape, vsync;
        int sawDensity, sawDetune, fixedNote, bendRange;
    };
    const OscNums osc[3] = {
        { 3, 14, 46, 15, 47, 9, 16, 48, 14, 15, 16, 12, 119, 33, 34, 17, 18, 19, 20 },
        { 37, 17, 49, 18, 50, 38, 19, 51, 23, 24, 25, 39, 40, 41, 42, 26, 27, 28, 29 },
        { 65, 20, 52, 21, 53, 43, 22, 54, 32, 33, 34, 71, 72, 73, 44, 35, 36, 37, 38 },
    };
    for (int i = 0; i < 3; ++i)
    {
        const auto& o = osc[i];
        const std::string k = "osc" + std::to_string (i + 1) + "_";
        const std::string n = "Osc " + std::to_string (i + 1) + " ";
        choice (b.cc (k + "range", n + "Range", o.range, 63, 66, 64), kOscRange);
        bip (b.pair (k + "coarse", n + "Coarse", o.coarse, o.coarseL, 0, 255, 128), 128);
        bip (b.pair (k + "fine", n + "Fine", o.fine, o.fineL, 28, 228, 128), 128);
        bip (b.cc (k + "env2_pitch", n + "ModEnv2 > Pitch", o.env2Pitch, 1, 127, 64), 64);
        bip (b.pair (k + "lfo2_pitch", n + "LFO2 > Pitch", o.lfo2Pitch, o.lfo2PitchL, 1, 255, 128), 128);
        choice (b.nrpn (k + "wave", n + "Wave", 0, o.wave, 0, 4, 2), kOscWave);
        choice (b.nrpn (k + "wavetable", n + "Wavetable", 0, o.waveMore, 4, 63, 4), kWavetables);
        choice (b.nrpn (k + "shape_src", n + "Shape Source", 0, o.shapeSrc, 0, 2, 0), kShapeSource);
        bip (b.cc (k + "shape", n + "Manual Shape", o.manualShape, 0, 127, 64), 64);
        bip (b.cc (k + "env1_shape", n + "ModEnv1 > Shape", o.env1Shape, 0, 127, 64), 64);
        bip (b.cc (k + "lfo1_shape", n + "LFO1 > Shape", o.lfo1Shape, 0, 127, 64), 64);
        b.cc (k + "vsync", n + "Vsync", o.vsync);
        b.nrpn (k + "saw_density", n + "Saw Density", 0, o.sawDensity);
        b.nrpn (k + "saw_detune", n + "Saw Density Detune", 0, o.sawDetune, 0, 127, 64);
        b.nrpn (k + "fixed_note", n + "Fixed Note", 0, o.fixedNote, 0, 88, 0);
        bip (b.nrpn (k + "bend", n + "Bend Range", 0, o.bendRange, 40, 88, 76), 64);
    }

    // ---- Mixer ----
    b.section = Section::Mixer;
    b.pair ("mix_osc1", "Osc 1 Level", 23, 55, 0, 255, 255);
    b.pair ("mix_osc2", "Osc 2 Level", 24, 56);
    b.pair ("mix_osc3", "Osc 3 Level", 25, 57);
    b.pair ("mix_ring", "Ring 1x2 Level", 26, 58);
    b.pair ("mix_noise", "Noise Level", 27, 59);
    b.nrpn ("patch_level", "Patch Level", 0, 41, 0, 127, 64);
    b.nrpn ("vca_gain", "VCA Gain", 0, 42, 0, 127, 127);
    b.nrpn ("fx_dry", "FX Dry Level", 0, 43, 0, 127, 127);
    b.nrpn ("fx_wet", "FX Wet Level", 0, 44, 0, 127, 127);

    // ---- Filter ----
    b.section = Section::Filter;
    b.cc ("flt_overdrive", "Overdrive", 80);
    b.cc ("flt_postdrive", "Post Drive", 36);
    choice (b.nrpn ("flt_slope", "Filter Slope", 0, 45, 0, 1, 1), kFilterSlope);
    choice (b.nrpn ("flt_shape", "Filter Shape", 0, 46, 0, 2, 0), kFilterShape);
    b.cc ("flt_keytrack", "Key Tracking", 75, 0, 127, 127);
    b.cc ("flt_res", "Resonance", 79);
    b.pair ("flt_freq", "Frequency", 29, 61, 0, 255, 255);
    bip (b.pair ("flt_lfo1", "LFO1 > Filter", 28, 60, 1, 255, 128), 128);
    b.cc ("flt_osc3", "Osc3 > Filter", 76);
    choice (b.nrpn ("flt_env_sel", "Filter Env Select", 0, 47, 0, 1, 1), kFilterEnv);
    bip (b.cc ("flt_ampenv", "AmpEnv > Filter", 77, 1, 127, 64), 64);
    bip (b.cc ("flt_modenv1", "ModEnv1 > Filter", 78, 1, 127, 64), 64);
    b.nrpn ("flt_divergence", "Filter Divergence", 0, 48);

    // ---- Envelopes ----
    b.section = Section::Envelopes;
    struct EnvNums
    {
        const char* key;
        const char* name;
        int a, d, s, r, vel, trig;
        int defD, defS, defR, defTrig;
    };
    const EnvNums envs[3] = {
        { "amp", "Amp Env", 86, 87, 88, 89, 55, 56, 90, 127, 40, 0 },
        { "mod1", "Mod Env 1", 90, 91, 92, 93, 60, 61, 75, 35, 45, 1 },
        { "mod2", "Mod Env 2", 94, 95, 117, 103, 64, 65, 75, 35, 45, 1 },
    };
    for (const auto& e : envs)
    {
        const std::string k = std::string (e.key) + "_";
        const std::string n = std::string (e.name) + " ";
        b.cc (k + "attack", n + "Attack", e.a, 0, 127, 2);
        b.cc (k + "decay", n + "Decay", e.d, 0, 127, e.defD);
        b.cc (k + "sustain", n + "Sustain", e.s, 0, 127, e.defS);
        b.cc (k + "release", n + "Release", e.r, 0, 127, e.defR);
        bip (b.nrpn (k + "velocity", n + "Velocity", 0, e.vel, 0, 127, 64), 64);
        choice (b.nrpn (k + "trigger", n + "Trigger", 0, e.trig, 0, 1, e.defTrig), kTrigger);
    }
    choice (b.nrpn ("modenv_select", "Mod Env Select", 0, 59, 0, 1, 0), kModEnvSelect);
    msb25 (b, "anim1_attack", "Animate 1 Attack", 34);
    msb25 (b, "anim1_release", "Animate 1 Release", 35);
    msb25 (b, "anim2_attack", "Animate 2 Attack", 36);
    msb25 (b, "anim2_release", "Animate 2 Release", 37);

    // ---- LFOs ----
    b.section = Section::Lfo;
    struct LfoNums
    {
        int rangeMsb, rangeNum;  // LFO 1 Range is NRPN 0:68, LFO 2 Range is CC 83 (asymmetric in the manual)
        int rate, rateL, sync, wave, phase, slew, fade, fadeMode, oneShot, common;
        int defSync;
    };
    const LfoNums lfos[2] = {
        { 0, 68, 30, 62, 81, 69, 70, 71, 82, 72, 75, 76, 16 },
        { -1, 83, 31, 63, 84, 78, 79, 80, 85, 81, 84, 85, 12 },
    };
    for (int i = 0; i < 2; ++i)
    {
        const auto& l = lfos[i];
        const std::string k = "lfo" + std::to_string (i + 1) + "_";
        const std::string n = "LFO " + std::to_string (i + 1) + " ";
        if (l.rangeMsb < 0)
            choice (b.cc (k + "range", n + "Range", l.rangeNum, 0, 2, 0), kLfoRange);
        else
            choice (b.nrpn (k + "range", n + "Range", l.rangeMsb, l.rangeNum, 0, 2, 0), kLfoRange);
        b.pair (k + "rate", n + "Rate", l.rate, l.rateL, 0, 255, 128);
        choice (b.cc (k + "sync", n + "Sync Rate", l.sync, 0, 34, l.defSync), kLfoSync);
        choice (b.nrpn (k + "wave", n + "Wave", 0, l.wave, 0, 3, 0), kLfoWave);
        b.nrpn (k + "phase", n + "Phase", 0, l.phase, 0, 120, 0);
        b.nrpn (k + "slew", n + "Slew", 0, l.slew);
        b.cc (k + "fade", n + "Fade Time", l.fade);
        choice (b.nrpn (k + "fade_mode", n + "Fade Mode", 0, l.fadeMode, 0, 3, 0), kLfoFade);
        onOff (b.nrpn (k + "oneshot", n + "One Shot", 0, l.oneShot, 0, 1, 0));
        onOff (b.nrpn (k + "common", n + "Common", 0, l.common, 0, 1, 0));
    }
    for (int i = 0; i < 2; ++i)
    {
        const int base = 38 + 3 * i;  // LFO 3: 25:38-40, LFO 4: 25:41-43
        const std::string k = "lfo" + std::to_string (i + 3) + "_";
        const std::string n = "LFO " + std::to_string (i + 3) + " ";
        msb25 (b, k + "phase", n + "Phase", base, 0, 120, 0);
        msb25 (b, k + "slew", n + "Slew", base + 1);
        msb25 (b, k + "fade", n + "Fade Time", base + 2);
    }

    // ---- Effects ----
    b.section = Section::Effects;
    b.cc ("dist_level", "Distortion Level", 104);
    onOff (b.nrpn ("fx_bypass", "FX Bypass", 0, 88, 0, 1, 0));
    b.nrpn ("fx_routing", "FX Routing", 0, 89, 0, 6, 0);
    b.cc ("dly_level", "Delay Level", 108);
    b.cc ("dly_time", "Delay Time", 109, 0, 127, 64);
    b.nrpn ("dly_width", "Delay Width", 0, 92, 0, 127, 127);
    onOff (b.nrpn ("dly_sync", "Delay Sync", 0, 93, 0, 1, 0));
    choice (b.nrpn ("dly_sync_time", "Delay Sync Time", 0, 94, 0, 15, 8), kDelaySync);  // default 4th T
    b.cc ("dly_feedback", "Delay Feedback", 110, 0, 127, 64);
    b.nrpn ("dly_lp_damp", "Delay LP Damp", 0, 95, 0, 127, 85);
    b.nrpn ("dly_hp_damp", "Delay HP Damp", 0, 96);
    b.nrpn ("dly_slew", "Delay Slew Rate", 0, 97, 0, 127, 32);
    choice (b.nrpn ("dly_time_mode", "Delay Time Mode", 0, 98, 0, 4, 0), kDelayTimeMode);
    b.cc ("rev_level", "Reverb Level", 112);
    choice (b.nrpn ("rev_type", "Reverb Type", 0, 101, 0, 2, 2), kFxType);
    b.cc ("rev_time", "Reverb Time", 113, 0, 127, 90);
    b.nrpn ("rev_damp_lp", "Reverb Damp LP", 0, 102, 0, 127, 50);
    b.nrpn ("rev_damp_hp", "Reverb Damp HP", 0, 103, 0, 127, 1);
    b.nrpn ("rev_size", "Reverb Size", 0, 104, 0, 127, 64);
    b.nrpn ("rev_mod", "Reverb Mod", 0, 105, 0, 127, 64);
    b.nrpn ("rev_mod_rate", "Reverb Mod Rate", 0, 106, 0, 127, 4);
    b.nrpn ("rev_lowpass", "Reverb Low Pass", 0, 107, 0, 127, 74);
    b.nrpn ("rev_highpass", "Reverb High Pass", 0, 108);
    b.nrpn ("rev_predelay", "Reverb Pre Delay", 0, 109, 0, 127, 40);
    b.cc ("cho_level", "Chorus Level", 105);
    choice (b.nrpn ("cho_type", "Chorus Type", 0, 111, 0, 2, 2), kFxType);
    b.cc ("cho_rate", "Chorus Rate", 118, 0, 127, 20);
    b.nrpn ("cho_depth", "Chorus Mod Depth", 0, 112, 0, 127, 64);
    bip (b.cc ("cho_feedback", "Chorus Feedback", 107, 0, 127, 64), 64);
    b.nrpn ("cho_lp", "Chorus LP", 0, 113, 0, 127, 90);
    b.nrpn ("cho_hp", "Chorus HP", 0, 114, 0, 127, 2);
    choice (b.nrpn ("cho_mode", "Chorus Mode", 0, 115, 0, 2, 0), kChorusMode);

    // ---- Arp / Animate ----
    b.section = Section::Arp;
    choice (b.nrpn ("arp_sync", "Arp Sync Rate", 0, 116, 0, 18, 15), kArpSync);
    choice (b.nrpn ("arp_type", "Arp Type", 0, 117, 0, 7, 0), kArpType);
    b.nrpn ("arp_rhythm", "Arp Rhythm", 0, 118, 0, 32, 0);
    b.nrpn ("arp_octaves", "Arp Octaves", 0, 119, 0, 5, 1);
    b.cc ("arp_gate", "Arp Gate", 116, 0, 127, 64);
    b.nrpn ("arp_swing", "Arp Swing", 0, 120, 20, 80, 50);
    onOff (b.nrpn ("arp_on", "Arp On", 0, 121, 0, 1, 0));
    onOff (b.nrpn ("arp_latch", "Arp Key Latch", 0, 122, 0, 1, 0));
    onOff (b.nrpn ("arp_keysync", "Arp Key Sync", 0, 123, 0, 1, 0));
    msb25 (b, "arp_chance", "Arp Chance", 33, 1, 100, 100);
    // The Animate buttons send 127 when pressed and 0 when released (confirmed), not the manual's 0-1.
    onOff (b.cc ("anim1_hold", "Animate 1 Hold", 114, 0, 1, 0)).onValue = 127;
    onOff (b.cc ("anim2_hold", "Animate 2 Hold", 115, 0, 1, 0)).onValue = 127;

    // ---- Mod matrix: slot n = NRPN n:0..3 ----
    b.section = Section::ModMatrix;
    for (int s = 1; s <= 16; ++s)
    {
        const std::string k = "mod" + std::to_string (s) + "_";
        const std::string n = "Mod " + std::to_string (s) + " ";
        const int off = kModMatrixBase + 4 * (s - 1);
        const int srcMax = (int) kModSource.size() - 1, dstMax = (int) kModDest.size() - 1;
        at (choice (b.nrpn (k + "src_a", n + "Source A", s, 0, 0, srcMax, 0), kModSource), off);
        at (choice (b.nrpn (k + "src_b", n + "Source B", s, 1, 0, srcMax, 0), kModSource), off + 1);
        at (bip (b.nrpn (k + "depth", n + "Depth", s, 2, 0, 127, 64), 64), off + 2);
        at (choice (b.nrpn (k + "dest", n + "Destination", s, 3, 0, dstMax, 0), kModDest), off + 3);
    }

    // ---- FX mod matrix: no published NRPN, patch only ----
    b.section = Section::FxMatrix;
    for (int s = 1; s <= 4; ++s)
    {
        const std::string k = "fxmod" + std::to_string (s) + "_";
        const std::string n = "FX Mod " + std::to_string (s) + " ";
        const int off = kFxMatrixBase + 4 * (s - 1);
        const int srcMax = (int) kFxSource.size() - 1, dstMax = (int) kFxDest.size() - 1;
        choice (b.patch (k + "src_a", n + "Source A", off, 0, srcMax, 0), kFxSource);
        choice (b.patch (k + "src_b", n + "Source B", off + 1, 0, srcMax, 0), kFxSource);
        bip (b.patch (k + "depth", n + "Depth", off + 2, 0, 127, 64), 64);
        choice (b.patch (k + "dest", n + "Destination", off + 3, 0, dstMax, 0), kFxDest);
    }

    return std::move (b.out);
}

struct Index
{
    std::map<std::string, int> byId;
    std::map<int, int> byCc;
    std::map<int, int> byNrpn;
};

const Index& index()
{
    static const Index idx = [] {
        Index x;
        const auto& t = paramTable();
        for (int i = 0; i < (int) t.size(); ++i)
        {
            const auto& p = t[(size_t) i];
            x.byId[p.id] = i;
            if (p.addr == Addr::Cc || p.addr == Addr::CcPair)
                x.byCc[p.num] = i;
            if (p.addr == Addr::CcPair)
                x.byCc[p.num2] = i;
            if (p.addr == Addr::Nrpn)
                x.byNrpn[p.num * 128 + p.num2] = i;
        }
        return x;
    }();
    return idx;
}

int find (const std::map<int, int>& m, int key)
{
    auto it = m.find (key);
    return it == m.end() ? -1 : it->second;
}

} // namespace

const std::vector<ParamDef>& paramTable()
{
    static const std::vector<ParamDef> table = build();
    return table;
}

int indexOfId (const std::string& id)
{
    const auto& m = index().byId;
    auto it = m.find (id);
    return it == m.end() ? -1 : it->second;
}

int indexOfCc (int controller) { return find (index().byCc, controller); }

int indexOfNrpn (int msb, int lsb) { return find (index().byNrpn, msb * 128 + lsb); }

int clampValue (const ParamDef& p, int value) { return std::clamp (value, p.min, p.max); }

std::string formatValue (const ParamDef& p, int value)
{
    value = clampValue (p, value);
    switch (p.kind)
    {
        case Kind::Bool:
            return value ? "On" : "Off";
        case Kind::Choice:
        {
            const auto i = (size_t) (value - p.min);
            return i < p.choices.size() ? p.choices[i] : std::to_string (value);
        }
        case Kind::Bipolar:
        {
            const int v = value - p.center;
            return v > 0 ? "+" + std::to_string (v) : std::to_string (v);
        }
        case Kind::Int:
            break;
    }
    if (p.id.size() > 10 && p.id.compare (p.id.size() - 10, 10, "fixed_note") == 0 && value == 0)
        return "Off";
    return std::to_string (value);
}

} // namespace peak
