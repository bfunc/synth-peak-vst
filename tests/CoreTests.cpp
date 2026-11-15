// Core tests: plain C++, no framework, no JUCE. Exit code 0 = all passed.
// PEAK_TEST_DATA points at docs/dumps (set by CMake or the Docker script).

#include "core/Midi.h"
#include "core/ParamTable.h"
#include "core/SendQueue.h"
#include "core/Sysex.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

using namespace peak;

static int failures = 0;
#define CHECK(cond)                                                      \
    do                                                                   \
    {                                                                    \
        if (! (cond))                                                    \
        {                                                                \
            std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                  \
        }                                                                \
    } while (0)

static const ParamDef& param (const char* id)
{
    const int i = indexOfId (id);
    if (i < 0)
    {
        std::printf ("FAIL unknown parameter %s\n", id);
        ++failures;
        return paramTable().front();
    }
    return paramTable()[(size_t) i];
}

static std::vector<uint8_t> readFile (const std::string& path)
{
    std::ifstream f (path, std::ios::binary);
    return { std::istreambuf_iterator<char> (f), std::istreambuf_iterator<char>() };
}

static void testTable()
{
    const auto& t = paramTable();
    CHECK (t.size() > 200);

    std::set<std::string> ids;
    std::set<int> ccs, nrpns, offsets;
    for (const auto& p : t)
    {
        CHECK (ids.insert (p.id).second);  // unique ids
        CHECK (p.min <= p.def && p.def <= p.max);
        CHECK (p.min >= 0 && p.max <= 255);
        if (p.kind == Kind::Choice && p.choices.size() != (size_t) (p.max - p.min + 1))
        {
            std::printf ("FAIL %s: %zu names for %d values\n", p.id.c_str(), p.choices.size(), p.max - p.min + 1);
            ++failures;
        }
        if (p.kind == Kind::Bipolar)
            CHECK (p.min <= p.center && p.center <= p.max);
        if (p.kind == Kind::Bool)
            CHECK (p.min == 0 && p.max == 1);

        switch (p.addr)
        {
            case Addr::Cc:
                CHECK (p.max <= 127);
                CHECK (ccs.insert (p.num).second);
                break;
            case Addr::CcPair:
                CHECK (p.max > 127);
                CHECK (ccs.insert (p.num).second);
                CHECK (ccs.insert (p.num2).second);
                break;
            case Addr::Nrpn:
                CHECK (p.max <= 127);
                CHECK (nrpns.insert (p.num * 128 + p.num2).second);
                break;
            case Addr::PatchOnly:
                CHECK (p.offset >= 0);  // otherwise it could never reach the synth
                break;
        }
        // No parameter on the NRPN controllers (CC 38 is a real parameter on the Peak).
        if (p.addr == Addr::Cc || p.addr == Addr::CcPair)
            CHECK (p.num != 6 && p.num != 98 && p.num != 99 && p.num2 != 6 && p.num2 != 98 && p.num2 != 99);

        if (p.offset >= 0)
        {
            CHECK (p.offset + (p.wide() ? 1 : 0) < (int) kPatchDataBytes);
            CHECK (offsets.insert (p.offset).second);  // no two parameters on one byte
            if (p.wide())
                CHECK (offsets.insert (p.offset + 1).second);
        }
    }

    CHECK (indexOfCc (38) == indexOfId ("osc2_env2_pitch"));
    CHECK (indexOfCc (46) == indexOfId ("osc1_coarse"));  // LSB of a pair finds the pair
    CHECK (indexOfNrpn (0, 117) == indexOfId ("arp_type"));
    CHECK (indexOfNrpn (16, 3) == indexOfId ("mod16_dest"));
    CHECK (indexOfNrpn (0, 8) == -1);
}

static void testFormat()
{
    CHECK (formatValue (param ("osc1_coarse"), 128) == "0");
    CHECK (formatValue (param ("osc1_coarse"), 140) == "+12");
    CHECK (formatValue (param ("osc1_coarse"), 116) == "-12");
    CHECK (formatValue (param ("osc1_wave"), 2) == "Sawtooth");
    CHECK (formatValue (param ("osc1_wavetable"), 4) == "BS sine");
    CHECK (formatValue (param ("osc1_wavetable"), 63) == "Yeah");
    CHECK (formatValue (param ("osc2_range"), 64) == "8'");
    CHECK (formatValue (param ("glide_on"), 1) == "On");
    CHECK (formatValue (param ("osc3_fixed_note"), 0) == "Off");
    CHECK (formatValue (param ("arp_sync"), 15) == "16th");
    CHECK (formatValue (param ("lfo1_sync"), 34) == "32nd T");
    CHECK (formatValue (param ("flt_res"), 999) == "127");  // clamped
}

