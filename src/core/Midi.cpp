#include "Midi.h"

namespace peak
{

int encodeParamInto (const ParamDef& p, int value, Cc out[3])
{
    value = clampValue (p, value);
    switch (p.addr)
    {
        case Addr::Cc:
            out[0] = { (uint8_t) p.num, (uint8_t) value };
            return 1;
        case Addr::CcPair:
            out[0] = { (uint8_t) p.num, wideHigh (value) };
            out[1] = { (uint8_t) p.num2, wideLow (value) };
            return 2;
        case Addr::Nrpn:
            out[0] = { 99, (uint8_t) p.num };
            out[1] = { 98, (uint8_t) p.num2 };
            out[2] = { 6, (uint8_t) value };
            return 3;
        case Addr::PatchOnly:
            break;
    }
    return 0;
}

std::vector<Cc> encodeParam (const ParamDef& p, int value)
{
    Cc c[3];
    const int n = encodeParamInto (p, value, c);
    return std::vector<Cc> (c, c + n);
}

std::vector<uint8_t> encodeParamBytes (const ParamDef& p, int value, int channel)
{
    const auto status = (uint8_t) (0xB0 | ((channel - 1) & 0x0F));
    std::vector<uint8_t> out;
    for (auto cc : encodeParam (p, value))
    {
        out.push_back (status);
        out.push_back (cc.controller);
        out.push_back (cc.value);
    }
    return out;
}

std::optional<ParamChange> CcDecoder::feed (uint8_t controller, uint8_t value)
{
    controller &= 0x7F;
    value &= 0x7F;

    if (controller == 99) { nrpnMsb = value; return std::nullopt; }
    if (controller == 98) { nrpnLsb = value; return std::nullopt; }
    if (controller == 6)
    {
        if (nrpnMsb < 0 || nrpnLsb < 0)
            return std::nullopt;
        const int i = indexOfNrpn (nrpnMsb, nrpnLsb);
        if (i < 0)
            return std::nullopt;
        return ParamChange { i, clampValue (paramTable()[(size_t) i], value) };
    }

    const int i = indexOfCc (controller);
    if (i < 0)
        return std::nullopt;
    const auto& p = paramTable()[(size_t) i];
    if (p.addr == Addr::Cc)
        return ParamChange { i, clampValue (p, value) };

    // CC pair: the MSB alone sets the value with the low bit cleared; the LSB that follows fills in the low bit.
    if (controller == p.num)
    {
        pairHigh[p.num] = value;
        return ParamChange { i, clampValue (p, wideValue (value, 0)) };
    }
    return ParamChange { i, clampValue (p, wideValue ((uint8_t) pairHigh[p.num], value)) };
}

void CcDecoder::reset()
{
    nrpnMsb = nrpnLsb = -1;
    for (auto& h : pairHigh)
        h = 0;
}

} // namespace peak
