// The patches stored on the synth and on disk: fetching banks (read-only), the local cache, .syx import / export,
// and saving the editor's patch into a slot with a backup first (core/SlotWrite). Message thread only.
#pragma once

#include <juce_core/juce_core.h>

#include "core/Library.h"
#include "core/SlotWrite.h"
#include "core/Timing.h"
#include "link/PeakLink.h"
#include "plugin/PatchSession.h"

#include <memory>
#include <optional>
#include <vector>

class LibraryService
{
public:
    LibraryService (PeakLink&, PatchSession&);

    void tick (double nowMs);

    // ---- reading the synth's banks ----
    void fetchBanks (const std::vector<int>& banks);  // 1..4
    void cancelFetch();
    bool fetching() const { return plan != nullptr && ! plan->done(); }
    float fetchProgress() const;
    const peak::Library& library() const { return lib; }
    uint32_t revision() const { return rev; }  // grows whenever the library changes (for list refresh)

    // ---- saving into a slot (writes the synth's memory; only after the user confirmed) ----
    void saveToSlot (int bank, int slot, const std::string& name);
    bool saving() const { return writer != nullptr && ! writer->finished(); }

    // ---- files ----
    static juce::File userFolder();      // Documents/Peak Control
    static juce::File backupFolder();    // .../Backups
    bool exportPatch (const juce::File&) const;          // the editor's patch, as an edit-buffer message
    bool exportBank (const juce::File&, int bank) const; // the cached bank, as slot messages
    // A file's patches; one patch loads into the editor (and the edit buffer). Returns how many were found.
    int importFile (const juce::File&);

    const juce::String& statusText() const { return status; }

private:
    void loadCache();
    void saveCache() const;

    PeakLink& link;
    PatchSession& session;
    peak::Library lib;
    uint32_t rev = 1;
    std::unique_ptr<peak::FetchPlan> plan;
    std::unique_ptr<peak::SlotWrite> writer;
    uint32_t patchCursor = 0;
    juce::String status;
};
