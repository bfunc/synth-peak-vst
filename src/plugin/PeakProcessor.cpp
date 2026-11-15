#include "PeakProcessor.h"

#include "ui/PeakEditor.h"

using namespace peak;

namespace
{
const juce::Identifier kChannel { "midiChannel" };
const juce::Identifier kOutputMode { "outputMode" };  // "host" / "direct"
const juce::Identifier kInPort { "inputPort" };
const juce::Identifier kOutPort { "outputPort" };
const juce::Identifier kNotesThru { "notesThru" };
const juce::Identifier kPlayheads { "playheads" };
double now() { return juce::Time::getMillisecondCounterHiRes(); }
} // namespace

PeakProcessor::PeakProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PeakControl", ParamBridge::createLayout()),
      bridge (apvts),
      peakLink (PeakLink::acquire())
{
    patch = std::make_unique<PatchSession> (*peakLink, *this);
    lib = std::make_unique<LibraryService> (*peakLink, *patch);
    paramCursor = peakLink->paramSeq();

    // A new plugin (and the standalone app) talks to the synth's port itself; a saved state brings its own choice.
    setHostOutput (false);
    pump.startThread();
    startTimerHz (30);
}

PeakProcessor::~PeakProcessor()
{
    stopTimer();
    pump.stopThread (1000);
}

// ---------------------------------------------------------------------------------------------------------------
// Sending

template <typename EmitCc>
void PeakProcessor::runSync (EmitCc&& emitCc)
{
    if (primeRequested.exchange (false))
        for (size_t i = 0; i < bridge.size(); ++i)
            sync.markSynced (i, bridge.value (i));
    if (sendAllRequested.exchange (false))
        sync.markAllDirty();

    sync.collect (now(), [this] (size_t i) { return bridge.value (i); }, emitCc,
                  [this] (size_t, int) { ++patchOnlyChanges; });
}

void PeakProcessor::pumpDirect()
{
    if (peakLink->isHostOutput() || ! peakLink->isOutputOpen())
        return;

    // Notes first: they are time-critical.
    int s1, n1, s2, n2;
    const int ready = noteFifo.getNumReady();
    if (ready > 0)
    {
        noteFifo.prepareToRead (ready, s1, n1, s2, n2);
        auto put = [this] (int start, int n)
        {
            for (int k = 0; k < n; ++k)
            {
                const uint32_t m = noteBuf[(size_t) (start + k)];
                peakLink->sendShort ((uint8_t) (m & 0xFF), (uint8_t) ((m >> 8) & 0xFF), (uint8_t) ((m >> 16) & 0xFF));
            }
        };
        put (s1, n1);
        put (s2, n2);
        noteFifo.finishedRead (n1 + n2);
    }

    const juce::ScopedLock sl (syncLock);
    const int ch = channel.load();
    runSync ([&] (size_t i, int value, const Cc* ccs, int n) { peakLink->sendCcs (ch, ccs, n, (int) i, value); });
}

void PeakProcessor::handleNotes (const juce::MidiBuffer& in, juce::MidiBuffer& out, bool host)
{
    const bool thru = notesThru.load();
    const int ch = channel.load();
    for (const auto meta : in)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
            gate.noteOn (m.getChannel(), m.getNoteNumber());
        else if (m.isNoteOff())
            gate.noteOff (m.getChannel(), m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            gate.allOff();

        if (! thru)
            continue;
        // Notes and performance data go on, on the plugin's channel. Parameter CCs and SysEx don't (the panel owns them).
        const bool perf = m.isNoteOnOrOff() || m.isPitchWheel() || m.isChannelPressure() || m.isAftertouch()
                       || (m.isController() && (m.getControllerNumber() == 1 || m.getControllerNumber() == 64))
                       || m.isAllNotesOff();
        if (! perf)
            continue;
        auto msg = m;
        msg.setChannel (ch);
        if (host)
            out.addEvent (msg, meta.samplePosition);
        else
        {
            const auto* raw = msg.getRawData();
            const int size = msg.getRawDataSize();
            int s1, n1, s2, n2;
            noteFifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 + n2 == 1)
            {
                noteBuf[(size_t) (n1 ? s1 : s2)] =
                    (uint32_t) raw[0] | ((size > 1 ? (uint32_t) raw[1] : 0u) << 8) | ((size > 2 ? (uint32_t) raw[2] : 0u) << 16);
                noteFifo.finishedWrite (1);
            }
        }
    }
    uiFeed.notesHeld = gate.held();
    uiFeed.noteOns = gate.onCount();
}

void PeakProcessor::readTransport()
{
    bool has = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            has = true;
            uiFeed.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition())
                uiFeed.ppq = *ppq;
            if (auto bpm = pos->getBpm())
                uiFeed.bpm = *bpm;
            uiFeed.ppqAtMs = now();
        }
    uiFeed.hasHost = has;
}

void PeakProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    audio.clear();
    readTransport();

    const bool host = peakLink->isHostOutput();
    juce::MidiBuffer out;
    handleNotes (midi, out, host);
    midi.swapWith (out);  // incoming MIDI is replaced by what goes to the synth

    if (! host)
        return;

    peakLink->drainHostOutput (midi);
    const juce::ScopedTryLock sl (syncLock);
    if (! sl.isLocked())
        return;  // a mode switch or apply is in progress; the next block sends
    const int ch = channel.load();
    runSync ([&] (size_t i, int value, const Cc* ccs, int n)
    {
        for (int k = 0; k < n; ++k)
            midi.addEvent (juce::MidiMessage::controllerEvent (ch, ccs[k].controller, ccs[k].value), 0);
        peakLink->noteSentThroughHost ((int) i, value);
    });
}

// ---------------------------------------------------------------------------------------------------------------
// Message thread

void PeakProcessor::timerCallback()
{
    const double t = now();

    // Knobs turned on the synth: show them, don't send them back.
    std::vector<PeakLink::ParamIn> in;
    paramCursor = peakLink->readParams (paramCursor, in);
    if (! in.empty())
    {
        const int ch = channel.load();
        const juce::ScopedLock sl (syncLock);
        for (const auto& e : in)
            if (e.channel == ch && e.index >= 0)
            {
                sync.markSynced ((size_t) e.index, e.value);
                bridge.setQuietly ((size_t) e.index, e.value);
            }
    }

    const auto changes = patchOnlyChanges.load();
    if (changes != patchOnlySeen)
    {
        patchOnlySeen = changes;
        patch->patchOnlyChanged (t);
    }
    patch->tick (t);
    lib->tick (t);
}

void PeakProcessor::applyFromSynth (const Values& v)
{
    const juce::ScopedLock sl (syncLock);
    for (size_t i = 0; i < v.size() && i < bridge.size(); ++i)
    {
        sync.markSynced (i, v[i]);
        bridge.setQuietly (i, v[i]);
    }
}

void PeakProcessor::markAllSynced()
{
    const juce::ScopedLock sl (syncLock);
    for (size_t i = 0; i < bridge.size(); ++i)
        sync.markSynced (i, bridge.value (i));
}

void PeakProcessor::sendAllParameters() { sendAllRequested = true; }

PeakProcessor::Settings PeakProcessor::getSettings() const
{
    Settings s;
    s.channel = channel.load();
    s.hostOutput = peakLink->isHostOutput();
    s.inPort = peakLink->getInputChoice();
    s.outPort = peakLink->getOutputChoice();
    s.notesThru = notesThru.load();
    s.playheads = playheads.load();
    return s;
}

void PeakProcessor::setChannel (int ch)
{
    channel = juce::jlimit (1, 16, ch);
    apvts.state.setProperty (kChannel, channel.load(), nullptr);
}

void PeakProcessor::setHostOutput (bool h)
{
    if (juce::JUCEApplicationBase::isStandaloneApp())
        h = false;  // there is no host output to send through
    {
        const juce::ScopedLock sl (syncLock);
        peakLink->setHostOutput (h);
    }
    apvts.state.setProperty (kOutputMode, h ? "host" : "direct", nullptr);
}

void PeakProcessor::setPorts (const juce::String& in, const juce::String& out)
{
    peakLink->setPortChoice (in, out);
    apvts.state.setProperty (kInPort, in, nullptr);
    apvts.state.setProperty (kOutPort, out, nullptr);
}

void PeakProcessor::setNotesThru (bool on)
{
    notesThru = on;
    apvts.state.setProperty (kNotesThru, on, nullptr);
}

void PeakProcessor::setPlayheads (bool on)
{
    playheads = on;
    apvts.state.setProperty (kPlayheads, on, nullptr);
}

void PeakProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("session"), nullptr);
    state.appendChild (patch->toState(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PeakProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    patch->fromState (state.getChildWithName ("session"));
    apvts.replaceState (state);

    channel = juce::jlimit (1, 16, (int) apvts.state.getProperty (kChannel, 1));
    if (apvts.state.hasProperty (kOutputMode))
        setHostOutput (apvts.state.getProperty (kOutputMode).toString() == "host");
    peakLink->setPortChoice (apvts.state.getProperty (kInPort).toString(), apvts.state.getProperty (kOutPort).toString());
    notesThru = (bool) apvts.state.getProperty (kNotesThru, true);
    playheads = (bool) apvts.state.getProperty (kPlayheads, false);
    primeRequested = true;  // a loaded project does not overwrite the synth; Send does that
}

juce::AudioProcessorEditor* PeakProcessor::createEditor() { return new PeakEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PeakProcessor(); }
