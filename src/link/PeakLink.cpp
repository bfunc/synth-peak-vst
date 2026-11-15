#include "PeakLink.h"

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace
{
constexpr int kTickMs = 50;
constexpr int kPortCheckTicks = 30;  // look for the ports every 1.5 s
constexpr double kEchoWindowMs = 300;
constexpr size_t kPatchesKept = 160;

std::atomic<int> portsClosing { 0 };

bool waitForCloses (int ms)
{
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds (ms);
    while (portsClosing.load() > 0 && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    return portsClosing.load() == 0;
}

std::mutex instanceMutex;
std::weak_ptr<PeakLink> instance;

struct Pick
{
    juce::MidiDeviceInfo device;
    bool savedMissing = false;
};

Pick pick (const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& choice)
{
    std::vector<std::string> names;
    for (const auto& d : devices)
        names.push_back (d.name.toStdString());
    const auto p = peak::pickPort (names, choice.toStdString());
    Pick r;
    if (p.index >= 0)
        r.device = devices[p.index];
    r.savedMissing = p.savedMissing;
    return r;
}

juce::StringArray names (const juce::Array<juce::MidiDeviceInfo>& devices)
{
    juce::StringArray n;
    for (const auto& d : devices)
        n.add (d.name);
    return n;
}
} // namespace

// Input callbacks go through this, so a port that closes late (or never) can't call into a deleted PeakLink.
class PeakLink::InputForwarder : public juce::MidiInputCallback
{
public:
    explicit InputForwarder (PeakLink& l) : link (&l) {}
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& m) override
    {
        const juce::ScopedLock sl (lock);
        if (link != nullptr)
            link->handleIncomingMidiMessage (source, m);
    }
    void detach()
    {
        const juce::ScopedLock sl (lock);
        link = nullptr;
    }

private:
    juce::CriticalSection lock;
    PeakLink* link;
};

std::shared_ptr<PeakLink> PeakLink::acquire()
{
    const std::lock_guard<std::mutex> lock (instanceMutex);
    auto link = instance.lock();
    if (link == nullptr)
    {
        link = std::shared_ptr<PeakLink> (new PeakLink());
        instance = link;
    }
    return link;
}

PeakLink::PeakLink() : forwarder (std::make_shared<InputForwarder> (*this)) { startTimer (kTickMs); }

PeakLink::~PeakLink()
{
    stopTimer();
    forwarder->detach();
    std::unique_ptr<juce::MidiOutput> out;
    {
        const juce::ScopedLock sl (outLock);
        out = std::move (output);
    }
    closeLater (std::move (input), std::move (out));
    // A port whose driver hung would keep the standalone app alive with no window.
    if (! waitForCloses (2000) && juce::JUCEApplicationBase::isStandaloneApp())
        std::_Exit (0);
}

void PeakLink::closeLater (std::unique_ptr<juce::MidiInput> in, std::unique_ptr<juce::MidiOutput> out)
{
    if (in == nullptr && out == nullptr)
        return;
    ++portsClosing;
    std::thread ([in = std::move (in), out = std::move (out), keep = forwarder]() mutable
    {
        if (in != nullptr)
            in->stop();
        in.reset();
        out.reset();
        keep.reset();
        --portsClosing;
    }).detach();
}

juce::StringArray PeakLink::inputNames() { return names (juce::MidiInput::getAvailableDevices()); }
juce::StringArray PeakLink::outputNames() { return names (juce::MidiOutput::getAvailableDevices()); }

void PeakLink::setPortChoice (const juce::String& inputName, const juce::String& outputName)
{
    {
        const juce::ScopedLock sl (stateLock);
        inChoice = inputName;
        outChoice = outputName;
    }
    ticks = 0;
}

juce::String PeakLink::getInputChoice() const
{
    const juce::ScopedLock sl (stateLock);
    return inChoice;
}

juce::String PeakLink::getOutputChoice() const
{
    const juce::ScopedLock sl (stateLock);
    return outChoice;
}

void PeakLink::setHostOutput (bool host)
{
    hostOutput = host;
    ticks = 0;
}

