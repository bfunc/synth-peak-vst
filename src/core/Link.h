// Plain parts of the MIDI connection: picking the Peak's ports by name, program change messages, and the
// connection checklist the Setup page shows. The JUCE side (link/PeakLink) fills LinkStatus.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace peak
{

struct PortPick
{
    int index = -1;            // into the names; -1 = none
    bool savedMissing = false; // a port was chosen by name but is not there now (fell back to automatic)
};

// choice "" = automatic: the first name containing "peak" (any case). A chosen name must match exactly.
PortPick pickPort (const std::vector<std::string>& names, const std::string& choice);

struct ShortMsg
{
    uint8_t status, d1, d2;
};

// Load a stored patch: Bank Select (CC 0 = 0, CC 32 = bank - 1) then Program Change. bank 1..4 (A-D), slot 0..127.
// Provisional: the Peak's bank numbering is to be confirmed on the synth.
std::vector<ShortMsg> programChange (int channel, int bank, int slot);

// What the connection has seen; filled by the link, read by the checklist.
struct LinkStatus
{
    double nowMs = 0;
    bool hostOutput = false;          // sending through the DAW's MIDI out instead of the port
    std::string inName, outName;      // ports in use ("" = none found)
    bool inOpen = false, outOpen = false;
    bool inBusy = false, outBusy = false;   // found but could not be opened (another program has it)
    bool inSavedMissing = false, outSavedMissing = false;
    double inquirySentMs = -1, inquiryReplyMs = -1;
    bool isPeak = false;
    std::string firmware;
    double lastAnyInMs = -1;
    double lastPatchMs = -1;          // last patch message received
    int ccInCount = 0;                // parameter CCs / NRPNs received from the synth
    int lastCcChannel = 0;            // channel of the last one
    int pluginChannel = 1;
    int echoCount = 0;                // our own messages that came straight back (a MIDI loop)
    int sharedBy = 1;                 // plugin copies sharing the port
};

struct CheckLine
{
    enum class Level { Ok, Warn, Bad, Info } level;
    std::string text;
};

// The connection checklist, in the order a user would fix things.
std::vector<CheckLine> checklist (const LinkStatus& s);

} // namespace peak
