#include "Link.h"

#include <algorithm>
#include <cctype>

namespace peak
{

namespace
{
std::string lower (std::string s)
{
    std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
    return s;
}
} // namespace

PortPick pickPort (const std::vector<std::string>& names, const std::string& choice)
{
    PortPick r;
    if (! choice.empty())
    {
        for (size_t i = 0; i < names.size(); ++i)
            if (names[i] == choice)
            {
                r.index = (int) i;
                return r;
            }
        r.savedMissing = true;
    }
    for (size_t i = 0; i < names.size(); ++i)
        if (lower (names[i]).find ("peak") != std::string::npos)
        {
            r.index = (int) i;
            break;
        }
    return r;
}

std::vector<ShortMsg> programChange (int channel, int bank, int slot)
{
    const auto st = [&] (uint8_t kind) { return (uint8_t) (kind | ((channel - 1) & 0x0F)); };
    return { { st (0xB0), 0, 0 },
             { st (0xB0), 32, (uint8_t) std::clamp (bank - 1, 0, 3) },
             { st (0xC0), (uint8_t) std::clamp (slot, 0, 127), 0 } };
}

std::vector<CheckLine> checklist (const LinkStatus& s)
{
    using L = CheckLine::Level;
    std::vector<CheckLine> out;
    auto add = [&] (L l, std::string t) { out.push_back ({ l, std::move (t) }); };

    // Ports
    if (s.inName.empty())
        add (L::Bad, "No MIDI input named \"Peak\". Is the Peak on and connected by USB?");
    else if (s.inBusy)
        add (L::Bad, "Input \"" + s.inName + "\" is busy: close Novation Components, MIDISynth or a DAW that holds it.");
    else if (s.inOpen)
        add (L::Ok, "Input: " + s.inName);
    if (s.inSavedMissing)
        add (L::Warn, "The chosen input port is missing; using automatic choice.");

    if (s.hostOutput)
        add (L::Info, "Output: through the DAW's MIDI out (route the plugin's MIDI out to the Peak).");
    else if (s.outName.empty())
        add (L::Bad, "No MIDI output named \"Peak\".");
    else if (s.outBusy)
        add (L::Bad, "Output \"" + s.outName + "\" is busy: close the program that holds it.");
    else if (s.outOpen)
        add (L::Ok, "Output: " + s.outName);
    if (s.outSavedMissing)
        add (L::Warn, "The chosen output port is missing; using automatic choice.");

    // Identity
    if (s.inquiryReplyMs >= 0)
    {
        if (s.isPeak)
            add (L::Ok, "Peak found, firmware " + s.firmware + (s.firmware.rfind ("2.", 0) == 0 ? "" : " (tested with 2.1)"));
        else
            add (L::Warn, "A device answered, but it is not a Peak.");
    }
    else if (s.inquirySentMs >= 0 && s.nowMs - s.inquirySentMs > 1500)
        add (L::Bad, "No answer to the identity request. Check the USB cable and that the port is the Peak's.");

    // Parameters coming back
    if (s.ccInCount > 0 && s.lastCcChannel != s.pluginChannel)
        add (L::Warn, "The Peak sends on channel " + std::to_string (s.lastCcChannel) + ", the plugin uses "
                          + std::to_string (s.pluginChannel) + ".");
    if (s.ccInCount == 0 && s.inOpen)
        add (L::Info, "Turn a knob on the Peak: if nothing shows up, set Settings > CC/NRPN to Rec+Trans.");
    if (s.echoCount > 0)
        add (L::Warn, "The plugin's own messages come back (" + std::to_string (s.echoCount)
                          + "): a MIDI loop through the DAW or Local Off routing.");
    if (s.sharedBy > 1)
        add (L::Info, std::to_string (s.sharedBy) + " plugin copies share the Peak's port.");
    return out;
}

} // namespace peak