void PeakLink::updatePorts()
{
    juce::String wantIn, wantOut;
    {
        const juce::ScopedLock sl (stateLock);
        wantIn = inChoice;
        wantOut = outChoice;
    }
    const auto inPick = pick (juce::MidiInput::getAvailableDevices(), wantIn);
    const auto outPick = hostOutput ? Pick() : pick (juce::MidiOutput::getAvailableDevices(), wantOut);
    bool opened = false;

    if (input != nullptr && inPick.device.identifier != inputId)
    {
        closeLater (std::move (input), nullptr);
        inputId = {};
    }
    if (input == nullptr && inPick.device.identifier.isNotEmpty())
    {
        input = juce::MidiInput::openDevice (inPick.device.identifier, forwarder.get());
        if (input != nullptr)
        {
            input->start();
            inputId = inPick.device.identifier;
            for (auto& d : decoders)
                d.reset();
            opened = true;
        }
    }

    {
        const juce::ScopedLock sl (outLock);
        if (output != nullptr && outPick.device.identifier != outputId)
        {
            closeLater (nullptr, std::move (output));
            outputId = {};
        }
        if (output == nullptr && outPick.device.identifier.isNotEmpty())
        {
            output = juce::MidiOutput::openDevice (outPick.device.identifier);
            if (output != nullptr)
            {
                outputId = outPick.device.identifier;
                opened = true;
            }
        }
        outputOpen = output != nullptr;
    }

    const juce::ScopedLock sl (stateLock);
    if (opened)
    {
        // A new connection: what was seen before says nothing about it.
        st = peak::LinkStatus();
        probeAfterOpen = true;
    }
    st.inName = inPick.device.name.toStdString();
    st.inOpen = input != nullptr;
    st.inBusy = inPick.device.identifier.isNotEmpty() && input == nullptr;
    st.inSavedMissing = inPick.savedMissing;
    st.outName = outPick.device.name.toStdString();
    st.outOpen = outputOpen.load();
    st.outBusy = outPick.device.identifier.isNotEmpty() && ! outputOpen.load();
    st.outSavedMissing = outPick.savedMissing;
}

void PeakLink::timerCallback()
{
    if (ticks++ % kPortCheckTicks == 0)
        updatePorts();
    if (probeAfterOpen && canSend() && input != nullptr)
    {
        probeAfterOpen = false;
        probe();
    }
}

void PeakLink::send (const juce::MidiMessage& m)
{
    if (hostOutput)
    {
        const juce::SpinLock::ScopedLockType sl (hostLock);
        hostPending.push_back (m);
        return;
    }
    const juce::ScopedLock sl (outLock);
    if (output != nullptr)
        output->sendMessageNow (m);
}

void PeakLink::sendShort (uint8_t status, uint8_t d1, uint8_t d2) { send (juce::MidiMessage (status, d1, d2)); }

void PeakLink::sendCcs (int channel, const peak::Cc* ccs, int count, int paramIndex, int value)
{
    {
        const juce::ScopedLock sl (outLock);
        if (output == nullptr)
            return;
        for (int i = 0; i < count; ++i)
            output->sendMessageNow (juce::MidiMessage::controllerEvent (channel, ccs[i].controller, ccs[i].value));
    }
    rememberSent (paramIndex, value, false);
}

void PeakLink::sendSysex (const std::vector<uint8_t>& message)
{
    if (message.size() < 2)
        return;
    send (juce::MidiMessage::createSysExMessage (message.data() + 1, (int) message.size() - 2));  // JUCE adds F0/F7
}

void PeakLink::noteSentThroughHost (int paramIndex, int value) { rememberSent (paramIndex, value, true); }

void PeakLink::rememberSent (int paramIndex, int value, bool fromAudioThread)
{
    const double t = nowMs();
    auto put = [&]
    {
        sent[sentNext] = { paramIndex, value, t };
        sentNext = (sentNext + 1) % sent.size();
    };
    if (fromAudioThread)
    {
        const juce::SpinLock::ScopedTryLockType sl (sentLock);
        if (sl.isLocked())
            put();
    }
    else
    {
        const juce::SpinLock::ScopedLockType sl (sentLock);
        put();
    }
}

void PeakLink::drainHostOutput (juce::MidiBuffer& midi)
{
    const juce::SpinLock::ScopedTryLockType sl (hostLock);
    if (! sl.isLocked())
        return;
    for (const auto& m : hostPending)
        midi.addEvent (m, 0);
    hostPending.clear();
}

void PeakLink::probe()
{
    {
        const juce::ScopedLock sl (stateLock);
        st.inquirySentMs = nowMs();
    }
    sendSysex (peak::requestDeviceInquiry());
}

void PeakLink::requestEditBuffer() { sendSysex (peak::requestEditBuffer()); }
void PeakLink::requestPatch (int bank, int slot) { sendSysex (peak::requestPatch (bank, slot)); }
void PeakLink::sendEditBuffer (const peak::PatchData& d) { sendSysex (peak::editBufferDump (d)); }

