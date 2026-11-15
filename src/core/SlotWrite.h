// Saving a patch into a memory slot, the careful way:
//   1. read the slot's current patch (the backup; the caller stores it as a .syx before anything is written),
//   2. write the new patch into the slot,
//   3. read the slot back and compare.
// The caller sends what action() asks for and reports replies; time is injected. Nothing is written unless the
// backup arrived (or the caller explicitly allowed writing without one).
#pragma once

#include "Sysex.h"

#include <optional>
#include <string>

namespace peak
{

class SlotWrite
{
public:
    enum class Step { ReadBackup, WaitBackup, Write, WaitAfterWrite, ReadBack, WaitReadBack, Done, Failed };
    enum class Action { None, RequestSlot, WriteSlot };

    SlotWrite (int bank, int slot, const PatchData& patch, double timeoutMs = 2000.0, double settleMs = 400.0);

    // What to send now (call often). RequestSlot -> requestPatch (bank, slot); WriteSlot -> writeSlotDump (...).
    Action action (double nowMs);
    // A patch message for (bank, slot) arrived.
    void gotPatch (int bank, int slot, const PatchData& data, double nowMs);
    // The caller saved the backup to disk (or failed to): only then is the write allowed.
    void backupStored (bool ok);

    Step step() const { return st; }
    bool finished() const { return st == Step::Done || st == Step::Failed; }
    const std::string& message() const { return msg; }
    const std::optional<PatchData>& backup() const { return backupData; }
    int bank() const { return b; }
    int slot() const { return s; }
    const PatchData& patch() const { return data; }

private:
    void fail (std::string why);

    int b, s;
    PatchData data;
    double timeout, settle;
    Step st = Step::ReadBackup;
    double since = 0;
    bool backupSaved = false, backupAnswered = false;
    std::optional<PatchData> backupData;
    std::string msg;
};

} // namespace peak
