// What the audio thread tells the UI, lock-free: host transport and notes on their way to the synth.
// Written by processBlock, read by the editor's timer. Only atomics, so neither side ever waits.
#pragma once

#include <atomic>
#include <cstdint>

struct UiFeed
{
    std::atomic<bool> hasHost { false };    // a host position came in during the last blocks
    std::atomic<bool> playing { false };
    std::atomic<double> ppq { 0.0 };        // quarter notes at the start of the last block
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> ppqAtMs { 0.0 };    // when ppq was taken (ms), to move the playhead between blocks
    std::atomic<int> notesHeld { 0 };
    std::atomic<uint32_t> noteOns { 0 };

    // ppq now, estimated from the last block and the tempo.
    double ppqNow (double nowMs) const
    {
        if (! playing.load())
            return ppq.load();
        return ppq.load() + (nowMs - ppqAtMs.load()) / 60000.0 * bpm.load();
    }
};