void PeakLink::writeSlot (int bank, int slot, const peak::PatchData& d)
{
    if (bank < 1 || bank > 4 || slot < 0 || slot > 127)
        return;
    sendSysex (peak::writeSlotDump (bank, slot, d));
}

void PeakLink::sendProgramChange (int channel, int bank, int slot)
{
    for (const auto& m : peak::programChange (channel, bank, slot))
        sendShort (m.status, m.d1, m.d2);
}

void PeakLink::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    const double t = nowMs();

    if (m.isSysEx())
    {
        // Rebuild the full message (JUCE strips F0 / F7).
        std::vector<uint8_t> full;
        full.reserve ((size_t) m.getSysExDataSize() + 2);
        full.push_back (0xF0);
        full.insert (full.end(), m.getSysExData(), m.getSysExData() + m.getSysExDataSize());
        full.push_back (0xF7);

        const juce::ScopedLock sl (stateLock);
        st.lastAnyInMs = t;
        if (auto info = peak::parseDeviceInquiryReply (full.data(), full.size()))
        {
            st.inquiryReplyMs = t;
            st.isPeak = info->isPeak;
            st.firmware = info->firmware;
        }
        else if (auto patch = peak::parsePatchMessage (full.data(), full.size()))
        {
            st.lastPatchMs = t;
            patchesIn.push_back ({ ++patchInSeq, *patch });
            while (patchesIn.size() > kPatchesKept)
                patchesIn.pop_front();
        }
        return;
    }

    if (m.isActiveSense() || m.isMidiClock())
        return;

    const int ch = m.getChannel();
    if (ch < 1 || ch > 16)
        return;

    if (m.isProgramChange())
    {
        const juce::ScopedLock sl (stateLock);
        st.lastAnyInMs = t;
        programIn = { programIn.seq + 1, ch, bankIn[(size_t) (ch - 1)] + 1, m.getProgramChangeNumber() };
        return;
    }
    if (! m.isController())
    {
        const juce::ScopedLock sl (stateLock);
        st.lastAnyInMs = t;
        return;
    }

    const int cc = m.getControllerNumber();
    if (cc == 32)
        bankIn[(size_t) (ch - 1)] = m.getControllerValue();
    const auto change = decoders[(size_t) (ch - 1)].feed ((uint8_t) cc, (uint8_t) m.getControllerValue());

    bool echo = false;
    if (change)
    {
        const juce::SpinLock::ScopedLockType sl (sentLock);
        for (const auto& s : sent)
            if (s.index == change->index && s.value == change->value && t - s.ms < kEchoWindowMs)
                echo = true;
    }

    const juce::ScopedLock sl (stateLock);
    st.lastAnyInMs = t;
    if (! change)
        return;
    if (echo)
    {
        ++st.echoCount;
        return;
    }
    ++st.ccInCount;
    st.lastCcChannel = ch;
    ++paramInSeq;
    paramsIn[paramInSeq % paramsIn.size()] = { paramInSeq, ch, change->index, change->value };
}

uint32_t PeakLink::readParams (uint32_t afterSeq, std::vector<ParamIn>& out) const
{
    const juce::ScopedLock sl (stateLock);
    const uint32_t from = paramInSeq - afterSeq > paramsIn.size() ? paramInSeq - (uint32_t) paramsIn.size() : afterSeq;
    for (uint32_t s = from + 1; s <= paramInSeq; ++s)
        out.push_back (paramsIn[s % paramsIn.size()]);
    return paramInSeq;
}

uint32_t PeakLink::paramSeq() const
{
    const juce::ScopedLock sl (stateLock);
    return paramInSeq;
}

uint32_t PeakLink::readPatches (uint32_t afterSeq, std::vector<PatchIn>& out) const
{
    const juce::ScopedLock sl (stateLock);
    for (const auto& p : patchesIn)
        if (p.seq > afterSeq)
            out.push_back (p);
    return patchInSeq;
}

uint32_t PeakLink::patchSeq() const
{
    const juce::ScopedLock sl (stateLock);
    return patchInSeq;
}

PeakLink::ProgramIn PeakLink::lastProgramChange() const
{
    const juce::ScopedLock sl (stateLock);
    return programIn;
}

peak::LinkStatus PeakLink::status (int pluginChannel) const
{
    peak::LinkStatus s;
    {
        const juce::ScopedLock sl (stateLock);
        s = st;
    }
    s.nowMs = nowMs();
    s.pluginChannel = pluginChannel;
    s.hostOutput = hostOutput;
    {
        const std::lock_guard<std::mutex> lock (instanceMutex);
        s.sharedBy = juce::jmax (1, (int) instance.use_count());
    }
    return s;
}
