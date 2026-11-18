// Patches on the synth and in files. Message thread only.
//
// Two sources, never mixed:
//   Peak  the synth's 4 x 128 patches as last read (cached on disk). Reading saves a dated backup automatically.
//   File  a .syx opened from disk (one patch, a bank, or a full backup).
// Every write to the synth's memory goes through a careful sequence with a backup first and a read-back after:
// core/SlotWrite for one patch, core/BankWrite for a bank.
#pragma once

#include <juce_core/juce_core.h>

#include "core/BankWrite.h"
#include "core/Library.h"
#include "core/SlotWrite.h"
#include "core/Timing.h"
#include "link/PeakLink.h"
#include "plugin/PatchSession.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

class LibraryService
{
public:
    LibraryService (PeakLink&, PatchSession&);

    void tick (double nowMs);
    uint32_t revision() const { return rev; }  // grows whenever a list changes
    const juce::String& statusText() const { return status; }
    bool busy() const;                          // reading or writing: other actions wait
    float progress() const;                     // of the running read / write, 0..1; -1 = none

    // ---- Peak ----
    const peak::Library& library() const { return lib; }
    void readBanks (const std::vector<int>& banks);  // read only; saves a backup .syx when complete
    void cancel();                                   // stops a read (a running write finishes its current step)
    bool saveToFile (const juce::File&, const std::vector<int>& banks) const;  // the cached banks as slot messages

    // ---- File ----
    bool openFile (const juce::File&);               // false: no Peak patch in it
    bool hasFile() const { return ! fileEntries.empty(); }
    const juce::String& fileName() const { return fileTitle; }
    const std::vector<peak::Library::Entry>& file() const { return fileEntries; }  // bank 0 = a single patch
    std::vector<int> fileBanks() const;              // banks that have slot messages in the file
    void tryPatch (size_t fileIndex);                // into the edit buffer (the synth's memory is untouched)
    void copyToPeak (size_t fileIndex, int bank, int slot);    // writes one slot (backup, write, check)
    void sendBank (int fileBank, int targetBank);              // writes a bank (backup, write, check)

    // ---- the editor's patch ----
    void saveToSlot (int bank, int slot, const std::string& name);  // the top bar's Save: the Peak and the PC copy
    void saveOnPcOnly (int bank, int slot, const std::string& name);  // the PC copy only; the Peak's slot stays older

    // ---- slots whose newest version is on the PC only ----
    bool isPcOnly (int bank, int slot) const { return pcOnly.count (key (bank, slot)) > 0; }
    int pcOnlyCount() const { return (int) pcOnly.size(); }
    void writeToPeak (int bank, int slot);   // the PC version into the Peak's slot (backup, write, check)

    // ---- first run: offer to copy all 512 patches to the PC ----
    bool offerFirstCopy() const;
    void copyAll();                          // read all banks (read only) and stop offering
    void dismissFirstCopy();

    // A stored patch identical to `data` (which slot an edit buffer came from); {0, 0} when none.
    std::pair<int, int> findSlot (const peak::PatchData& data) const;

    void resync();                           // skip what arrived while this copy was not the main one

    static juce::File userFolder();      // Documents/Peak Control
    static juce::File backupFolder();    // .../Backups

private:
    static int key (int bank, int slot) { return bank * 1000 + slot; }
    void loadPcOnly();
    void savePcOnly() const;
    void loadCache();
    void saveCache() const;
    void startSlotWrite (int bank, int slot, const peak::PatchData&, bool fromEditor);
    bool writeBackup (const juce::String& title, int bank, const std::vector<peak::BankWrite::Patch>&) const;

    PeakLink& link;
    PatchSession& session;
    peak::Library lib;
    uint32_t rev = 1;
    uint32_t patchCursor = 0;
    juce::String status;

    std::unique_ptr<peak::FetchPlan> reading;
    std::vector<int> readingBanks;
    std::unique_ptr<peak::SlotWrite> slotWriter;
    bool slotFromEditor = false;
    std::unique_ptr<peak::BankWrite> bankWriter;

    std::vector<peak::Library::Entry> fileEntries;
    juce::String fileTitle;

    std::set<int> pcOnly;                // key (bank, slot)
    uint32_t programSeen = 0;            // the Peak switched slots: read that slot again, quietly
    double rereadAtMs = -1;
    int rereadBank = 0, rereadSlot = 0;
};
