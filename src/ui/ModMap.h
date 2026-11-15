// Which knob a mod-matrix destination moves, for the orange modulation rings and for dragging a source onto a knob.
// Plain C++ (tested). Destination / source numbers index the Mod / FX matrix choice lists in ParamTable.cpp.
#pragma once

#include "core/Patch.h"

#include <string>
#include <vector>

namespace peak::ui
{

// The parameter a destination is drawn on ("" = none, e.g. O123Ptch which moves several).
std::string paramForModDest (int dest);
std::string paramForFxDest (int dest);

// The destination for a knob, or -1.
int modDestForParam (const std::string& id);
int fxDestForParam (const std::string& id);

// The FX matrix source with the same name as a mod matrix source, or -1 (LFO 1/2 and envelopes can't reach FX).
int fxSourceForModSource (int modSource);

// Total modulation depth on a knob, -1..1 (sum of slot depths aimed at it, 64 = 0), from the current values.
double modAmountFor (const std::string& id, const Values& values);

struct SlotTarget
{
    bool fx = false;
    int slot = 0;  // 1-based; 0 = no free slot or no destination
    int dest = -1, source = -1;
};
// Where dropping `modSource` on knob `id` would go: the first empty slot (sources Direct/Direct, depth 0).
SlotTarget slotForDrop (int modSource, const std::string& id, const Values& values);

} // namespace peak::ui
