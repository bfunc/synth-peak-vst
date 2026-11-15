#include "Sysex.h"

#include "Midi.h"

#include <cstdio>

namespace peak
{

namespace
{

const uint8_t kHeader[] = { 0xF0, 0x00, 0x20, 0x29, 0x01, 0x10, 0x00, 0x7E };

std::vector<uint8_t> patchMessage (PatchCmd cmd, int bank, int slot, const PatchData& data)
{
    std::vector<uint8_t> m (std::begin (kHeader), std::end (kHeader));
    m.insert (m.end(), { (uint8_t) cmd, 0, 0, 0, (uint8_t) (bank & 0x7F), (uint8_t) (slot & 0x7F), 0, 0 });
    for (auto b : data)
        m.push_back (b & 0x7F);
    m.push_back (0xF7);
    return m;
}

bool hasHeader (const uint8_t* msg, size_t size)
{
    if (size < sizeof (kHeader) + 2 || msg[size - 1] != 0xF7)
        return false;
    for (size_t i = 0; i < sizeof (kHeader); ++i)
        if (msg[i] != kHeader[i])
            return false;
    return true;
}

} // namespace

std::vector<uint8_t> requestDeviceInquiry() { return { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 }; }

std::vector<uint8_t> requestEditBuffer()
{
    std::vector<uint8_t> m (std::begin (kHeader), std::end (kHeader));
    m.insert (m.end(), { 0x40, 0, 0, 0, 0, 0, 0xF7 });
    return m;
}

std::vector<uint8_t> requestPatch (int bank, int slot)
{
    std::vector<uint8_t> m (std::begin (kHeader), std::end (kHeader));
    m.insert (m.end(), { 0x41, 0, 0, 0, (uint8_t) (bank & 0x7F), (uint8_t) (slot & 0x7F), 0xF7 });
    return m;
}

std::vector<uint8_t> editBufferDump (const PatchData& data) { return patchMessage (PatchCmd::EditBuffer, 0, 0, data); }

std::vector<uint8_t> writeSlotDump (int bank, int slot, const PatchData& data)
{
    return patchMessage (PatchCmd::WriteSlot, bank, slot, data);
}

std::optional<PatchMessage> parsePatchMessage (const uint8_t* msg, size_t size)
{
    if (size != kPatchMessageBytes || ! hasHeader (msg, size))
        return std::nullopt;
    const auto cmd = msg[8];
    if (cmd != (uint8_t) PatchCmd::EditBuffer && cmd != (uint8_t) PatchCmd::WriteSlot)
        return std::nullopt;
    PatchMessage p;
    p.cmd = (PatchCmd) cmd;
    p.bank = msg[12];
    p.slot = msg[13];
    for (size_t i = 0; i < kPatchDataBytes; ++i)
        p.data[i] = msg[kPatchHeaderBytes + i];
    return p;
}

std::optional<DeviceInfo> parseDeviceInquiryReply (const uint8_t* msg, size_t size)
{
    // F0 7E <ch> 06 02 <00 20 29> <7E 00> <00 00> <v1 v2 v3 v4> F7
    if (size < 17 || msg[0] != 0xF0 || msg[1] != 0x7E || msg[3] != 0x06 || msg[4] != 0x02 || msg[size - 1] != 0xF7)
        return std::nullopt;
    DeviceInfo d;
    d.isPeak = msg[5] == 0x00 && msg[6] == 0x20 && msg[7] == 0x29 && msg[8] == 0x7E && msg[9] == 0x00;
    char buf[32];
    std::snprintf (buf, sizeof (buf), "%d.%d.%d.%d", msg[12], msg[13], msg[14], msg[15]);
    d.firmware = buf;
    return d;
}

std::string patchName (const PatchData& data)
{
    std::string s;
    for (size_t i = 0; i < kNameBytes; ++i)
    {
        const char c = (char) data[i];
        s += (c >= 32 && c < 127) ? c : ' ';
    }
    while (! s.empty() && s.back() == ' ')
        s.pop_back();
    return s;
}

void setPatchName (PatchData& data, const std::string& name)
{
    for (size_t i = 0; i < kNameBytes; ++i)
    {
        const char c = i < name.size() ? name[i] : ' ';
        data[i] = (uint8_t) ((c >= 32 && c < 127) ? c : ' ');
    }
}

int readValue (const ParamDef& p, const PatchData& data)
{
    if (p.offset < 0)
        return -1;
    const auto o = (size_t) p.offset;
    if (p.wide())
        return o + 1 < kPatchDataBytes ? wideValue (data[o], data[o + 1]) : -1;
    return o < kPatchDataBytes ? data[o] : -1;
}

bool writeValue (const ParamDef& p, PatchData& data, int value)
{
    if (p.offset < 0)
        return false;
    value = clampValue (p, value);
    const auto o = (size_t) p.offset;
    if (p.wide())
    {
        if (o + 1 >= kPatchDataBytes)
            return false;
        data[o] = wideHigh (value);
        data[o + 1] = wideLow (value);
        return true;
    }
    if (o >= kPatchDataBytes)
        return false;
    data[o] = (uint8_t) value;
    return true;
}

std::string toHex (const uint8_t* msg, size_t size, size_t maxBytes)
{
    std::string s;
    char buf[4];
    for (size_t i = 0; i < size && i < maxBytes; ++i)
    {
        std::snprintf (buf, sizeof (buf), i ? " %02X" : "%02X", msg[i]);
        s += buf;
    }
    if (size > maxBytes)
        s += " ...";
    return s;
}

} // namespace peak
