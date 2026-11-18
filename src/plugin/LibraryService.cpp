#include "LibraryService.h"

#include "core/Patch.h"

#include <set>

using namespace peak;

namespace
{
const char* kCacheName = "library-cache.syx";
const char* kPcOnlyName = "pc-only-slots.txt";      // slots whose newest version is on the PC only
const char* kFirstCopyName = "first-copy-offered.txt";

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

juce::String banksText (const std::vector<int>& banks)
{
    juce::String t;
    for (int b : banks)
        t << juce::String::charToString ((juce::juce_wchar) ('A' + b - 1));
    return t.length() == 4 ? juce::String ("A-D") : t;
}

juce::String stamp() { return juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S"); }

double now() { return juce::Time::getMillisecondCounterHiRes(); }
} // namespace

LibraryService::LibraryService (PeakLink& l, PatchSession& s) : link (l), session (s)
{
    patchCursor = link.patchSeq();
    programSeen = link.lastProgramChange().seq;
    loadCache();
    loadPcOnly();
}

void LibraryService::resync()
{
    patchCursor = link.patchSeq();
    programSeen = link.lastProgramChange().seq;
}

// ---- PC-only slots: "A 012" lines in a small text file next to the cache ----

void LibraryService::loadPcOnly()
{
    juce::StringArray lines;
    userFolder().getChildFile (kPcOnlyName).readLines (lines);
    for (const auto& l : lines)
    {
        const auto t = l.trim();
        if (t.length() == 5 && t[0] >= 'A' && t[0] <= 'D')
            pcOnly.insert (key (t[0] - 'A' + 1, t.substring (2).getIntValue()));
    }
}

void LibraryService::savePcOnly() const
{
    juce::StringArray lines;
    for (int k : pcOnly)
        lines.add (slotLabel (k / 1000, k % 1000));
    const auto f = userFolder().getChildFile (kPcOnlyName);
    f.getParentDirectory().createDirectory();
    f.replaceWithText (lines.joinIntoString ("\r\n"));
}

void LibraryService::saveOnPcOnly (int bank, int slot, const std::string& name)
{
    session.setName (name);
    lib.set (bank, slot, session.build());
    pcOnly.insert (key (bank, slot));
    savePcOnly();
    saveCache();
    session.noteSavedToSlot (bank, slot, true);
    ++rev;
    status = juce::String ("Saved \"") + name + "\" as " + slotLabel (bank, slot) + " on this PC. The Peak's "
           + slotLabel (bank, slot) + " is unchanged: write it from the Presets tab.";
}

void LibraryService::writeToPeak (int bank, int slot)
{
    if (isPcOnly (bank, slot))
        startSlotWrite (bank, slot, lib.at (bank, slot).data, false);
}

// ---- first run ----

bool LibraryService::offerFirstCopy() const
{
    return lib.loadedCount() == 0 && ! userFolder().getChildFile (kFirstCopyName).existsAsFile();
}

void LibraryService::dismissFirstCopy()
{
    const auto f = userFolder().getChildFile (kFirstCopyName);
    f.getParentDirectory().createDirectory();
    f.replaceWithText ("The offer to copy the Peak's patches was shown.");
    ++rev;
}

void LibraryService::copyAll()
{
    dismissFirstCopy();
    readBanks ({ 1, 2, 3, 4 });
}

std::pair<int, int> LibraryService::findSlot (const PatchData& data) const
{
    for (int b = 1; b <= kBanks; ++b)
        for (int s = 0; s < kSlots; ++s)
        {
            const auto& e = lib.at (b, s);
            if (e.loaded && e.data == data)
                return { b, s };
        }
    return { 0, 0 };
}

juce::File LibraryService::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Peak Control");
}

juce::File LibraryService::backupFolder() { return userFolder().getChildFile ("Backups"); }

bool LibraryService::busy() const
{
    return reading != nullptr || (slotWriter && ! slotWriter->finished()) || (bankWriter && ! bankWriter->finished());
}

float LibraryService::progress() const
{
    if (reading)
        return reading->total() ? (float) reading->finished() / (float) reading->total() : 0.0f;
    if (bankWriter && ! bankWriter->finished())
        return bankWriter->progress();
    return -1.0f;
}

// ---- cache ----

void LibraryService::loadCache()
{
    for (const auto& m : parseSyx (readAll (userFolder().getChildFile (kCacheName))))
        if (m.cmd == PatchCmd::WriteSlot)
            lib.set (m.bank, m.slot, m.data);
    ++rev;
}

