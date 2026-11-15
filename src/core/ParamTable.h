// The Peak patch parameters: one declarative table, the single source of truth.
// Plugin parameters, MIDI sending, patch decoding, the UI and the tests are all built from it.
//
// Sources: Peak User Guide 1.2 (MIDI list pp. 40-43) + firmware 2.1 addendum, see docs/RESEARCH.md.
// Many value lists, patch byte offsets and the 0-255 CC pair coding are still to be confirmed on the synth
// (PLAN.md step 1); entries marked "provisional" in the .cpp may change.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace peak
{

// Sections, in the order the UI shows them.
enum class Section
{
    Voice,
    Oscillators,
    Mixer,
    Filter,
    Envelopes,
    Lfo,
    Effects,
    Arp,
    ModMatrix,
    FxMatrix
};

const char* sectionName (Section s);

// How a parameter reaches the synth live.
enum class Addr
{
    Cc,         // one CC, 0-127
    CcPair,     // two CCs for a 0-255 value: num carries value >> 1, num2 carries (value & 1) << 6
    Nrpn,       // CC 99 = num (MSB), CC 98 = num2 (LSB), CC 6 = value; no CC 38 (the Peak uses CC 38 for Osc 2)
    PatchOnly   // no published CC/NRPN: changed by rewriting the patch into the edit buffer
};

enum class Kind
{
    Int,      // plain number
    Bipolar,  // shown as value - center
    Bool,     // 0 = off, 1 = on
    Choice    // value - min indexes `choices`
};

struct ParamDef
{
    std::string id;    // plugin parameter id; never rename once released (DAW automation uses it)
    std::string name;  // shown in the DAW and the UI
    Section section = Section::Voice;
    Addr addr = Addr::PatchOnly;
    int num = -1;      // Cc: controller; CcPair: MSB controller; Nrpn: number MSB
    int num2 = -1;     // CcPair: LSB controller; Nrpn: number LSB
    int min = 0, max = 127, def = 0;  // value range (inclusive) and Init Patch default
    Kind kind = Kind::Int;
    int center = 0;                    // Bipolar only
    std::vector<std::string> choices;  // Choice only: max - min + 1 names
    int offset = -1;   // byte in the 510 patch data bytes; -1 = not mapped yet
    int onValue = 1;   // Bool CC only: the value sent for "on" (some Peak buttons use 127)
    bool wide() const { return max > 127; }  // 0-255: two bytes in the patch, coded like a CC pair
};

const std::vector<ParamDef>& paramTable();

// Index into paramTable(), or -1.
int indexOfId (const std::string& id);
int indexOfCc (int controller);       // a Cc parameter, or either controller of a CcPair
int indexOfNrpn (int msb, int lsb);

int clampValue (const ParamDef& p, int value);

// Display text for a raw value, e.g. "-12", "On", "Saw".
std::string formatValue (const ParamDef& p, int value);

} // namespace peak
