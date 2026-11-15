// The current patch on the message thread: its raw bytes (so bytes the editor does not map yet survive a round trip),
// where it came from (a slot, a file, or unknown), the values it was loaded with (for "edited" and reset), Fetch /
// Send / program changes, and the whole-patch rewrite for parameters without a live CC / NRPN.
//
// It reaches the rest of the plugin only through SessionHost and PeakLink, so it can change without touching them.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "core/Patch.h"
#include "core/Timing.h"
#include "link/PeakLink.h"

#include <memory>
#include <string>

class SessionHost
{
public:
    virtual ~SessionHost() = default;
    virtual peak::Values currentValues() const = 0;
    // The synth now has these values: show them, and don't send them back.
    virtual void applyFromSynth (const peak::Values&) = 0;
    // The synth got the whole patch: every current value counts as sent.
    virtual void markAllSynced() = 0;
    virtual int midiChannel() const = 0;
};

class PatchSession
{
public:
    PatchSession (PeakLink&, SessionHost&);

    void tick (double nowMs);                // message thread, ~30 Hz
    void patchOnlyChanged (double nowMs);    // a patch-only parameter changed: schedule a rewrite

    void fetch();                            // read the edit buffer into the editor
    void sendAll();                          // write the editor's patch into the edit buffer
    void loadSlot (int bank, int slot);      // program change, then fetch
    void step (int delta);                   // next / previous slot (A 000 after D 127 wraps)
    void loadFromFile (const peak::PatchData&, const std::string& fileName);  // into the editor and the edit buffer
    void noteSavedToSlot (int bank, int slot);

    peak::PatchData build() const;           // the editor's patch: raw bytes + current values + name
    std::string name() const;
    void setName (const std::string&);

    struct Origin
    {
        enum class Kind { None, Slot, File } kind = Kind::None;
        int bank = 1, slot = 0;
        std::string file;
    };
    const Origin& origin() const { return org; }
    std::string originText() const;          // "A 012", "file: x.syx", "edit buffer"

    bool hasBaseline() const { return ! baseline.empty(); }
    const peak::Values& baselineValues() const { return baseline; }
    int editedCount() const;

    const juce::String& statusText() const { return status; }
    bool busy() const { return fetchPending; }

    juce::ValueTree toState() const;
    void fromState (const juce::ValueTree&);

private:
    void apply (const peak::PatchData&);
    void setSlot (int bank, int slot);

    PeakLink& link;
    SessionHost& host;
    peak::PatchData base;
    peak::Values baseline;
    Origin org;
    juce::String status;
    peak::PatchRewrite rewrite;

    bool fetchPending = false;
    double fetchAskedMs = 0;
    uint32_t patchCursor = 0;
    uint32_t programSeen = 0;
    double autoFetchAtMs = -1;
};
