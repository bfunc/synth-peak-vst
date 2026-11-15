#include "Timing.h"

#include <cmath>

namespace peak
{

void PatchRewrite::touch (double nowMs)
{
    dirty = true;
    lastTouch = nowMs;
}

bool PatchRewrite::due (double nowMs)
{
    if (! dirty || nowMs - lastTouch < quiet || nowMs - lastSent < gap)
        return false;
    dirty = false;
    lastSent = nowMs;
    return true;
}

FetchPlan::FetchPlan (std::vector<Item> list, double timeoutMs, int retries)
    : items (std::move (list)), states (items.size(), State::Waiting), tries (items.size(), 0),
      timeout (timeoutMs), maxTries (1 + retries)
{
}

std::optional<FetchPlan::Item> FetchPlan::next (double nowMs)
{
    if (cancelled)
        return std::nullopt;
    if (inFlight)
    {
        if (nowMs - sentAt < timeout)
            return std::nullopt;
        inFlight = false;  // timed out: retry or give up on this one
        if (tries[cursor] >= maxTries)
        {
            states[cursor] = State::Failed;
            ++cursor;
        }
    }
    while (cursor < items.size() && states[cursor] != State::Waiting)
        ++cursor;
    if (cursor >= items.size())
        return std::nullopt;
    ++tries[cursor];
    inFlight = true;
    sentAt = nowMs;
    return items[cursor];
}

void FetchPlan::gotReply (int bank, int slot)
{
    if (! inFlight || cursor >= items.size())
        return;
    if (items[cursor].bank != bank || items[cursor].slot != slot)
        return;  // a late reply to something else
    states[cursor] = State::Ok;
    inFlight = false;
    ++cursor;
}

void FetchPlan::cancel()
{
    cancelled = true;
    inFlight = false;
}

bool FetchPlan::done() const { return cancelled || finished() == total(); }

int FetchPlan::finished() const
{
    int n = 0;
    for (auto s : states)
        n += s != State::Waiting;
    return n;
}

int FetchPlan::failed() const
{
    int n = 0;
    for (auto s : states)
        n += s == State::Failed;
    return n;
}

void GateTracker::noteOn (int channel, int note)
{
    if (channel < 1 || channel > 16 || note < 0 || note > 127)
        return;
    ++ons;
    bool& d = down[channel - 1][note];
    if (! d)
    {
        d = true;
        ++count;
    }
}

void GateTracker::noteOff (int channel, int note)
{
    if (channel < 1 || channel > 16 || note < 0 || note > 127)
        return;
    bool& d = down[channel - 1][note];
    if (d)
    {
        d = false;
        --count;
    }
}

void GateTracker::allOff()
{
    for (auto& ch : down)
        for (auto& n : ch)
            n = false;
    count = 0;
}

double lfoBeatsPerCycle (int syncIndex)
{
    static const double beats[] = { 64, 48, 42, 36, 32, 30, 28, 24, 64.0 / 3, 20, 56.0 / 3, 18, 16, 40.0 / 3, 12,
                                    32.0 / 3, 8, 6, 16.0 / 3, 4, 3, 8.0 / 3, 2, 1.5, 4.0 / 3, 1, 0.75, 2.0 / 3, 0.5,
                                    0.375, 1.0 / 3, 0.25, 1.0 / 6, 0.125, 1.0 / 12 };
    constexpr int n = (int) (sizeof (beats) / sizeof (beats[0]));
    if (syncIndex < 0)
        syncIndex = 0;
    if (syncIndex >= n)
        syncIndex = n - 1;
    return beats[syncIndex];
}

double lfoSyncPhase (double ppq, int syncIndex)
{
    const double b = lfoBeatsPerCycle (syncIndex);
    double ph = std::fmod (ppq / b, 1.0);
    return ph < 0 ? ph + 1.0 : ph;
}

} // namespace peak
