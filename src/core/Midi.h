// Live parameter messages for the Peak: CC, 0-255 CC pairs and NRPN, both directions.
#pragma once

#include "ParamTable.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace peak
{

struct Cc
{
    uint8_t controller;
    uint8_t value;
};

// The CCs that set a parameter, in sending order. Nothing for a PatchOnly parameter.
//   Cc:     one CC.
//   CcPair: MSB CC = value >> 1, then LSB CC = (value & 1) << 6  (the patch's own 0-255 coding; to verify live).
//   Nrpn:   CC 99 = MSB, CC 98 = LSB, CC 6 = value. No CC 38: on the Peak that is Osc 2 ModEnv2 > Pitch.
std::vector<Cc> encodeParam (const ParamDef& p, int value);

// The same as raw MIDI bytes (status, controller, value per CC, no running status). channel is 1..16.
std::vector<uint8_t> encodeParamBytes (const ParamDef& p, int value, int channel);

// The 0-255 coding shared by CC pairs and the patch's two-byte values.
inline uint8_t wideHigh (int value) { return (uint8_t) ((value >> 1) & 0x7F); }
inline uint8_t wideLow (int value) { return (uint8_t) ((value & 1) << 6); }
inline int wideValue (uint8_t high, uint8_t low) { return ((high & 0x7F) << 1) | ((low >> 6) & 1); }

struct ParamChange
{
    int index;  // into paramTable()
    int value;
};

// Turns incoming CCs (one channel) into parameter changes.
class CcDecoder
{
public:
    std::optional<ParamChange> feed (uint8_t controller, uint8_t value);
    void reset();

private:
    int nrpnMsb = -1, nrpnLsb = -1;
    int pairHigh[128] {};  // last MSB seen per CC-pair MSB controller
};

} // namespace peak
