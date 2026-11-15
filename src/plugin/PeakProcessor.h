// The plugin: holds the Peak's patch parameters (from peak::paramTable()) and keeps the synth in step with them.
// It makes no sound.
//
// Pieces, each in its own file:
//   ParamBridge     table <-> JUCE parameters
//   SyncEngine      which changes go out live (CC / NRPN) and which need a whole-patch rewrite   (core)
//   PatchSession    the current patch: raw bytes, origin, Fetch / Send / program changes
//   LibraryService  banks, cache, files, saving into a slot
//   PeakLink        the shared MIDI ports
//   UiFeed          transport and notes for the UI (lock-free)
//
// Live sends go out either through the host's MIDI output (host mode; audio thread) or straight to the synth's port
// (direct mode; a pump thread). Notes coming into the plugin are passed on to the synth the same way.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "core/Sync.h"
#include "core/Timing.h"
#include "link/PeakLink.h"
#include "plugin/LibraryService.h"
#include "plugin/ParamBridge.h"
#include "plugin/PatchSession.h"
#include "plugin/UiFeed.h"

#include <atomic>
#include <memory>

class PeakProcessor : public juce::AudioProcessor, public SessionHost, private juce::Timer
{
public:
    PeakProcessor();
    ~PeakProcessor() override;

    // ---- juce::AudioProcessor ----
    void prepareToPlay (double, int) override { outBuf.ensureSize (8192); }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- SessionHost ----
    peak::Values currentValues() const override { return bridge.values(); }
    peak::Values soundingValues() const override;
    void applyFromSynth (const peak::Values&) override;
    void markAllSynced() override;
    int midiChannel() const override { return channel.load(); }

    // ---- for the editor (message thread) ----
    juce::AudioProcessorValueTreeState apvts;
    ParamBridge& params() { return bridge; }
    PatchSession& session() { return *patch; }
    LibraryService& library() { return *lib; }
    PeakLink& link() { return *peakLink; }
    const UiFeed& feed() const { return uiFeed; }

    struct Settings
    {
        int channel = 1;
        bool hostOutput = false;  // standalone: always direct
        juce::String inPort, outPort;
        bool notesThru = true;    // pass incoming notes / bend / pressure on to the synth
        bool playheads = false;   // LFO sync playhead + envelope gate light (off by default)
        bool followChannel = true;  // take the channel the Peak sends on
    };
    Settings getSettings() const;
    void setChannel (int);
    void setHostOutput (bool);
    void setPorts (const juce::String& in, const juce::String& out);
    void setNotesThru (bool);
    void setPlayheads (bool);
    void setFollowChannel (bool);
    void sendAllParameters();  // every live parameter again (patch-only ones via a rewrite)

    // Compare: while held, the synth plays the saved patch (only the differing parameters are sent); the editor keeps
    // showing the edit. Message thread. begin does nothing when there is nothing saved to compare with.
    void beginCompare();
    void endCompare();
    bool comparing() const { return previewing.load(); }

private:
    void timerCallback() override;
    void pumpDirect();
    template <typename EmitCc>
    void runSync (EmitCc&&);  // caller holds syncLock
    void handleNotes (const juce::MidiBuffer& in, juce::MidiBuffer& out, bool host);
    void readTransport();

    struct Pump : juce::Thread
    {
        explicit Pump (PeakProcessor& p) : juce::Thread ("Peak out"), proc (p) {}
        void run() override
        {
            while (! threadShouldExit())
            {
                proc.pumpDirect();
                wait (2);
            }
        }
        PeakProcessor& proc;
    };

    ParamBridge bridge;
    std::shared_ptr<PeakLink> peakLink;  // shared by every plugin copy in this program
    std::unique_ptr<PatchSession> patch;
    std::unique_ptr<LibraryService> lib;
    UiFeed uiFeed;

    peak::SyncEngine sync;
    juce::CriticalSection syncLock;          // the audio thread only try-locks it
    std::atomic<bool> primeRequested { true };   // take current values as "already on the synth"
    std::atomic<bool> sendAllRequested { false };
    std::atomic<uint32_t> patchOnlyChanges { 0 };  // bumped when a patch-only parameter is due
    uint32_t patchOnlySeen = 0;                     // message thread

    int syncValue (size_t i) const;  // what goes to the synth: the edit, or the saved value while comparing
    std::atomic<bool> previewing { false };
    peak::Values previewValues;          // written only while previewing is false

    std::atomic<int> channel { 1 };
    std::atomic<bool> notesThru { true };
    std::atomic<bool> playheads { false };
    std::atomic<bool> followChannel { true };
    peak::GateTracker gate;                  // audio thread

    // Notes to pass on in direct mode: audio thread -> pump thread.
    juce::AbstractFifo noteFifo { 512 };
    std::array<uint32_t, 512> noteBuf {};    // packed status | d1 << 8 | d2 << 16

    uint32_t paramCursor = 0;                // message thread: last PeakLink param seq applied
    juce::MidiBuffer outBuf;                 // audio thread: what goes to the host output (allocated once)

    Pump pump { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeakProcessor)
};