static void testEncode()
{
    // CC
    auto c = encodeParam (param ("flt_res"), 100);
    CHECK (c.size() == 1 && c[0].controller == 79 && c[0].value == 100);

    // CC pair: 173 = 86 << 1 | 1 -> MSB 0x56, LSB 0x40 (the patch example from the community notes)
    c = encodeParam (param ("flt_freq"), 173);
    CHECK (c.size() == 2);
    CHECK (c[0].controller == 29 && c[0].value == 0x56);
    CHECK (c[1].controller == 61 && c[1].value == 0x40);
    c = encodeParam (param ("flt_freq"), 255);
    CHECK (c[0].value == 127 && c[1].value == 64);

    // NRPN: 99, 98, 6; no 38
    c = encodeParam (param ("arp_type"), 5);
    CHECK (c.size() == 3);
    CHECK (c[0].controller == 99 && c[0].value == 0);
    CHECK (c[1].controller == 98 && c[1].value == 117);
    CHECK (c[2].controller == 6 && c[2].value == 5);
    c = encodeParam (param ("mod3_depth"), 80);
    CHECK (c[0].value == 3 && c[1].value == 2 && c[2].value == 80);

    // PatchOnly: nothing live
    CHECK (encodeParam (param ("fxmod1_dest"), 3).empty());

    // raw bytes, channel 16, value clamped into range
    auto b = encodeParamBytes (param ("kbd_octave"), 99, 16);
    const uint8_t want[9] = { 0xBF, 99, 0, 0xBF, 98, 6, 0xBF, 6, 67 };
    CHECK (b.size() == 9);
    for (size_t i = 0; i < 9 && i < b.size(); ++i)
        CHECK (b[i] == want[i]);
}

static void testDecode()
{
    CcDecoder d;

    // CC 38 is Osc 2 ModEnv2 > Pitch, never NRPN data
    auto r = d.feed (38, 70);
    CHECK (r && r->index == indexOfId ("osc2_env2_pitch") && r->value == 70);

    // NRPN
    CHECK (! d.feed (99, 0));
    CHECK (! d.feed (98, 46));
    r = d.feed (6, 2);
    CHECK (r && r->index == indexOfId ("flt_shape") && r->value == 2);
    r = d.feed (6, 1);  // data entry again: same parameter
    CHECK (r && r->value == 1);
    CHECK (! d.feed (98, 8));  // unknown NRPN 0:8
    CHECK (! d.feed (6, 1));

    // CC pair
    r = d.feed (29, 0x56);
    CHECK (r && r->index == indexOfId ("flt_freq") && r->value == 172);
    r = d.feed (61, 0x40);
    CHECK (r && r->value == 173);

    // round trip every live parameter at a few values
    for (const auto& p : paramTable())
    {
        if (p.addr == Addr::PatchOnly)
            continue;
        for (int v : { p.min, p.def, p.max })
        {
            CcDecoder dd;
            std::optional<ParamChange> last;
            for (auto cc : encodeParam (p, v))
                if (auto x = dd.feed (cc.controller, cc.value))
                    last = x;
            if (! last || paramTable()[(size_t) last->index].id != p.id || last->value != v)
            {
                std::printf ("FAIL round trip %s = %d\n", p.id.c_str(), v);
                ++failures;
            }
        }
    }
}

static void testRequests()
{
    const std::vector<uint8_t> edit { 0xF0, 0x00, 0x20, 0x29, 0x01, 0x10, 0x00, 0x7E, 0x40, 0, 0, 0, 0, 0, 0xF7 };
    CHECK (requestEditBuffer() == edit);
    const std::vector<uint8_t> slot { 0xF0, 0x00, 0x20, 0x29, 0x01, 0x10, 0x00, 0x7E, 0x41, 0, 0, 0, 2, 5, 0xF7 };
    CHECK (requestPatch (2, 5) == slot);
    CHECK (requestDeviceInquiry() == (std::vector<uint8_t> { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 }));

    // The user's Peak (2026-11-13)
    const uint8_t reply[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x02, 0x00, 0x20, 0x29, 0x7E, 0x00,
                              0x00, 0x00, 0x02, 0x01, 0x00, 0x08, 0xF7 };
    auto info = parseDeviceInquiryReply (reply, sizeof (reply));
    CHECK (info && info->isPeak && info->firmware == "2.1.0.8");
    CHECK (! parseDeviceInquiryReply (reply, 6));
}

