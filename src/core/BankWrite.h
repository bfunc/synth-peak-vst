// Sending a whole bank (up to 128 patches) into the synth's memory, the careful way:
//   1. read every target slot (the backup; the caller stores it as a .syx before anything is written),
//   2. write the patches one by one, with a gap so the synth can store each,
//   3. read them all back and compare.
// Like SlotWrite: the caller sends what action() asks for and reports replies; time is injected. Nothing is written
// unless every target slot answered and the backup was stored.
#pragma once

#include "Sysex.h"
#include "Timing.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace peak
{

class BankWrite
{
public:
    struct Patch
    {
        int slot = 0;  // 0..127 in the target bank
        PatchData data {};
    };
    enum class Step { ReadBackup, WaitBackupStored, Write, Settle, ReadBack, Done, Failed };
    struct Action
    {
        enum class Kind { None, RequestSlot, WriteSlot } kind = Kind::None;
        int bank = 0, slot = 0;
        const PatchData* data = nullptr;  // WriteSlot only
    };

    BankWrite (int bank, std::vector<Patch> patches, double writeGapMs = 250.0, double settleMs = 800.0,
               double readTimeoutMs = 1500.0);

    Action action (double nowMs);
    void gotPatch (int bank, int slot, const PatchData& data);
    void backupStored (bool ok);

    Step step() const { return st; }
    bool finished() const { return st == Step::Done || st == Step::Failed; }
    int bank() const { return b; }
    int total() const { return (int) patches.size(); }
    int written() const { return writtenCount; }
    int checked() const { return checkedOk; }
    int mismatched() const { return checkedBad; }
    float progress() const;  // 0..1 over the three phases
    const std::string& message() const { return msg; }
    // The backup: the target slots as they were (filled during ReadBackup).
    const std::vector<Patch>& backup() const { return backupPatches; }

private:
    void fail (std::string why);
    std::unique_ptr<FetchPlan> makePlan() const;

    int b;
    std::vector<Patch> patches;
    double gap, settle, readTimeout;
    Step st = Step::ReadBackup;
    std::unique_ptr<FetchPlan> plan;
    std::vector<Patch> backupPatches;
    std::vector<bool> readBackDone;
    size_t writeNext = 0;
    double lastWrite = -1.0e12, settleFrom = 0;
    int writtenCount = 0, checkedOk = 0, checkedBad = 0;
    bool backupAnswered = false, backupOk = false;
    std::string msg;
};

} // namespace peak
