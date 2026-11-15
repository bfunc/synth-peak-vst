#include "LibraryService.h"

using namespace peak;

namespace
{
const char* kCacheName = "library-cache.syx";

std::vector<uint8_t> readAll (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb))
        return {};
    const auto* p = static_cast<const uint8_t*> (mb.getData());
    return std::vector<uint8_t> (p, p + mb.getSize());
}

bool writeAll (const juce::File& f, const std::vector<uint8_t>& bytes)
{
    f.getParentDirectory().createDirectory();
    return f.replaceWithData (bytes.data(), bytes.size());
}

juce::String safeName (const std::string& s)
{
    auto n = juce::File::createLegalFileName (juce::String (s).trim());
    return n.isEmpty() ? juce::String ("patch") : n;
}
} // namespace

LibraryService::LibraryService (PeakLink& l, PatchSession& s) : link (l), session (s)
{
    patchCursor = link.patchSeq();
    loadCache();
}

juce::File LibraryService::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Peak Control");
}

juce::File LibraryService::backupFolder() { return userFolder().getChildFile ("Backups"); }

void LibraryService::loadCache()
{
    for (const auto& m : parseSyx (readAll (userFolder().getChildFile (kCacheName))))
        if (m.cmd == PatchCmd::WriteSlot)
            lib.set (m.bank, m.slot, m.data);
    ++rev;
}

void LibraryService::saveCache() const
{
    std::vector<uint8_t> all;
    for (int b = 1; b <= kBanks; ++b)
    {
        const auto bank = bankFile (lib, b);
        all.insert (all.end(), bank.begin(), bank.end());
    }
    writeAll (userFolder().getChildFile (kCacheName), all);
}

void LibraryService::fetchBanks (const std::vector<int>& banks)
{
    if (! link.canSend())
    {
        status = "Not connected: see the Setup tab.";
        return;
    }
    std::vector<FetchPlan::Item> items;
    for (int b : banks)
        for (int s = 0; s < kSlots; ++s)
            items.push_back ({ b, s });
    plan = std::make_unique<FetchPlan> (std::move (items));
    status = "Reading patches...";
}

void LibraryService::cancelFetch()
{
    if (plan)
        plan->cancel();
    status = "Reading stopped.";
    saveCache();
}

float LibraryService::fetchProgress() const
{
    return plan == nullptr || plan->total() == 0 ? 0.0f : (float) plan->finished() / (float) plan->total();
}

void LibraryService::saveToSlot (int bank, int slot, const std::string& name)
{
    if (! link.canSend())
    {
        status = "Not connected: see the Setup tab.";
        return;
    }
    if (saving() || fetching())
        return;
    session.setName (name);
    writer = std::make_unique<SlotWrite> (bank, slot, session.build());
    status = writer->message();
}

bool LibraryService::exportPatch (const juce::File& f) const { return writeAll (f, patchFile (session.build())); }

bool LibraryService::exportBank (const juce::File& f, int bank) const
{
    const auto bytes = bankFile (lib, bank);
    return ! bytes.empty() && writeAll (f, bytes);
}

int LibraryService::importFile (const juce::File& f)
{
    const auto msgs = parseSyx (readAll (f));
    if (msgs.empty())
    {
        status = "No Peak patch in " + f.getFileName() + ".";
        return 0;
    }
    if (msgs.size() == 1)
    {
        session.loadFromFile (msgs[0].data, f.getFileName().toStdString());
        status = session.statusText();
        return 1;
    }
    // Several patches (a bank file): add them to the library view only; nothing is written to the synth.
    int n = 0;
    for (const auto& m : msgs)
        if (m.cmd == PatchCmd::WriteSlot)
        {
            lib.set (m.bank, m.slot, m.data);
            ++n;
        }
    ++rev;
    status = juce::String (n) + " patches from " + f.getFileName() + " shown in the list (not written to the Peak).";
    return n;
}

void LibraryService::tick (double nowMs)
{
    std::vector<PeakLink::PatchIn> in;
    patchCursor = link.readPatches (patchCursor, in);
    for (const auto& p : in)
    {
        const auto& m = p.patch;
        if (m.bank < 1)
            continue;  // edit buffer: PatchSession's
        if (writer != nullptr && ! writer->finished())
            writer->gotPatch (m.bank, m.slot, m.data, nowMs);
        if (plan != nullptr)
            plan->gotReply (m.bank, m.slot);
        lib.set (m.bank, m.slot, m.data);
        ++rev;
    }

    if (plan != nullptr)
    {
        if (auto item = plan->next (nowMs))
            link.requestPatch (item->bank, item->slot);
        if (plan->done())
        {
            status = juce::String ("Read ") + juce::String (plan->finished() - plan->failed()) + " patches";
            if (plan->failed() > 0)
                status << ", " << plan->failed() << " did not answer";
            saveCache();
            plan.reset();
        }
        else
            status = juce::String ("Reading patches: ") + juce::String (plan->finished()) + " / " + juce::String (plan->total());
    }

    if (writer != nullptr && ! writer->finished())
    {
        // The backup goes to disk as soon as it arrives, before anything is written.
        if (writer->step() == SlotWrite::Step::WaitBackup && writer->backup())
        {
            const auto& old = *writer->backup();
            const auto file = backupFolder().getChildFile (
                juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S ") + slotLabel (writer->bank(), writer->slot())
                + " " + safeName (patchName (old)) + ".syx");
            writer->backupStored (writeAll (file, writeSlotDump (writer->bank(), writer->slot(), old)));
        }
        switch (writer->action (nowMs))
        {
            case SlotWrite::Action::RequestSlot: link.requestPatch (writer->bank(), writer->slot()); break;
            case SlotWrite::Action::WriteSlot: link.writeSlot (writer->bank(), writer->slot(), writer->patch()); break;
            case SlotWrite::Action::None: break;
        }
        status = writer->message();
        if (writer->step() == SlotWrite::Step::Done)
        {
            session.noteSavedToSlot (writer->bank(), writer->slot());
            lib.set (writer->bank(), writer->slot(), writer->patch());
            ++rev;
            saveCache();
        }
    }
}
