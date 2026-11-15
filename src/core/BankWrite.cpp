#include "BankWrite.h"

#include "Library.h"

#include <algorithm>

namespace peak
{

namespace
{
std::string bankLetter (int bank) { return std::string (1, (char) ('A' + std::clamp (bank, 1, 4) - 1)); }
} // namespace

BankWrite::BankWrite (int bank, std::vector<Patch> list, double writeGapMs, double settleMs, double readTimeoutMs)
    : b (bank), patches (std::move (list)), gap (writeGapMs), settle (settleMs), readTimeout (readTimeoutMs)
{
    std::sort (patches.begin(), patches.end(), [] (const Patch& x, const Patch& y) { return x.slot < y.slot; });
    readBackDone.assign (patches.size(), false);
    plan = makePlan();
    msg = "Backing up bank " + bankLetter (b) + "...";
    if (patches.empty())
        fail ("Nothing to send.");
}

std::unique_ptr<FetchPlan> BankWrite::makePlan() const
{
    std::vector<FetchPlan::Item> items;
    for (const auto& p : patches)
        items.push_back ({ b, p.slot });
    return std::make_unique<FetchPlan> (std::move (items), readTimeout, 2);
}

void BankWrite::fail (std::string why)
{
    st = Step::Failed;
    msg = std::move (why);
}

float BankWrite::progress() const
{
    const float n = (float) std::max<size_t> (1, patches.size());
    switch (st)
    {
        case Step::ReadBackup: return 0.33f * (float) plan->finished() / n;
        case Step::WaitBackupStored: return 0.33f;
        case Step::Write: return 0.33f + 0.34f * (float) writtenCount / n;
        case Step::Settle: return 0.67f;
        case Step::ReadBack: return 0.67f + 0.33f * (float) plan->finished() / n;
        case Step::Done:
        case Step::Failed: break;
    }
    return 1.0f;
}

BankWrite::Action BankWrite::action (double nowMs)
{
    Action a;
    switch (st)
    {
        case Step::ReadBackup:
            if (auto it = plan->next (nowMs))
                return { Action::Kind::RequestSlot, it->bank, it->slot, nullptr };
            if (plan->done())
            {
                if (plan->failed() > 0)
                    fail (std::to_string (plan->failed()) + " slots of bank " + bankLetter (b)
                          + " did not answer, so nothing was written.");
                else
                {
                    st = Step::WaitBackupStored;
                    msg = "Saving the backup of bank " + bankLetter (b) + "...";
                }
            }
            return a;
        case Step::WaitBackupStored:
            if (backupAnswered)
            {
                if (! backupOk)
                    fail ("The backup could not be saved, so nothing was written.");
                else
                {
                    st = Step::Write;
                    msg = "Writing bank " + bankLetter (b) + "...";
                }
            }
            return a;
        case Step::Write:
            if (writeNext >= patches.size())
            {
                st = Step::Settle;
                settleFrom = nowMs;
                return a;
            }
            if (nowMs - lastWrite < gap)
                return a;
            lastWrite = nowMs;
            ++writtenCount;
            {
                const auto& p = patches[writeNext++];
                msg = "Writing bank " + bankLetter (b) + ": " + std::to_string (writtenCount) + " / " + std::to_string (patches.size());
                return { Action::Kind::WriteSlot, b, p.slot, &p.data };
            }
        case Step::Settle:
            if (nowMs - settleFrom >= settle)
            {
                st = Step::ReadBack;
                plan = makePlan();
                msg = "Checking bank " + bankLetter (b) + "...";
            }
            return a;
        case Step::ReadBack:
            if (auto it = plan->next (nowMs))
                return { Action::Kind::RequestSlot, it->bank, it->slot, nullptr };
            if (plan->done())
            {
                const int missing = plan->failed();
                if (checkedBad == 0 && missing == 0)
                {
                    st = Step::Done;
                    msg = "Bank " + bankLetter (b) + ": " + std::to_string (writtenCount) + " written, "
                        + std::to_string (checkedOk) + " checked.";
                }
                else
                    fail ("Bank " + bankLetter (b) + ": " + std::to_string (writtenCount) + " written, "
                          + std::to_string (checkedBad) + " differ, " + std::to_string (missing)
                          + " did not answer. The backup is kept.");
            }
            return a;
        case Step::Done:
        case Step::Failed:
            break;
    }
    return a;
}

void BankWrite::gotPatch (int bank, int slot, const PatchData& data)
{
    if (bank != b)
        return;
    auto it = std::find_if (patches.begin(), patches.end(), [slot] (const Patch& p) { return p.slot == slot; });
    if (it == patches.end())
        return;
    const auto idx = (size_t) (it - patches.begin());
    if (st == Step::ReadBackup)
    {
        if (std::none_of (backupPatches.begin(), backupPatches.end(), [slot] (const Patch& p) { return p.slot == slot; }))
            backupPatches.push_back ({ slot, data });
        plan->gotReply (bank, slot);
    }
    else if (st == Step::ReadBack && ! readBackDone[idx])
    {
        readBackDone[idx] = true;
        (data == it->data ? checkedOk : checkedBad)++;
        plan->gotReply (bank, slot);
    }
}

void BankWrite::backupStored (bool ok)
{
    backupAnswered = true;
    backupOk = ok;
}

} // namespace peak
