#include "Sync.h"

namespace peak
{

// 300 messages/s at most: a 3-CC NRPN is 9 bytes, so 2700 B/s, under what even a DIN link carries (3125 B/s).
SyncEngine::SyncEngine() : queue (paramTable().size(), SendQueue::Config { 20.0, 300.0, 24.0 }) {}

void SyncEngine::markAllSynced (const int* values, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        queue.markSynced (i, values[i]);
}

} // namespace peak
