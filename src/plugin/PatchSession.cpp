#include "PatchSession.h"

#include "core/Library.h"

using namespace peak;

namespace
{
constexpr double kFetchTimeoutMs = 2500;
constexpr double kFetchAfterProgramMs = 300;
const juce::Identifier kSession { "session" };
const juce::Identifier kBase { "base" };
const juce::Identifier kKind { "originKind" };
const juce::Identifier kBank { "originBank" };
const juce::Identifier kSlot { "originSlot" };
const juce::Identifier kFile { "originFile" };
const juce::Identifier kBaseline { "baseline" };
double now() { return juce::Time::getMillisecondCounterHiRes(); }
} // namespace

PatchSession::PatchSession (PeakLink& l, SessionHost& h) : link (l), host (h), base (initPatch())
{
    patchCursor = link.patchSeq();
    programSeen = link.lastProgramChange().seq;
}

PatchData PatchSession::build() const { return writeValues (base, host.currentValues()); }

std::string PatchSession::name() const { return patchName (base); }

void PatchSession::setName (const std::string& n) { setPatchName (base, n); }

void PatchSession::setSlot (int bank, int slot)
{
    org.kind = Origin::Kind::Slot;
    org.bank = juce::jlimit (1, kBanks, bank);
    org.slot = juce::jlimit (0, kSlots - 1, slot);
}

std::string PatchSession::originText() const
{
    switch (org.kind)
    {
        case Origin::Kind::Slot: return slotLabel (org.bank, org.slot);
        case Origin::Kind::File: return "file: " + org.file;
        case Origin::Kind::None: break;
    }
    return "edit buffer";
}

int PatchSession::editedCount() const
{
    if (baseline.empty())
        return 0;
    return (int) differing (baseline, host.currentValues()).size();
}

void PatchSession::fetch()
{
    if (! link.canSend())
    {
        status = "Not connected: see the Setup tab.";
        return;
    }
    fetchPending = true;
    fetchAskedMs = now();
    status = "Reading the Peak's patch...";
    link.requestEditBuffer();
}

void PatchSession::sendAll()
{
    if (! link.canSend())
    {
        status = "Not connected: see the Setup tab.";
        return;
    }
    rewrite.cancel();
    sentPatch = build();
    link.sendEditBuffer (sentPatch);
    host.markAllSynced();
    status = "Sending the patch to the Peak's edit buffer...";
    verifyAtMs = now() + 250;  // give the Peak a moment, then read it back
}

void PatchSession::loadSlot (int bank, int slot)
{
    setSlot (bank, slot);
    link.sendProgramChange (host.midiChannel(), org.bank, org.slot);
    autoFetchAtMs = now() + kFetchAfterProgramMs;
    status = juce::String ("Loading ") + slotLabel (org.bank, org.slot) + "...";
}

void PatchSession::step (int delta)
{
    int bank = org.kind == Origin::Kind::Slot ? org.bank : 1;
    int slot = org.kind == Origin::Kind::Slot ? org.slot + delta : 0;
    int linear = ((bank - 1) * kSlots + slot) % (kBanks * kSlots);
    if (linear < 0)
        linear += kBanks * kSlots;
    loadSlot (linear / kSlots + 1, linear % kSlots);
}

void PatchSession::loadFromFile (const PatchData& data, const std::string& fileName)
{
    base = data;
    const auto r = readValues (data, host.currentValues());
    host.applyFromSynth (r.values);
    baseline = r.values;
    org.kind = Origin::Kind::File;
    org.file = fileName;
    if (link.canSend())
    {
        link.sendEditBuffer (build());
        host.markAllSynced();
        status = juce::String ("Loaded ") + fileName + " and sent it to the edit buffer.";
    }
    else
        status = juce::String ("Loaded ") + fileName + " (not connected, nothing sent).";
}

void PatchSession::noteSavedToSlot (int bank, int slot)
{
    setSlot (bank, slot);
    base = build();
    baseline = host.currentValues();
}

void PatchSession::apply (const PatchData& data)
{
    base = data;
    const auto r = readValues (data, host.currentValues());
    host.applyFromSynth (r.values);
    baseline = r.values;
    status = juce::String ("Got \"") + patchName (data) + "\": " + juce::String (r.read) + " values";
    if (! r.outOfRange.empty())
        status << ", " << (int) r.outOfRange.size() << " out of range (kept)";
}

