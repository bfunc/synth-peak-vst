#include "Help.h"

#include <map>

namespace peak::ui
{

namespace
{
// Keys: a full id, or a generic key for the repeated sections ("osc_*", "env_*", "lfo_*", "mod_*", "fxmod_*").
const std::map<std::string, std::string>& texts()
{
    static const std::map<std::string, std::string> t = {
        // oscillators (osc1_..osc3_)
        { "osc_range", "Range, in organ-stop units: 16' lowest, 2' highest. 8' plays at concert pitch." },
        { "osc_coarse", "Coarse: pitch up or down by as much as one octave, in semitones." },
        { "osc_fine", "Fine: pitch up or down by as much as one semitone, in cents." },
        { "osc_env2_pitch", "Mod Env 2 → pitch. 8 shifts one octave at full envelope; negative makes the pitch fall instead of rise." },
        { "osc_lfo2_pitch", "LFO 2 → pitch (vibrato). Finer steps below ±12; negative inverts the LFO." },
        { "osc_wave", "Wave: Sine, Triangle, rising Saw, Pulse, or one of 60 wavetables. Arrows step one at a time; click the name for the full list." },
        { "osc_wavetable", "Wavetable: one of 60. Shape morphs across its 5 waves." },
        { "osc_shape_src", "Shape source: which amount the Shape knob edits, Manual, Mod Env 1 depth or LFO 1 depth. All three are stored in the patch." },
        { "osc_shape", "Shape (Manual): 0 = classic wave. Pulse: pulse width. Wavetable: morphs across its 5 waves." },
        { "osc_env1_shape", "Shape (Mod Env 1 depth): how far Mod Env 1 moves the shape." },
        { "osc_lfo1_shape", "Shape (LFO 1 depth): how far LFO 1 moves the shape." },
        { "osc_vsync", "V-Sync: a silent virtual oscillator re-triggers this one. Multiples of 16 land on harmonics; in between sounds harsher." },
        { "osc_saw_density", "Saw Density (saw wave only): adds two hidden copies of the saw for a thicker sound without using extra voices." },
        { "osc_saw_detune", "Density Detune (saw wave only): detunes those copies against each other for beating. Does nothing while Density is 0." },
        { "osc_fixed_note", "Fixed Note (Osc menu): Off plays normally. Any note makes every key play that pitch, useful for drums and effects." },
        { "osc_bend", "Bend Range (Osc menu): pitch-wheel range in semitones, up to ±24. Negative reverses the wheel." },
        { "fm_3to1", "FM 3→1 (FW 2.0): Osc 3 frequency-modulates Osc 1." },
        { "fm_1to2", "FM 1→2 (FW 2.0): Osc 1 frequency-modulates Osc 2." },
        { "fm_2to3", "FM 2→3 (FW 2.0): Osc 2 frequency-modulates Osc 3." },
        { "osc_divergence", "Oscillator Diverge (Osc menu): a tiny fixed pitch offset for each of the 24 oscillators, so every voice tunes slightly differently." },
        { "osc_drift", "Oscillator Drift (Osc menu): a very slow LFO detunes the three oscillators against each other. Unlike Diverge, it changes over time." },
        { "noise_lpf", "Noise LPF (Osc menu): low-pass filter on the noise generator. 127 = fully open." },
        { "noise_hpf", "Noise HPF (Osc menu): high-pass filter on the noise generator." },

        // mixer
        { "mix_osc1", "Osc 1 level into the mixer." },
        { "mix_osc2", "Osc 2 level into the mixer." },
        { "mix_osc3", "Osc 3 level into the mixer." },
        { "mix_ring", "Ring modulator level: Osc 1 × Osc 2. Turn on Key Sync (Osc menu) for a repeatable result." },
        { "mix_noise", "Noise level. Tone it with Noise LPF / HPF in the Osc menu." },
        { "vca_gain", "VCA Gain: mixer output level, scaled by the Amp Env. Lower it if a loud mix or high resonance clips." },
        { "patch_level", "Patch Level: volume trim saved with the patch. 0 halves the level, 127 doubles it." },

        // filter
        { "flt_shape", "Filter type: low-pass, band-pass (Frequency sets the centre) or high-pass." },
        { "flt_slope", "Slope: 24 dB rejects frequencies outside the pass-band more steeply than 12 dB." },
        { "flt_freq", "Frequency: cutoff for LP and HP, centre frequency for BP." },
        { "flt_res", "Resonance: boosts frequencies around the cutoff. High values whistle and raise the level." },
        { "flt_keytrack", "Key Track: how far the cutoff follows the note. 127 = one semitone per key (filter plays in tune); 0 = fixed." },
        { "flt_env_sel", "Env Src: which envelope the Env Depth knob edits, Amp Env or Mod Env 1. Both depths are stored and work together." },
        { "flt_ampenv", "Env Depth (Amp Env): Amp Env → cutoff. Negative sweeps the other way." },
        { "flt_modenv1", "Env Depth (Mod Env 1): Mod Env 1 → cutoff. Negative sweeps the other way." },
        { "flt_lfo1", "LFO 1 → cutoff depth. Negative inverts the LFO." },
        { "flt_osc3", "Osc 3 → cutoff: audio-rate filter modulation. The result depends on Osc 3's pitch, wave and shape." },
        { "flt_overdrive", "Overdrive: analogue drive per voice, before the filter." },
        { "flt_postdrive", "Post Drive (Voice menu): drive per voice after the filter and before the VCA, so it stays constant as the Amp Env opens and closes." },
        { "flt_divergence", "Filter Divergence (Voice menu): gives each voice's filter its own fixed detune, like an old analogue poly. Most audible near resonance." },
        { "dist_level", "Distortion: analogue distortion on all voices together, after the VCA, so it reacts to the envelope and to how many notes play. FX Bypass does not switch it off." },

        // envelopes (amp_, mod1_, mod2_)
        { "env_attack", "Attack: time to reach full level. Up to about 18 s." },
        { "env_decay", "Decay: time to fall to the Sustain level. Up to about 22 s." },
        { "env_sustain", "Sustain: level held while the key is down. 0 = silent once the decay ends." },
        { "env_release", "Release: fade-out after the key is let go. Up to about 24 s; the curve is not linear." },
        { "env_velocity", "Velocity: how much playing harder adds to this envelope. 0 = same every time; negative inverts. Try +40 on the Amp Env." },
        { "env_trigger", "Trigger (mono modes only): Re-Trig restarts the envelope on every note; Legato only on the first of overlapping notes." },

        // LFOs (lfo1_..lfo4_)
        { "lfo_wave", "LFO wave: Triangle, falling Saw, Square or Sample & Hold (random steps)." },
        { "lfo_range", "Range: Low runs up to 200 Hz, High up to 1.6 kHz, Sync follows the clock (internal or MIDI)." },
        { "lfo_rate", "Rate: LFO speed." },
        { "lfo_sync", "Sync rate: LFO speed as a division of the clock." },
        { "lfo_fade", "Fade time: how long the Fade Mode takes. 0 = the LFO runs at full depth from the start." },
        { "lfo_phase", "Phase: Free runs in the background; any other value restarts the LFO at that point (0–357°) on every note." },
        { "lfo_slew", "Slew: rounds off sharp edges, e.g. turns square-wave steps into glides. High values may stop the LFO reaching full level." },
        { "lfo_fade_mode", "Fade In / Fade Out ramp the LFO over the Fade time. Gate In starts it after that time; Gate Out stops it." },
        { "lfo_oneshot", "One Shot: the LFO runs a single cycle per note." },
        { "lfo_common", "Common: all notes share one LFO phase (poly modes). Off sounds more natural for vibrato." },

        // voice / glide
        { "voice_mode", "Mono, Mono LG (glide only when legato), Mono 2 (voices rotate), Poly, Poly 2 (repeated notes reuse their voice)." },
        { "glide_on", "Glide On: notes slide from the previous note's pitch. Works best in mono modes; Pre-Glide must be 0." },
        { "glide_time", "Glide time. 90 is about one second. Also sets the Pre-Glide time." },
        { "pre_glide", "Pre-Glide: every note starts up to 12 semitones above or below and glides to its pitch. Needs Glide On. Works well on chords." },
        { "unison", "Unison: voices per note, 1, 2, 3, 4 or 8. More voices per note means fewer notes at once." },
        { "unison_detune", "Unison Detune: detunes the unison voices against each other. Needs Unison above 1." },
        { "spread", "Spread: pans voices across the stereo field, odd voices left, even right." },
        { "spread_mode", "Spread Mode (FW 2.x): how Spread places the voices." },
        { "pan", "Pan position (FW 2.x)." },
        { "kbd_octave", "Keyboard Octave: shifts incoming notes by −3 to +3 octaves." },

        // arp
        { "arp_on", "Arp On: held notes play one at a time in a pattern." },
        { "arp_latch", "Key Latch: the pattern keeps playing after you let go. Keys added while holding join it." },
        { "arp_keysync", "Key Sync (with Latch): new keys restart the pattern at once instead of keeping its rhythm." },
        { "arp_type", "Note order: Up, Down, Up/Down 1 and 2, Played, Random, Chord, Chord 2." },
        { "arp_sync", "Rate: step length as a division of the clock (internal or MIDI)." },
        { "arp_rhythm", "Rhythm: one of 33 built-in patterns. 1 is straight steps; higher numbers add rests, shorter notes and syncopation." },
        { "arp_gate", "Gate: note length as a share of the step. 64 = half a step, 127 = no gap." },
        { "arp_swing", "Swing: 50 = straight. Higher values lengthen the odd steps and shorten the even ones." },
        { "arp_octaves", "Octaves: plays the pattern again in this many octaves above." },
        { "arp_chance", "Chance (FW 2.x): probability that each step plays." },

        // FX
        { "fx_bypass", "Bypass: switches chorus, delay and reverb out." },
        { "fx_routing", "Routing: chorus, delay and reverb in parallel, or in series in any order. Delay before reverb gives each echo its own reverb tail." },
        { "fx_dry", "Dry Level: the untreated signal. Lower it to hear mostly effect." },
        { "fx_wet", "Wet Level: the effect returns (chorus, delay, reverb). 0 = no effect heard." },
        { "cho_mode", "Mode (FW 2.x): Chorus, Phlanger or Flanger." },
        { "cho_type", "Type: 1 is a two-tap chorus, 2 uses four taps, 3 is an ensemble." },
        { "cho_rate", "Rate: speed of the chorus LFO. Slow rates usually work best." },
        { "cho_depth", "Depth: how far the LFO sweeps the delay. 0 = no chorus." },
        { "cho_feedback", "Feedback: output fed back to the input; negative flips its phase. High feedback with low Depth turns the chorus into a flanger." },
        { "cho_lp", "Low Pass: trims the top of the chorus signal." },
        { "cho_hp", "High Pass: trims the bottom of the chorus signal." },
        { "cho_level", "Chorus Level: how much chorus is added to the dry signal." },
        { "dly_time_mode", "Time Mode (FW 2.x): Normal, Double, Treble, or the Lo-Fi modes for longer, grittier echoes." },
        { "dly_sync", "Sync: the Time knob picks a clock division instead of milliseconds. Long divisions at slow tempos are capped at the maximum delay." },
        { "dly_time", "Time: up to about 1.4 s. Turning it while notes play bends their pitch." },
        { "dly_sync_time", "Time: a division of the clock." },
        { "dly_feedback", "Feedback: number of echoes. 0 = one echo, 64 ≈ five or six, 127 = almost endless." },
        { "dly_slew", "Slew Rate: how smoothly Time changes. 127 avoids pitch bends; lower values make them stronger." },
        { "dly_width", "Width: how far left and right the panned echoes go." },
        { "dly_lp_damp", "LP Damp: highs fade faster on each repeat, like echoes off soft surfaces." },
        { "dly_hp_damp", "HP Damp: lows fade faster on each repeat." },
        { "dly_level", "Delay Level: echo volume. 127 = first echo about as loud as the dry note." },
        { "rev_type", "Type: presets for small, medium and large spaces. Sets Size to 0, 64 or 127; fine-tune with the Size knob." },
        { "rev_time", "Time: how long the reverb takes to die away." },
        { "rev_size", "Size: larger values add more, stronger reflections, like a bigger room." },
        { "rev_predelay", "Pre-Delay: gap before the reverb starts. 127 ≈ half a second, like a very large hall." },
        { "rev_mod", "Mod Depth: how much the reverb's own modulator varies the reverb time." },
        { "rev_mod_rate", "Mod Rate: speed of the reverb's modulator." },
        { "rev_damp_lp", "LP Damp: highs die away faster inside the reverb algorithm, like soft surfaces." },
        { "rev_damp_hp", "HP Damp: lows die away faster inside the reverb algorithm." },
        { "rev_lowpass", "Low Pass: simple EQ on the whole reverb (not the dry note)." },
        { "rev_highpass", "High Pass: simple EQ on the whole reverb (not the dry note)." },
        { "rev_level", "Reverb Level: reverb volume." },

        // matrix slots
        { "mod_src_a", "Source A: the modulator. Direct = a constant amount." },
        { "mod_src_b", "Source B: multiplied with Source A. If either is at zero there is no modulation. Direct = Source A alone." },
        { "mod_depth", "Depth: −64 to +63. Negative lowers the destination. Some destinations (V-Sync, FM) only go negative to cancel modulation from another slot." },
        { "mod_dest", "Destination: the parameter being modulated." },
        { "fxmod_src_a", "FX Source A. LFO 1/2 and the envelopes run per voice, so they can't reach the global effects." },
        { "fxmod_src_b", "FX Source B: multiplied with Source A. Direct = Source A alone." },
        { "fxmod_depth", "Depth: −64 to +63." },
        { "fxmod_dest", "FX destination: an effect parameter, LFO 3/4 rate or the noise filters." },
    };
    return t;
}

bool startsWith (const std::string& s, const std::string& p) { return s.compare (0, p.size(), p) == 0; }

// "osc2_shape" -> "osc_shape", "mod1_attack" -> "env_attack", "mod12_depth" -> "mod_depth", ...
std::string genericKey (const std::string& id)
{
    const auto us = id.find ('_');
    if (us == std::string::npos)
        return id;
    const auto head = id.substr (0, us), tail = id.substr (us + 1);
    auto digitsAfter = [&] (const std::string& p)
    {
        if (! startsWith (head, p) || head.size() == p.size())
            return false;
        for (size_t i = p.size(); i < head.size(); ++i)
            if (head[i] < '0' || head[i] > '9')
                return false;
        return true;
    };
    if (digitsAfter ("osc"))
        return "osc_" + tail;
    if (digitsAfter ("lfo"))
        return "lfo_" + tail;
    if (digitsAfter ("fxmod"))
        return "fxmod_" + tail;
    if (head == "amp" || head == "mod1" || head == "mod2")
    {
        static const char* envTails[] = { "attack", "decay", "sustain", "release", "velocity", "trigger" };
        for (const char* e : envTails)
            if (tail == e)
                return "env_" + tail;
    }
    if (digitsAfter ("mod"))
        return "mod_" + tail;
    return id;
}
} // namespace

std::string helpFor (const std::string& id)
{
    const auto& t = texts();
    auto it = t.find (id);
    if (it == t.end())
        it = t.find (genericKey (id));
    return it == t.end() ? std::string() : it->second;
}

} // namespace peak::ui
