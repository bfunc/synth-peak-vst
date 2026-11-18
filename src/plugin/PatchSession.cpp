#include "PatchSession.h"

#include "core/Library.h"

using namespace peak;

namespace
{
constexpr double kFetchTimeoutMs = 2500;
constexpr double kFetchAfterProgramMs = 300;
constexpr double kSettleMs = 600;  // after the port opens or a state arrives, before the editor and the Peak meet
constexpr double kOutsideMs = 400;         // after the Peak's sound changed on the Peak, before re-reading it
constexpr double kQuietAfterParamMs = 600; // the editor sent a parameter: wait, so a re-read can't undo a fresh edit
constexpr double kQuietAfterPatchMs = 1000;
const juce::Identifier kPcOnly { "savedOnPcOnly" };
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
    soundSeen = link.soundChangedSeq();
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
    fetchKeepsReference = false;  // this read replaces a watch read still on its way
    watchAsk = false;
    fetchAskedMs = now();
    status = "Reading the Peak's patch...";
    link.requestEditBuffer();
}

void PatchSession::fetchSaved()
{
    if (org.kind != Origin::Kind::Slot)
        return;
    if (! link.canSend())
    {
        status = "Not connected: see the Setup tab.";
        return;
    }
    loadSlot (org.bank, org.slot);
    status = juce::String ("Fetching ") + slotLabel (org.bank, org.slot) + " as saved on the Peak...";
}

void PatchSession::pushAll()
{
    if (! link.canSend())
    {
        push = Push::Failed;
        status = "Not connected: the patch goes to the Peak when it is back.";
        return;
    }
    rewrite.cancel();
    sentPatch = build();
    link.sendEditBuffer (sentPatch);
    patchSentMs = now();
    host.markAllSynced();
    push = Push::Sending;
    status = "Sending this patch to the Peak...";
    verifyAtMs = now() + 250;  // give the Peak a moment, then read it back
}

void PatchSession::loadFromPc (int bank, int slot, const PatchData& data)
{
    base = data;
    const auto r = readValues (data, host.currentValues());
    host.applyFromSynth (r.values);
    baseline = r.values;
    setSlot (bank, slot);
    baselinePcOnly = true;
    if (link.canSend())
    {
        link.sendEditBuffer (build());
        patchSentMs = now();
        host.markAllSynced();
        status = juce::String ("Loaded the PC version of ") + slotLabel (bank, slot) + ".";
    }
    else
        status = juce::String ("Loaded the PC version of ") + slotLabel (bank, slot) + " (not connected, nothing sent).";
}

void PatchSession::resync()
{
    patchCursor = link.patchSeq();
    programSeen = link.lastProgramChange().seq;
    soundSeen = link.soundChangedSeq();
    wasConnected = false;  // this copy now meets the Peak as if it had just connected
}

void PatchSession::checkPeak (std::function<void (PeakCheck, const peak::PatchData&)> done)
{
    checkDone = std::move (done);  // asked in tick(), once no other read is waiting
}

void PatchSession::takePeakSound (const PatchData& data)
{
    const auto current = host.currentValues();
    const auto r = readValues (data, current);
    base = data;  // the Peak's raw bytes and name
    if (differing (r.values, current).empty())
        return;
    host.applyFromSynth (r.values);
    status = juce::String ("The Peak's sound changed on the Peak (e.g. Init Patch): the editor now has \"")
           + patchName (data) + "\".";
}

void PatchSession::watchTick (double nowMs, bool connected)
{
    if (watch == Watch::Off || ! connected || link.isHostOutput())  // through the host, a request can't go out on time
        return;
    if (fetchPending || verifyPending || checkPending || checkDone || pushWanted || autoFetchAtMs > 0 || outsideAtMs > 0
        || push == Push::Sending || rewrite.pending())
        return;
    // Quiet: nothing sent for a second (a knob, Compare, a whole patch), and Compare not held.
    constexpr double kQuietMs = 1000.0;
    if (nowMs - link.lastParamSentMs() < kQuietMs || nowMs - patchSentMs < kQuietMs || nowMs < nextWatchMs)
        return;
    const auto values = host.currentValues();
    if (host.soundingValues() != values)
        return;

    fetchPending = true;
    watchAsk = true;
    fetchAskedMs = nowMs;
    valuesAtWatch = values;
    sentMsAtWatch = link.lastParamSentMs();
    patchMsAtWatch = patchSentMs;
    ++watchStats.asked;
    link.requestEditBuffer();
    const double every = watch == Watch::Every250ms ? 250.0 : 2000.0;
    nextWatchMs = nowMs + juce::jmin (30000.0, every * (double) (1 << juce::jmin (watchFailures, 4)));
}