void LibraryService::saveCache() const { saveToFile (userFolder().getChildFile (kCacheName), { 1, 2, 3, 4 }); }

bool LibraryService::saveToFile (const juce::File& f, const std::vector<int>& banks) const
{
    std::vector<uint8_t> all;
    for (int b : banks)
    {
        const auto bank = bankFile (lib, b);
        all.insert (all.end(), bank.begin(), bank.end());
    }
    return ! all.empty() && writeAll (f, all);
}

bool LibraryService::writeBackup (const juce::String& title, int bank, const std::vector<BankWrite::Patch>& patches) const
{
    std::vector<uint8_t> all;
    for (const auto& p : patches)
    {
        const auto m = writeSlotDump (bank, p.slot, p.data);
        all.insert (all.end(), m.begin(), m.end());
    }
    return writeAll (backupFolder().getChildFile (title + ".syx"), all);
}

// ---- Peak ----

void LibraryService::readBanks (const std::vector<int>& banks)
{
    if (! link.canSend())
    {
        status = "Not connected: see the Setup tab.";
        return;
    }
    if (busy())
        return;
    std::vector<FetchPlan::Item> items;
    for (int b : banks)
        for (int s = 0; s < kSlots; ++s)
            items.push_back ({ b, s });
    reading = std::make_unique<FetchPlan> (std::move (items));
    readingBanks = banks;
    status = "Reading bank " + banksText (banks) + "...";
}

void LibraryService::cancel()
{
    if (reading)
    {
        reading.reset();
        status = "Reading stopped (no backup saved).";
        saveCache();
    }
}

// ---- File ----

bool LibraryService::openFile (const juce::File& f)
{
    const auto msgs = parseSyx (readAll (f));
    if (msgs.empty())
    {
        status = "No Peak patch in " + f.getFileName() + ".";
        return false;
    }
    fileEntries.clear();
    int single = 0;
    for (const auto& m : msgs)
    {
        Library::Entry e;
        e.loaded = true;
        e.data = m.data;
        e.name = patchName (m.data);
        e.category = categoryName (m.data);
        if (m.cmd == PatchCmd::WriteSlot)
        {
            e.bank = m.bank;
            e.slot = m.slot;
        }
        else
        {
            e.bank = 0;  // an edit-buffer message: a single patch with no slot
            e.slot = single++;
        }
        fileEntries.push_back (e);
    }
    fileTitle = f.getFileName();
    ++rev;
    status = juce::String ((int) fileEntries.size()) + " patches in " + fileTitle + ".";
    return true;
}

std::vector<int> LibraryService::fileBanks() const
{
    std::set<int> b;
    for (const auto& e : fileEntries)
        if (e.bank >= 1)
            b.insert (e.bank);
    return { b.begin(), b.end() };
}

void LibraryService::tryPatch (size_t i)
{
    if (i >= fileEntries.size())
        return;
    session.loadFromFile (fileEntries[i].data, (fileTitle + " / " + fileEntries[i].name).toStdString());
    status = session.statusText();
}

void LibraryService::copyToPeak (size_t i, int bank, int slot)
{
    if (i < fileEntries.size())
        startSlotWrite (bank, slot, fileEntries[i].data, false);
}

void LibraryService::sendBank (int fileBank, int targetBank)
{
    if (! link.canSend())
    {
        status = "Not connected: see the Setup tab.";
        return;
    }
    if (busy())
        return;
    std::vector<BankWrite::Patch> patches;
    for (const auto& e : fileEntries)
        if (e.bank == fileBank)
            patches.push_back ({ e.slot, e.data });
    bankWriter = std::make_unique<BankWrite> (targetBank, std::move (patches));
    status = bankWriter->message();
}

// ---- the editor's patch ----

void LibraryService::saveToSlot (int bank, int slot, const std::string& name)
{
    session.setName (name);
    startSlotWrite (bank, slot, session.build(), true);
}

void LibraryService::startSlotWrite (int bank, int slot, const PatchData& data, bool fromEditor)
{
    if (! link.canSend())
    {
        status = "Not connected: see the Setup tab.";
        return;
    }
    if (busy())
        return;
    slotWriter = std::make_unique<SlotWrite> (bank, slot, data);
    slotFromEditor = fromEditor;
    status = slotWriter->message();
}

// ---- tick ----

