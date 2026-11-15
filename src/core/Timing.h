// Small time-driven helpers, all with an injected clock (ms) so they test without waiting.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace peak
{

// Coalesces changes to parameters that can only reach the synth by rewriting the whole patch into the edit buffer.
// A rewrite goes out once changes have been quiet for `quietMs`, and never more often than every `minGapMs`
// (a whole-patch write may cut notes and restart LFOs, so a turning knob must not cause a stream of them).
class PatchRewrite
{
public:
    explicit PatchRewrite (double quietMs = 80.0, double minGapMs = 300.0) : quiet (quietMs), gap (minGapMs) {}

    void touch (double nowMs);     // a patch-only parameter changed
    bool due (double nowMs);       // true once per batch of changes, when it is time to send; clears the flag
    bool pending() const { return dirty; }
    void cancel() { dirty = false; }

private:
    double quiet, gap;
    bool dirty = false;
    double lastTouch = -1.0e12, lastSent = -1.0e12;
};

// Fetching many patches one by one: one request out at a time, the next when the reply came (or after a timeout,
// with one retry). The caller sends the requests and reports the replies.
class FetchPlan
{
public:
    struct Item
    {
        int bank = 0, slot = 0;
    };
    enum class State { Waiting, Ok, Failed };

    explicit FetchPlan (std::vector<Item> items, double timeoutMs = 1500.0, int retries = 1);

    // The request to send now, if any (call often). After it returns an item, wait for gotReply().
    std::optional<Item> next (double nowMs);
    void gotReply (int bank, int slot);
    void cancel();

    bool done() const;
    int total() const { return (int) items.size(); }
    int finished() const;   // Ok + Failed
    int failed() const;
    State state (size_t i) const { return states[i]; }
    const Item& item (size_t i) const { return items[i]; }

private:
    std::vector<Item> items;
    std::vector<State> states;
    std::vector<int> tries;
    double timeout;
    int maxTries;
    size_t cursor = 0;
    bool inFlight = false, cancelled = false;
    double sentAt = 0;
};

// Notes held on the way to the synth, for the envelope gate light.
class GateTracker
{
public:
    void noteOn (int channel, int note);
    void noteOff (int channel, int note);
    void allOff();
    int held() const { return count; }
    uint32_t onCount() const { return ons; }  // grows with every note-on (the light flashes when it changes)

private:
    bool down[16][128] {};
    int count = 0;
    uint32_t ons = 0;
};

// LFO Sync playhead: beats per LFO cycle for each Sync Rate value (0..34, "64 beats" .. "32nd T"),
// and the cycle phase 0..1 at a host position in quarter notes. Approximate by nature (see RESEARCH.md).
double lfoBeatsPerCycle (int syncIndex);
double lfoSyncPhase (double ppq, int syncIndex);

} // namespace peak
