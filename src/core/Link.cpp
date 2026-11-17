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
std::string quoted (const std::string& s) { return "\"" + s + "\""; }
std::string num (int v) { return std::to_string (v); }
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
    return { { st (0xB0), 32, (uint8_t) std::clamp (bank, 1, 4) },
             { st (0xC0), (uint8_t) std::clamp (slot, 0, 127), 0 } };
}

Report diagnose (const LinkStatus& s)
{
    using H = Health;
    Report r;
    enum { kIn, kOut, kAnswer, kChannel, kHears };

    // 1. Input port
    CheckItem in;
    in.title = "Input port";
    if (s.inName.empty())
    {
        in.health = H::Bad;
        in.detail = "No MIDI input named \"Peak\".";
        in.fix = "Switch the Peak on and connect it by USB: the plugin finds it within 2 seconds. Or pick its port by hand.";
        in.headline = "The Peak isn't connected";
    }
    else if (s.inBusy)
    {
        in.health = H::Bad;
        in.detail = quoted (s.inName) + " is there, but another program holds it.";
        in.fix = "Close Novation Components, MIDISynth, the standalone Peak Control or a DAW that has the port, "
                 "then press Check again.";
        in.headline = "Another program holds the Peak's port";
    }
    else if (s.inOpen)
    {
        in.health = H::Ok;
        in.detail = quoted (s.inName) + " is open.";
    }
    else
        in.detail = "Opening " + quoted (s.inName) + ".";
    if (s.inSavedMissing)
        in.detail += " The port you picked is missing, so the automatic choice is used.";
    r.steps.push_back (in);

    // 2. Output port
    CheckItem out;
    out.title = "Output port";
    if (s.hostOutput)
    {
        out.health = H::Ok;
        out.detail = "Through the DAW's MIDI out: route the plugin's MIDI output to the Peak.";
    }
    else if (s.outName.empty())
    {
        out.health = H::Bad;
        out.detail = "No MIDI output named \"Peak\".";
        out.fix = "Switch the Peak on and connect it by USB, or pick its port by hand.";
        out.headline = "The Peak isn't connected";
    }
    else if (s.outBusy)
    {
        out.health = H::Bad;
        out.detail = quoted (s.outName) + " is there, but another program holds it.";
        out.fix = "Close the program that has the Peak's port, then press Check again.";
        out.headline = "Another program holds the Peak's port";
    }
    else if (s.outOpen)
    {
        out.health = H::Ok;
        out.detail = quoted (s.outName) + " is open.";
    }
    else
        out.detail = "Opening " + quoted (s.outName) + ".";
    if (s.outSavedMissing && ! s.hostOutput)
        out.detail += " The port you picked is missing, so the automatic choice is used.";
    r.steps.push_back (out);

    const bool portsOk = in.health == H::Ok && out.health == H::Ok;

    // 3. The Peak answers the identity request
    CheckItem answer;
    answer.title = "The Peak answers";
    answer.blocked = ! portsOk;
    if (! portsOk)
        answer.detail = "Waits for the ports.";
    else if (s.inquiryReplyMs >= 0)
    {
        if (s.isPeak)
        {
            answer.health = H::Ok;
            answer.detail = "Firmware " + s.firmware + ".";
            if (s.firmware.rfind ("2.", 0) != 0)
                answer.detail += " Tested with 2.1.";
        }
        else
        {
            answer.health = H::Warn;
            answer.detail = "A device answered, but it is not a Peak.";
            answer.fix = "Pick the Peak's ports by hand.";
            answer.headline = "This isn't a Peak";
        }
    }
    else if (s.inquirySentMs >= 0 && s.nowMs - s.inquirySentMs > kInquiryTimeoutMs)
    {
        answer.health = H::Bad;
        answer.detail = "No reply to the identity request.";
        answer.fix = s.hostOutput
                         ? "Many DAWs don't pass SysEx from a plugin. Set Output mode to \"Straight to the Peak's port\"."
                         : "Play the test note: if you hear it, the Peak gets MIDI but its replies don't come back. "
                           "If not, check that these ports are the Peak's and switch it off and on.";
        answer.headline = "The Peak doesn't answer";
    }
    else
        answer.detail = "Asking the Peak.";
    r.steps.push_back (answer);

    // 4. MIDI channel: the Peak's own setting can't be asked for; it shows in what the Peak sends.
    CheckItem chan;
    chan.title = "MIDI channel";
    chan.blocked = in.health != H::Ok;
    const std::string ch = num (s.pluginChannel);
    if (s.ccInCount == 0)
        chan.detail = s.followChannel ? "Follow the Peak learns it from the first knob move the Peak sends."
                                      : "Channel " + ch + ", picked by hand. Turn a knob on the Peak to check it.";
    else if (s.lastCcChannel == s.pluginChannel)
    {
        chan.health = H::Ok;
        chan.detail = s.followChannel ? "Following the Peak: channel " + ch + "." : "Channel " + ch + ", the same as the Peak's.";
    }
    else
    {
        const std::string peakCh = num (s.lastCcChannel);
        chan.health = H::Bad;
        chan.detail = s.followChannel ? "The Peak sends on channel " + peakCh + ", the plugin uses " + ch + "."
                                      : "Channel " + ch + " was picked by hand, but the Peak sends on channel " + peakCh + ".";
        chan.fix = "Use channel " + peakCh + " (turns Follow the Peak back on), or set the Peak's Settings > MIDI Channel to "
                   + ch + ".";
        chan.headline = "The Peak is on channel " + peakCh;
        r.useChannel = s.lastCcChannel;
    }
    r.steps.push_back (chan);

    // 5. The Peak hears the plugin: only the user can tell.
    CheckItem hears;
    hears.title = "The Peak hears the plugin";
    hears.optional = true;
    hears.blocked = out.health != H::Ok;
    const std::string testCh = num (s.testNoteChannel);
    switch (s.noteTest)
    {
        case NoteTest::NotRun:
            hears.detail = "Not tested. Play test note sends a short C3.";
            break;
        case NoteTest::Waiting:
            hears.health = H::Warn;
            hears.detail = "Test note sent on channel " + testCh + ". Waiting for your answer.";
            break;
        case NoteTest::Heard:
            hears.health = H::Ok;
            hears.detail = "You heard the test note.";
            break;
        case NoteTest::NotHeard:
            hears.health = H::Bad;
            hears.detail = "You didn't hear the test note (channel " + testCh + ").";
            hears.fix = answer.health == H::Ok
                            ? "The Peak answers, so the cable works. Check its volume and that its MIDI channel is " + testCh + "."
                            : "Nothing gets through. Try another USB port or cable, and switch the Peak off and on.";
            hears.headline = "The Peak doesn't play the test note";
            break;
    }
    r.steps.push_back (hears);

    // Also note
    if (in.health == H::Ok)
    {
        CheckItem knobs;
        knobs.title = "Knob moves come back";
        knobs.optional = true;
        if (s.ccInCount > 0)
        {
            knobs.health = H::Ok;
            knobs.detail = num (s.ccInCount) + " CC / NRPN from the Peak so far.";
        }
        else
            knobs.detail = "Nothing yet: turn a knob on the Peak. It needs Settings > CC/NRPN = Rec+Trans; "
                           "without it Follow the Peak can't learn the channel.";
        r.notes.push_back (knobs);

        CheckItem loop;
        loop.optional = true;
        loop.health = H::Ok;
        loop.title = "No MIDI loop";
        loop.detail = "None of the plugin's own messages came back.";
        if (s.echoCount > 0)
        {
            loop.health = H::Warn;
            loop.title = "MIDI loop";
            loop.detail = num (s.echoCount) + " of the plugin's own messages came back.";
            loop.fix = "In the DAW, don't route the Peak's input back to its output.";
        }
        r.notes.push_back (loop);
    }
    if (s.sharedBy > 1)
    {
        CheckItem shared;
        shared.optional = true;
        shared.title = "Port shared";
        shared.detail = num (s.sharedBy) + " copies of the plugin share the Peak's port.";
        r.notes.push_back (shared);
    }

    // Verdict: the test note question first, then the first required step that isn't Ok.
    r.connected = portsOk && answer.health == H::Ok && chan.health != H::Bad;
    if (s.noteTest == NoteTest::Waiting && out.health == H::Ok)
    {
        r.verdictHealth = H::Warn;
        r.verdict = "Did you hear a short C3 from the Peak?";
        r.verdictText = "Turn the Peak's volume up. The answer tells a dead connection from a one-way one.";
        r.actions = { Action::AnswerTestNote };
        return r;
    }
    for (int i = kIn; i <= kChannel; ++i)
    {
        const auto& st = r.steps[(size_t) i];
        if (st.health == H::Ok || st.blocked)
            continue;
        if (st.health == H::Unknown)
        {
            if (i == kChannel)
                break;  // an unknown channel stops nothing
            r.verdictHealth = H::Unknown;
            r.verdict = "Checking";
            r.verdictText = st.detail;
            return r;
        }
        r.verdictHealth = st.health;
        r.verdict = st.headline;
        r.verdictText = st.detail + " " + st.fix;
        if (i == kAnswer)
            r.actions = s.hostOutput ? std::vector<Action> { Action::CheckAgain }
                                     : std::vector<Action> { Action::PlayTestNote, Action::CheckAgain };
        else if (i == kChannel)
            r.actions = { Action::UseChannel };
        else
            r.actions = { Action::CheckAgain };
        return r;
    }
    if (hears.health == H::Bad && ! hears.blocked)
    {
        r.verdictHealth = H::Bad;
        r.verdict = hears.headline;
        r.verdictText = hears.fix;
        r.actions = { Action::PlayTestNote };
        return r;
    }
    r.verdictHealth = H::Ok;
    r.verdict = "Connected";
    r.verdictText = chan.health == H::Ok ? "The plugin and the Peak talk both ways on channel " + ch + "."
                                         : "The Peak answers. Turn a knob on the Peak to confirm the MIDI channel.";
    return r;
}

std::string paramAddress (const ParamDef& p)
{
    switch (p.addr)
    {
        case Addr::Cc:        return "CC " + num (p.num);
        case Addr::CcPair:    return "CC " + num (p.num) + "+" + num (p.num2);
        case Addr::Nrpn:      return "NRPN " + num (p.num) + ":" + num (p.num2);
        case Addr::PatchOnly: break;
    }
    return "patch only";
}

} // namespace peak