void PatchSession::patchOnlyChanged (double nowMs) { rewrite.touch (nowMs); }

void PatchSession::tick (double nowMs)
{
    // A patch picked on the synth: that slot is now the origin; read the new sound shortly.
    const auto pc = link.lastProgramChange();
    if (pc.seq != programSeen)
    {
        programSeen = pc.seq;
        if (pc.channel == host.midiChannel())
        {
            setSlot (pc.bank, pc.slot);
            autoFetchAtMs = nowMs + kFetchAfterProgramMs;
        }
    }
    if (autoFetchAtMs > 0 && nowMs >= autoFetchAtMs && ! fetchPending)
    {
        autoFetchAtMs = -1;
        fetch();
    }

    if (verifyAtMs > 0 && nowMs >= verifyAtMs && ! fetchPending)
    {
        verifyAtMs = -1;
        verifyPending = true;
        verifyAskedMs = nowMs;
        link.requestEditBuffer();
    }

    std::vector<PeakLink::PatchIn> in;
    patchCursor = link.readPatches (patchCursor, in);
    for (const auto& p : in)
    {
        if (p.patch.bank != 0)
            continue;  // a stored slot: LibraryService's
        if (verifyPending)
        {
            verifyPending = false;
            if (p.patch.data == sentPatch)
                status = juce::String ("Sent \"") + patchName (sentPatch) + "\" to the edit buffer and checked.";
            else
            {
                int differ = 0;
                for (size_t i = 0; i < sentPatch.size(); ++i)
                    differ += p.patch.data[i] != sentPatch[i];
                status = "Sent, but the Peak's edit buffer differs in " + juce::String (differ) + " bytes.";
            }
        }
        else if (fetchPending)
        {
            fetchPending = false;
            apply (p.patch.data);
        }
    }
    if (verifyPending && nowMs - verifyAskedMs > kFetchTimeoutMs)
    {
        verifyPending = false;
        status = "Sent, but the Peak did not answer the check. Is the port still open?";
    }
    if (fetchPending && nowMs - fetchAskedMs > kFetchTimeoutMs)
    {
        fetchPending = false;
        status = "No patch came back. Check the Setup tab.";
    }

    if (rewrite.due (nowMs) && link.canSend())
    {
        link.sendEditBuffer (build());
        host.markAllSynced();
    }
}

juce::ValueTree PatchSession::toState() const
{
    juce::ValueTree t (kSession);
    t.setProperty (kBase, juce::Base64::toBase64 (base.data(), base.size()), nullptr);
    t.setProperty (kKind, (int) org.kind, nullptr);
    t.setProperty (kBank, org.bank, nullptr);
    t.setProperty (kSlot, org.slot, nullptr);
    t.setProperty (kFile, juce::String (org.file), nullptr);
    juce::StringArray b;
    for (int v : baseline)
        b.add (juce::String (v));
    t.setProperty (kBaseline, b.joinIntoString (","), nullptr);
    return t;
}

void PatchSession::fromState (const juce::ValueTree& t)
{
    if (! t.hasType (kSession))
        return;
    juce::MemoryOutputStream mo;
    if (juce::Base64::convertFromBase64 (mo, t.getProperty (kBase).toString()) && mo.getDataSize() == base.size())
        std::memcpy (base.data(), mo.getData(), base.size());
    org.kind = (Origin::Kind) juce::jlimit (0, 2, (int) t.getProperty (kKind, 0));
    org.bank = juce::jlimit (1, kBanks, (int) t.getProperty (kBank, 1));
    org.slot = juce::jlimit (0, kSlots - 1, (int) t.getProperty (kSlot, 0));
    org.file = t.getProperty (kFile).toString().toStdString();
    baseline.clear();
    juce::StringArray b;
    b.addTokens (t.getProperty (kBaseline).toString(), ",", "");
    b.removeEmptyStrings();
    if (b.size() == (int) paramTable().size())
        for (const auto& s : b)
            baseline.push_back (s.getIntValue());
}