void LibraryService::tick (double nowMs)
{
    std::vector<PeakLink::PatchIn> in;
    patchCursor = link.readPatches (patchCursor, in);
    for (const auto& p : in)
    {
        const auto& m = p.patch;
        if (m.bank < 1)
            continue;  // edit buffer: PatchSession's
        if (slotWriter && ! slotWriter->finished())
            slotWriter->gotPatch (m.bank, m.slot, m.data, nowMs);
        if (bankWriter && ! bankWriter->finished())
            bankWriter->gotPatch (m.bank, m.slot, m.data);
        if (reading)
            reading->gotReply (m.bank, m.slot);
        if (isPcOnly (m.bank, m.slot))
            continue;  // the PC holds a newer version of this slot; the Peak's older one does not replace it
        lib.set (m.bank, m.slot, m.data);  // whatever the synth sends is what it holds
        ++rev;
    }

    // The Peak switched to another slot (its panel, or a Load here): read that slot again shortly, so a save made
    // with the Peak's own Write button shows up in the PC copy. Read only; skipped while something else runs.
    const auto pc = link.lastProgramChange();
    if (pc.seq != programSeen)
    {
        programSeen = pc.seq;
        rereadBank = pc.bank;
        rereadSlot = pc.slot;
        rereadAtMs = nowMs + 900;  // after PatchSession's own fetch of the edit buffer
    }
    if (rereadAtMs > 0 && nowMs >= rereadAtMs)
    {
        rereadAtMs = -1;
        if (! busy() && link.canSend() && rereadBank >= 1 && rereadBank <= kBanks)
            link.requestPatch (rereadBank, rereadSlot);
    }

    if (reading)
    {
        if (auto item = reading->next (nowMs))
            link.requestPatch (item->bank, item->slot);
        if (reading->done())
        {
            const int ok = reading->finished() - reading->failed();
            status = juce::String ("Read ") + juce::String (ok) + " patches";
            if (reading->failed() > 0)
                status << ", " << reading->failed() << " did not answer (no backup saved)";
            else
            {
                const auto name = "Peak " + banksText (readingBanks) + " " + stamp() + ".syx";
                if (saveToFile (backupFolder().getChildFile (name), readingBanks))
                    status << ". Backup: Backups\\" << name;
            }
            saveCache();
            reading.reset();
        }
        else
            status = "Reading bank " + banksText (readingBanks) + ": " + juce::String (reading->finished()) + " / "
                   + juce::String (reading->total());
    }

    if (slotWriter && ! slotWriter->finished())
    {
        if (slotWriter->step() == SlotWrite::Step::WaitBackup && slotWriter->backup())
            slotWriter->backupStored (writeBackup (stamp() + " " + slotLabel (slotWriter->bank(), slotWriter->slot()) + " "
                                                       + safeName (patchName (*slotWriter->backup())),
                                                   slotWriter->bank(), { { slotWriter->slot(), *slotWriter->backup() } }));
        switch (slotWriter->action (nowMs))
        {
            case SlotWrite::Action::RequestSlot: link.requestPatch (slotWriter->bank(), slotWriter->slot()); break;
            case SlotWrite::Action::WriteSlot: link.writeSlot (slotWriter->bank(), slotWriter->slot(), slotWriter->patch()); break;
            case SlotWrite::Action::None: break;
        }
        status = slotWriter->message();
        if (slotWriter->step() == SlotWrite::Step::Done)
        {
            // The Peak and the PC copy now hold the same patch.
            lib.set (slotWriter->bank(), slotWriter->slot(), slotWriter->patch());
            if (pcOnly.erase (key (slotWriter->bank(), slotWriter->slot())) > 0)
                savePcOnly();
            ++rev;
            if (slotFromEditor)
                session.noteSavedToSlot (slotWriter->bank(), slotWriter->slot(), false);
            saveCache();
        }
    }

    if (bankWriter && ! bankWriter->finished())
    {
        if (bankWriter->step() == BankWrite::Step::WaitBackupStored)
        {
            const auto letter = juce::String::charToString ((juce::juce_wchar) ('A' + bankWriter->bank() - 1));
            bankWriter->backupStored (writeBackup ("Peak " + letter + " " + stamp() + " before send", bankWriter->bank(),
                                                   bankWriter->backup()));
        }
        const auto a = bankWriter->action (nowMs);
        if (a.kind == BankWrite::Action::Kind::RequestSlot)
            link.requestPatch (a.bank, a.slot);
        else if (a.kind == BankWrite::Action::Kind::WriteSlot && a.data != nullptr)
        {
            link.writeSlot (a.bank, a.slot, *a.data);
            lib.set (a.bank, a.slot, *a.data);
            if (pcOnly.erase (key (a.bank, a.slot)) > 0)
                savePcOnly();
            ++rev;
        }
        status = bankWriter->message();
        if (bankWriter->finished())
            saveCache();
    }
}
