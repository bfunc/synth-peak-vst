// Plain parts of the MIDI connection: picking the Peak's ports by name, program change messages, and the
// connection diagnosis the Setup page shows. The JUCE side (link/PeakLink) fills LinkStatus.
#pragma once

#include "ParamTable.h"

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

// Load a stored patch: Bank Select LSB (CC 32 = bank, 1..4 = A-D) then Program Change (slot 0..127).
// This is what the Peak itself sends when a patch is picked on its panel (confirmed 2026-11-15; no CC 0).
std::vector<ShortMsg> programChange (int channel, int bank, int slot);

// The test note: the user plays it and says whether the Peak sounded.
enum class NoteTest { NotRun, Waiting, Heard, NotHeard };

// What the connection has seen; filled by the link, read by diagnose().
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
    bool followChannel = true;        // the plugin takes the channel the Peak sends on
    int echoCount = 0;                // our own messages that came straight back (a MIDI loop)
    int sharedBy = 1;                 // plugin copies sharing the port
    NoteTest noteTest = NoteTest::NotRun;
    int testNoteChannel = 1;          // the channel the test note went out on
};

constexpr double kInquiryTimeoutMs = 1500;

enum class Health { Ok, Warn, Bad, Unknown };

struct CheckItem
{
    Health health = Health::Unknown;
    std::string title;
    std::string detail;
    std::string fix;        // what to do, when not Ok
    std::string headline;   // the verdict's title when this item is the one to fix
    bool optional = false;  // doesn't count for "connected"
    bool blocked = false;   // can't be judged until an earlier step works
};

// What the verdict card offers to do.
enum class Action { CheckAgain, PlayTestNote, UseChannel, AnswerTestNote };

struct Report
{
    std::vector<CheckItem> steps;  // in order: each depends on the ones before
    std::vector<CheckItem> notes;  // don't stop the connection, but explain odd behaviour
    bool connected = false;        // every required step is Ok (the channel may still be unknown)
    Health verdictHealth = Health::Unknown;
    std::string verdict, verdictText;
    std::vector<Action> actions;
    int useChannel = 0;            // for Action::UseChannel
};

// The connection diagnosis: steps in the order a user would fix things, and the one thing to do now.
Report diagnose (const LinkStatus&);

// How a parameter travels as MIDI, for the monitor: "CC 79", "CC 47+49", "NRPN 0:7", "patch only".
std::string paramAddress (const ParamDef&);

} // namespace peak
