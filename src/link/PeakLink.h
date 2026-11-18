// The plugin's own MIDI connection to the Peak: opens its USB ports (or ports picked by name), reopens them when the
// synth is switched on or plugged in, sends CC / SysEx / program changes, and keeps what comes back for the rest of
// the plugin to read.
//
// One per program: Windows gives a MIDI port to one client at a time, so every plugin copy in the host shares this
// object (acquire()). Ports and output mode are shared too.
//
// Threads: the port timer runs on the message thread, MIDI input arrives on the MIDI thread, live CCs are sent from
// the processor's pump thread (direct mode) or put into the host's buffer by the audio thread (host mode). The audio
// thread only ever try-locks. Closing a port runs on its own thread: a Windows driver can hang when the synth was
// switched off with its port open.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "core/Link.h"
#include "core/Midi.h"
#include "core/Sysex.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

class PeakLink : private juce::MidiInputCallback, private juce::Timer
{
    class InputForwarder;

public:
    static std::shared_ptr<PeakLink> acquire();  // message thread
    ~PeakLink() override;

    // ---- ports ----
    void setPortChoice (const juce::String& inputName, const juce::String& outputName);  // "" = automatic
    juce::String getInputChoice() const;
    juce::String getOutputChoice() const;
    static juce::StringArray inputNames();
    static juce::StringArray outputNames();

    // true: everything goes out through the host's MIDI out (drainHostOutput); no output port is opened.
    void setHostOutput (bool);
    bool isHostOutput() const { return hostOutput.load(); }
    bool isOutputOpen() const { return outputOpen.load(); }
    bool canSend() const { return hostOutput.load() || outputOpen.load(); }

    // ---- sending ----
    void sendShort (uint8_t status, uint8_t d1, uint8_t d2);  // direct or through the host (any thread but audio)
    void sendCcs (int channel, const peak::Cc* ccs, int count, int paramIndex, int value);  // direct mode, pump thread
    void sendSysex (const std::vector<uint8_t>& message);     // F0 ... F7
    void noteSentThroughHost (int paramIndex, int value);     // audio thread: for loop detection
    void drainHostOutput (juce::MidiBuffer&);                 // audio thread, host mode; never blocks

    void probe();                                       // identity request (also sent after a port opens)
    void requestEditBuffer();
    void requestPatch (int bank, int slot);
    void sendEditBuffer (const peak::PatchData&);       // changes the current sound, not the memory
    void writeSlot (int bank, int slot, const peak::PatchData&);  // writes memory: only the save sequence calls it
    void sendProgramChange (int channel, int bank, int slot);

    // ---- receiving (every plugin copy reads with its own cursor: pass the last seq seen) ----
    struct ParamIn
    {
        uint32_t seq = 0;
        int channel = 0, index = -1, value = 0;
    };
    uint32_t readParams (uint32_t afterSeq, std::vector<ParamIn>& out) const;
    uint32_t paramSeq() const;

    struct PatchIn
    {
        uint32_t seq = 0;
        peak::PatchMessage patch;
    };
    uint32_t readPatches (uint32_t afterSeq, std::vector<PatchIn>& out) const;
    uint32_t patchSeq() const;

    struct ProgramIn
    {
        uint32_t seq = 0;
        int channel = 0, bank = 0, slot = 0;
    };
    ProgramIn lastProgramChange() const;

    // ---- connection check ----
    void playTestNote (int channel);  // C3 for half a second; the user then answers
    void answerTestNote (bool heard);

    // Every message from the Peak, for the Setup page's monitor (newest last).
    struct MonitorLine
    {
        uint32_t seq = 0;
        juce::int64 timeMs = 0;  // wall clock
        int channel = 0;         // 0 = no channel (SysEx)
        juce::String what, name;
        bool out = false;        // sent by the editor (else from the Peak)
    };
    uint32_t readMonitor (uint32_t afterSeq, std::vector<MonitorLine>& out) const;

    peak::LinkStatus status (int pluginChannel, bool followChannel = true) const;

    // ---- plugin copies (message thread) ----
    // Only one copy talks to the Peak: the first one; when it goes, the next takes over. The others show a lock page.
    void addClient (const void* id);
    void removeClient (const void* id);
    bool isMain (const void* id) const { return mainClient.load() == id; }  // any thread
    void setClientTrack (const void* id, const juce::String& trackName);
    juce::String mainTrack() const;                                          // "" when the host did not tell

private:
    void log (int channel, const juce::String& what, const juce::String& name, bool out = false);  // caller holds stateLock
    void logOut (int channel, const juce::String& what, const juce::String& name);  // takes stateLock
    void sendSysexLogged (const std::vector<uint8_t>& message, const juce::String& name);
    PeakLink();
    void timerCallback() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void updatePorts();
    void send (const juce::MidiMessage&);
    void rememberSent (int paramIndex, int value, bool fromAudioThread);
    void closeLater (std::unique_ptr<juce::MidiInput>, std::unique_ptr<juce::MidiOutput>);
    static double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    mutable juce::CriticalSection stateLock;  // guards everything marked (S)
    peak::LinkStatus st;                      // (S)
    juce::String inChoice, outChoice;         // (S)

    juce::CriticalSection outLock;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String outputId;
    std::unique_ptr<juce::MidiInput> input;  // message thread
    juce::String inputId;
    std::shared_ptr<InputForwarder> forwarder;

    std::atomic<bool> hostOutput { false };
    std::atomic<bool> outputOpen { false };

    juce::SpinLock hostLock;
    std::vector<juce::MidiMessage> hostPending;

    struct Sent
    {
        int index = -1, value = 0;
        double ms = -1;
    };
    juce::SpinLock sentLock;
    std::array<Sent, 64> sent;
    size_t sentNext = 0;

    std::array<peak::CcDecoder, 16> decoders;  // MIDI thread only
    std::array<int, 16> bankIn {};             // MIDI thread only: last Bank Select LSB (CC 32 = 1..4) per channel
    // MIDI thread only: the NRPN being set per channel, value + 1 (0 = none yet)
    std::array<int, 16> nrpnMsbIn {}, nrpnLsbIn {};

    std::array<ParamIn, 512> paramsIn;  // ring (S)
    uint32_t paramInSeq = 0;            // (S)
    std::deque<PatchIn> patchesIn;      // (S), the last 160
    uint32_t patchInSeq = 0;            // (S)
    ProgramIn programIn;                // (S)
    std::array<MonitorLine, 256> monitor;  // ring (S)
    uint32_t monitorSeq = 0;              // (S)

    std::vector<std::pair<const void*, juce::String>> clients;  // (S), in the order they came
    std::atomic<const void*> mainClient { nullptr };

    double testNoteOffMs = -1;  // message thread
    int testNoteChannel = 1;

    int ticks = 0;
    bool probeAfterOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeakLink)
};