static void testDump()
{
    const auto file = readFile (std::string (PEAK_TEST_DATA) + "/2026-11-13-editbuffer-perc-up-geo.syx");
    CHECK (file.size() == kPatchMessageBytes);
    auto msg = parsePatchMessage (file.data(), file.size());
    CHECK (msg.has_value());
    if (! msg)
        return;
    CHECK (msg->cmd == PatchCmd::EditBuffer && msg->bank == 0 && msg->slot == 0);
    const auto& d = msg->data;

    CHECK (patchName (d) == "Perc Up GEO");
    CHECK (readValue (param ("category"), d) == 6);
    CHECK (readValue (param ("voice_mode"), d) == 3);
    CHECK (readValue (param ("unison_detune"), d) == 25);
    CHECK (readValue (param ("spread"), d) == 2);
    CHECK (readValue (param ("kbd_octave"), d) == 64);
    CHECK (readValue (param ("glide_time"), d) == 60);
    CHECK (readValue (param ("arp_chance"), d) == 100);
    CHECK (readValue (param ("osc1_coarse"), d) == -1);  // not mapped yet

    // Mod matrix slot 2: ModWheel -> FiltFreq, +6
    CHECK (formatValue (param ("mod2_src_a"), readValue (param ("mod2_src_a"), d)) == "ModWheel");
    CHECK (formatValue (param ("mod2_dest"), readValue (param ("mod2_dest"), d)) == "FiltFreq");
    CHECK (formatValue (param ("mod2_depth"), readValue (param ("mod2_depth"), d)) == "+6");
    CHECK (readValue (param ("mod5_depth"), d) == 64);  // empty slot
    // FX matrix slot 3: Animate1 -> Del Lev
    CHECK (formatValue (param ("fxmod3_src_a"), readValue (param ("fxmod3_src_a"), d)) == "Animate1");
    CHECK (formatValue (param ("fxmod3_dest"), readValue (param ("fxmod3_dest"), d)) == "Del Lev");

    // Sending it back unchanged gives the same bytes as the reply
    CHECK (editBufferDump (d) == file);

    // Edits
    auto e = d;
    setPatchName (e, "New Name");
    CHECK (patchName (e) == "New Name");
    CHECK (e[8] == ' ' && e[15] == ' ');
    setPatchName (e, "A very long patch name");
    CHECK (patchName (e) == "A very long patc");
    CHECK (writeValue (param ("mod2_depth"), e, 30));
    CHECK (readValue (param ("mod2_depth"), e) == 30);
    CHECK (! writeValue (param ("osc1_coarse"), e, 140));  // unmapped: refused

    auto slot = writeSlotDump (4, 127, d);
    CHECK (slot.size() == kPatchMessageBytes && slot[8] == 0x01 && slot[12] == 4 && slot[13] == 127);
    auto back = parsePatchMessage (slot.data(), slot.size());
    CHECK (back && back->cmd == PatchCmd::WriteSlot && back->bank == 4 && back->slot == 127 && back->data == d);

    auto bad = file;
    bad[3] = 0x28;
    CHECK (! parsePatchMessage (bad.data(), bad.size()));
    CHECK (! parsePatchMessage (file.data(), file.size() - 1));
}

static void testWideCoding()
{
    for (int v = 0; v <= 255; ++v)
        CHECK (wideValue (wideHigh (v), wideLow (v)) == v);
    CHECK (wideHigh (173) == 0x56 && wideLow (173) == 0x40);
}

static void testSendQueue()
{
    SendQueue q (3, { 20.0, 1000.0, 2.0 });
    std::vector<std::pair<size_t, int>> out;
    auto grab = [&] (size_t i, int v) { out.push_back ({ i, v }); };

    q.setValue (0, 10);
    q.setValue (1, 20);
    q.setValue (2, 30);
    q.collect (0.0, grab);
    CHECK (out.size() == 2);  // burst of 2
    q.collect (5.0, grab);
    CHECK (out.size() == 3);
    CHECK (! q.pending());

    out.clear();
    q.setValue (0, 11);
    q.collect (10.0, grab);  // < 20 ms since the last send of 0
    CHECK (out.empty());
    q.collect (25.0, grab);
    CHECK (out.size() == 1 && out[0].second == 11);

    out.clear();
    q.markSynced (1, 99);
    q.collect (100.0, grab);
    CHECK (out.empty());
}

int main()
{
    testTable();
    testFormat();
    testEncode();
    testDecode();
    testRequests();
    testDump();
    testWideCoding();
    testSendQueue();
    if (failures)
        std::printf ("%d FAILED\n", failures);
    else
        std::printf ("all passed (%zu parameters)\n", paramTable().size());
    return failures ? 1 : 0;
}
