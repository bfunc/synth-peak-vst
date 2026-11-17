// Peak SysEx: requests, the 527-byte patch message, and values inside the 510 patch data bytes.
// The format is unofficial (community + checks on the user's Peak, firmware 2.1.0.8); see docs/RESEARCH.md.
//
//   F0 00 20 29 01 10 00 7E <cmd> 00 00 00 <bank> <slot> 00 00 <510 data bytes> F7
#pragma once

#include "ParamTable.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace peak
{

constexpr size_t kPatchDataBytes = 510;
constexpr size_t kPatchHeaderBytes = 16;
constexpr size_t kPatchMessageBytes = kPatchHeaderBytes + kPatchDataBytes + 1;  // 527
constexpr size_t kNameBytes = 16;

using PatchData = std::array<uint8_t, kPatchDataBytes>;

enum class PatchCmd : uint8_t
{
    EditBuffer = 0x00,     // patch to / from the edit buffer (both directions confirmed on the synth)
    WriteSlot = 0x01,      // write a patch to bank/slot: writes the synth's memory (confirmed on the synth)
    RequestEdit = 0x40,    // confirmed
    RequestSlot = 0x41,    // community
};

std::vector<uint8_t> requestDeviceInquiry();                 // F0 7E 7F 06 01 F7
std::vector<uint8_t> requestEditBuffer();                    // F0 00 20 29 01 10 00 7E 40 00 00 00 00 00 F7
std::vector<uint8_t> requestPatch (int bank, int slot);      // bank 1..4 (A-D), slot 0..127

// Patch to the edit buffer: changes the sound playing, not the synth's memory.
std::vector<uint8_t> editBufferDump (const PatchData& data);
// Patch into a memory slot. Only behind an explicit Save, with a backup of the slot first.
std::vector<uint8_t> writeSlotDump (int bank, int slot, const PatchData& data);

struct PatchMessage
{
    PatchCmd cmd = PatchCmd::EditBuffer;
    int bank = 0, slot = 0;  // 0 / 0 for the edit buffer
    PatchData data {};
};
// A 527-byte patch message (cmd 00 or 01). Nothing for anything else.
std::optional<PatchMessage> parsePatchMessage (const uint8_t* msg, size_t size);

struct DeviceInfo
{
    bool isPeak = false;     // Novation 00 20 29, family 7E 00
    std::string firmware;    // "2.1.0.8"
};
std::optional<DeviceInfo> parseDeviceInquiryReply (const uint8_t* msg, size_t size);

// Patch name: 16 ASCII bytes, space padded. Non-printable bytes become spaces.
std::string patchName (const PatchData& data);
void setPatchName (PatchData& data, const std::string& name);

// A parameter's value in the patch; -1 when its byte is not mapped yet. 0-255 values take two bytes.
int readValue (const ParamDef& p, const PatchData& data);
// Returns false (and changes nothing) when the byte is not mapped yet.
bool writeValue (const ParamDef& p, PatchData& data, int value);

// Bytes as "7E 7F 06", at most maxBytes of them, then " ..." when there are more.
std::string toHex (const uint8_t* msg, size_t size, size_t maxBytes = 32);

} // namespace peak