void PatchSession::takeWatched (const PatchData& data, double nowMs)
{
    const double took = nowMs - fetchAskedMs;  // up to one tick (33 ms) more than the real reply time
    watchStats.lastReplyMs = took;
    watchStats.slowestReplyMs = juce::jmax (watchStats.slowestReplyMs, took);
    ++watchStats.answered;
    watchFailures = 0;

    // Something was sent or edited while the reply was on its way: it may be older than the edit. Skip it.
    const auto current = host.currentValues();
    if (link.lastParamSentMs() != sentMsAtWatch || patchSentMs != patchMsAtWatch || current != valuesAtWatch
        || host.soundingValues() != current)
    {
        ++watchStats.skipped;
        return;
    }
    const auto n = differing (readValues (data, current).values, current).size();
    if (n == 0)
        return;
    takePeakSound (data);
    ++watchStats.taken;
    watchStats.lastTaken = juce::Time::getCurrentTime().formatted ("%H:%M:%S ") + juce::String ((int) n) + " values";
}

void PatchSession::loadSlot (int bank, int slot)
{
    setSlot (bank, slot);
    baselinePcOnly = false;
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
    baselinePcOnly = false;
    org.kind = Origin::Kind::File;
    org.file = fileName;
    if (link.canSend())
    {
        link.sendEditBuffer (build());
        patchSentMs = now();
        host.markAllSynced();
        status = juce::String ("Loaded ") + fileName + " and sent it to the edit buffer.";
    }
    else
        status = juce::String ("Loaded ") + fileName + " (not connected, nothing sent).";
}

void PatchSession::noteSavedToSlot (int bank, int slot, bool pcOnly)
{
    setSlot (bank, slot);
    base = build();
    baseline = host.currentValues();
    baselinePcOnly = pcOnly;
}

void PatchSession::apply (const PatchData& data)
{
    base = data;
    const auto r = readValues (data, host.currentValues());
    host.applyFromSynth (r.values);
    baseline = r.values;
    baselinePcOnly = false;
    status = juce::String ("Got \"") + patchName (data) + "\": " + juce::String (r.read) + " values";
    if (! r.outOfRange.empty())
        status << ", " << (int) r.outOfRange.size() << " out of range (kept)";
    // Not knowing the slot (e.g. on start): a stored patch with the same bytes tells which one it is.
    if (org.kind != Origin::Kind::Slot && findSlot)
    {
        const auto [b, s] = findSlot (data);
        if (b >= 1)
            setSlot (b, s);
    }
}

void PatchSession::patchOnlyChanged (double nowMs) { rewrite.touch (nowMs); }

