// What goes to the synth when parameter values change: live CC / NRPN for parameters that have one, and a request
// for a whole-patch rewrite (edit buffer) for those that don't. Built on SendQueue, so turning a knob sends its
// latest value at a steady rate instead of every step.
//
// Not thread-safe: the processor calls it under its own lock (audio thread in host mode, pump thread otherwise).
#pragma once

#include "Midi.h"
#include "SendQueue.h"

#include <cstdint>

namespace peak
{

class SyncEngine
{
public:
    SyncEngine();

    // The synth already has these values (after Fetch, a loaded project, or a value the synth itself sent).
    void markSynced (size_t index, int value) { queue.markSynced (index, value); }
    void markAllSynced (const int* values, size_t count);
    void markAllDirty() { queue.markAllDirty(); }  // "send everything again"

    // Compare the wanted values with what was sent; for each due parameter call
    //   emitCc (index, value, ccs, count)   for a live parameter (ccs: 1-3 CCs in sending order), or
    //   patchOnly (index, value)            for a patch-only parameter (the caller schedules a rewrite).
    // getValue (index) returns the wanted value.
    template <typename Get, typename EmitCc, typename PatchOnly>
    void collect (double nowMs, Get&& getValue, EmitCc&& emitCc, PatchOnly&& patchOnly)
    {
        const auto& t = paramTable();
        for (size_t i = 0; i < t.size(); ++i)
            queue.setValue (i, getValue (i));
        queue.collect (nowMs, [&] (size_t i, int value)
        {
            Cc ccs[3];
            const int n = encodeParamInto (t[i], value, ccs);
            if (n > 0)
                emitCc (i, value, ccs, n);
            else
                patchOnly (i, value);
        });
    }

private:
    SendQueue queue;
};

} // namespace peak
