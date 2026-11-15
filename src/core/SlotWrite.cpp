#include "SlotWrite.h"

#include "Library.h"

namespace peak
{

SlotWrite::SlotWrite (int bank, int slot, const PatchData& patch, double timeoutMs, double settleMs)
    : b (bank), s (slot), data (patch), timeout (timeoutMs), settle (settleMs)
{
    msg = "Reading " + slotLabel (b, s) + " for the backup...";
}

void SlotWrite::fail (std::string why)
{
    st = Step::Failed;
    msg = std::move (why);
}

SlotWrite::Action SlotWrite::action (double nowMs)
{
    switch (st)
    {
        case Step::ReadBackup:
            st = Step::WaitBackup;
            since = nowMs;
            return Action::RequestSlot;
        case Step::WaitBackup:
            if (backupData && backupAnswered)
            {
                if (! backupSaved)
                {
                    fail ("The backup could not be saved, so nothing was written.");
                    return Action::None;
                }
                st = Step::Write;
            }
            else if (! backupData && nowMs - since > timeout)
                fail ("No answer when reading " + slotLabel (b, s) + "; nothing was written.");
            return Action::None;
        case Step::Write:
            st = Step::WaitAfterWrite;
            since = nowMs;
            msg = "Writing " + slotLabel (b, s) + "...";
            return Action::WriteSlot;
        case Step::WaitAfterWrite:
            if (nowMs - since >= settle)
                st = Step::ReadBack;
            return Action::None;
        case Step::ReadBack:
            st = Step::WaitReadBack;
            since = nowMs;
            msg = "Checking " + slotLabel (b, s) + "...";
            return Action::RequestSlot;
        case Step::WaitReadBack:
            if (nowMs - since > timeout)
                fail ("Written, but reading " + slotLabel (b, s) + " back got no answer. Check it on the synth.");
            return Action::None;
        case Step::Done:
        case Step::Failed:
            break;
    }
    return Action::None;
}

void SlotWrite::gotPatch (int bank, int slot, const PatchData& got, double)
{
    if (bank != b || slot != s)
        return;
    if (st == Step::WaitBackup && ! backupData)
    {
        backupData = got;
        msg = "Saving the backup...";
    }
    else if (st == Step::WaitReadBack)
    {
        if (got == data)
        {
            st = Step::Done;
            msg = "Saved to " + slotLabel (b, s) + " and checked.";
        }
        else
            fail ("Written, but " + slotLabel (b, s) + " reads back different. The backup is kept.");
    }
}

void SlotWrite::backupStored (bool ok)
{
    backupAnswered = true;
    backupSaved = ok;
}

} // namespace peak
