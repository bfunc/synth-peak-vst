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
    void saveToSlot (int bank, int slot, const std::string& name);  // the top bar's Save
    bool exportPatch (const juce::File&) const;

    static juce::File userFolder();      // Documents/Peak Control
    static juce::File backupFolder();    // .../Backups

private:
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
};