void PatchSession::tick (double nowMs)
{
    // The editor and the Peak meet: when the port opens (at start or after the Peak was away). A project that brought
    // its own patch pushes it; otherwise the editor takes the Peak's sound. Waits a moment for a host that restores
    // its state more than once.
    const bool connected = link.canSend();
    if (connected && ! wasConnected)
    {
        if (editorIsMaster && stateLoaded)
        {
            pushWanted = true;
            pushAtMs = juce::jmax (pushAtMs, nowMs + kSettleMs);
        }
        else if (autoFetchAtMs < 0 && ! fetchPending)
        {
            org.kind = Origin::Kind::None;  // the Peak's sound; apply() finds its slot when the PC copy has it
            autoFetchAtMs = nowMs + kSettleMs;
        }
    }
    wasConnected = connected;
    if (pushWanted && nowMs >= pushAtMs)
    {
        if (! connected)
            push = Push::Failed;  // waits for the Peak; sent when it connects (above)
        else if (! fetchPending && ! verifyPending)
        {
            pushWanted = false;
            pushAll();
        }
    }

    // A patch picked on the synth: that slot is now the origin; read the new sound shortly.
    const auto pc = link.lastProgramChange();
    if (pc.seq != programSeen)
    {
        programSeen = pc.seq;
        if (pc.channel == host.midiChannel())
        {
            setSlot (pc.bank, pc.slot);
            autoFetchAtMs = nowMs + kFetchAfterProgramMs;
            outsideAtMs = -1;  // the program change's own read covers it
        }
    }
    if (autoFetchAtMs > 0 && nowMs >= autoFetchAtMs && ! fetchPending)
    {
        autoFetchAtMs = -1;
        fetch();
    }

    // The Peak's sound changed on the Peak without a program change (Init Patch, ...): read it once the editor has
    // been quiet for a moment, so the read can't undo an edit that is still on its way.
    const auto sc = link.soundChangedSeq();
    if (sc != soundSeen)
    {
        soundSeen = sc;
        if (autoFetchAtMs < 0)
            outsideAtMs = nowMs + kOutsideMs;
    }
    if (outsideAtMs > 0 && nowMs >= outsideAtMs)
    {
        if (autoFetchAtMs > 0 || pushWanted || ! connected)
            outsideAtMs = -1;  // a read or the project's push is coming anyway; or nobody to ask
        else if (fetchPending || verifyPending || checkPending || nowMs - link.lastParamSentMs() < kQuietAfterParamMs
                 || nowMs - patchSentMs < kQuietAfterPatchMs)
            outsideAtMs = nowMs + 200;
        else
        {
            outsideAtMs = -1;
            fetchPending = true;
            fetchKeepsReference = true;
            fetchAskedMs = nowMs;
            link.requestEditBuffer();
        }
    }

    watchTick (nowMs, connected);

    // Before a save: what is the Peak playing?
    if (checkDone && ! checkPending && ! fetchPending && ! verifyPending)
    {
        if (! connected)
        {
            auto done = std::move (checkDone);
            checkDone = nullptr;
            done (PeakCheck::NoAnswer, {});
        }
        else
        {
            checkPending = true;
            checkAskedMs = nowMs;
            link.requestEditBuffer();
        }
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
            push = Push::Idle;
            if (p.patch.data == sentPatch)
                status = juce::String ("Sent \"") + patchName (sentPatch) + "\" to the Peak and checked.";
            else
            {
                int differ = 0;
                for (size_t i = 0; i < sentPatch.size(); ++i)
                    differ += p.patch.data[i] != sentPatch[i];
                status = "Sent, but the Peak's sound differs in " + juce::String (differ) + " bytes.";
            }
        }
        else if (fetchPending)
        {
            fetchPending = false;
            if (std::exchange (watchAsk, false))
            {
                fetchKeepsReference = false;
                takeWatched (p.patch.data, nowMs);
            }
            else if (std::exchange (fetchKeepsReference, false))
                takePeakSound (p.patch.data);
            else
                apply (p.patch.data);
        }
        else if (checkPending)
        {
            checkPending = false;
            const auto current = host.currentValues();
            const bool same = differing (readValues (p.patch.data, current).values, current).empty();
            auto done = std::move (checkDone);
            checkDone = nullptr;
            if (done)
                done (same ? PeakCheck::Same : PeakCheck::Differs, p.patch.data);
        }
    }
    if (checkPending && nowMs - checkAskedMs > kFetchTimeoutMs)
    {
        checkPending = false;
        status = "The Peak did not answer when asked what it is playing. Is the port still open?";
        auto done = std::move (checkDone);
        checkDone = nullptr;
        if (done)
            done (PeakCheck::NoAnswer, {});
    }
    if (verifyPending && nowMs - verifyAskedMs > kFetchTimeoutMs)
    {
        verifyPending = false;
        push = Push::Failed;
        status = "Sent, but the Peak did not answer the check. Is the port still open?";
    }
    if (fetchPending && nowMs - fetchAskedMs > kFetchTimeoutMs)
    {
        fetchPending = false;
        fetchKeepsReference = false;
        if (std::exchange (watchAsk, false))
        {
            ++watchStats.timeouts;
            ++watchFailures;  // ask less often until the Peak answers again
        }
        else
            status = "No patch came back. Check the Setup tab.";
    }

    if (rewrite.due (nowMs) && link.canSend())
    {
        link.sendEditBuffer (writeValues (base, host.soundingValues()));
        patchSentMs = nowMs;
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
    t.setProperty (kPcOnly, baselinePcOnly, nullptr);
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
    baselinePcOnly = (bool) t.getProperty (kPcOnly, false);

    // A project brought its own patch: it goes to the Peak once the state has settled (hosts may restore twice).
    stateLoaded = true;
    if (editorIsMaster)
    {
        pushWanted = true;
        pushAtMs = now() + kSettleMs;
        autoFetchAtMs = -1;
    }
}
