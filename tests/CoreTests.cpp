// Core tests: plain C++, no framework, no JUCE. Exit code 0 = all passed.
// PEAK_TEST_DATA points at docs/dumps (set by CMake or the Docker script).

#include "core/Library.h"
#include "core/Link.h"
#include "core/Midi.h"
#include "core/Patch.h"
#include "core/Timing.h"
#include "ui/Layout.h"
#include "ui/ModMap.h"
#include "core/ParamTable.h"
#include "core/SendQueue.h"
#include "core/SlotWrite.h"
#include "core/Sync.h"
#include "core/Sysex.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
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
    CHECK (formatValue (param ("osc1_wavetable"), 4) == "BS Sine");
    CHECK (formatValue (param ("osc1_wavetable"), 5) == "Random");
    CHECK (formatValue (param ("osc1_wavetable"), 19) == "String");
    CHECK (formatValue (param ("osc1_wavetable"), 63) == "Yeah");
    CHECK (formatValue (param ("osc1_wavetable"), 64) == "User 1");
    CHECK (formatValue (param ("osc2_range"), 64) == "8'");
    CHECK (formatValue (param ("glide_on"), 1) == "On");
    CHECK (formatValue (param ("osc3_fixed_note"), 0) == "Off");
    CHECK (formatValue (param ("arp_sync"), 15) == "16th");
    CHECK (formatValue (param ("dly_sync_time"), 0) == "4 beats" && formatValue (param ("dly_sync_time"), 8) == "4th T");
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

    // Animate buttons: 127 / 0 on the wire, on / off in the editor
    CHECK (encodeParam (param ("anim1_hold"), 1)[0].value == 127);
    CHECK (encodeParam (param ("glide_on"), 1)[0].value == 1);
    r = d.feed (114, 127);
    CHECK (r && r->index == indexOfId ("anim1_hold") && r->value == 1);
    r = d.feed (114, 0);
    CHECK (r && r->value == 0);

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
    CHECK (readValue (param ("anim1_hold"), d) == -1);   // a performance button: not in the patch
    CHECK (readValue (param ("osc1_coarse"), d) == 128);  // measured offset 47-48 (0-255 pair)
    CHECK (readValue (param ("osc1_range"), d) >= 63 && readValue (param ("osc1_range"), d) <= 66);

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
    CHECK (! writeValue (param ("anim1_hold"), e, 1));  // not in the patch: refused
    CHECK (writeValue (param ("osc1_coarse"), e, 141) && e[47] == 70 && e[48] == 64);

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


static void testPatchModel()
{
    const auto def = defaultValues();
    CHECK (def.size() == paramTable().size());
    const auto init = initPatch();
    CHECK (patchName (init) == "Init Patch");
    auto r = readValues (init, def);
    CHECK (r.values == def);
    CHECK (r.outOfRange.empty());
    CHECK (r.read > 50);

    const auto file = readFile (std::string (PEAK_TEST_DATA) + "/2026-11-13-editbuffer-perc-up-geo.syx");
    auto msg = parsePatchMessage (file.data(), file.size());
    CHECK (msg.has_value());
    if (! msg)
        return;
    r = readValues (msg->data, def);
    CHECK (r.outOfRange.empty());
    // Unmapped parameters keep the fallback; mapped ones come from the dump.
    CHECK (r.values[(size_t) indexOfId ("anim1_hold")] == 0);
    CHECK (r.values[(size_t) indexOfId ("unison_detune")] == 25);
    // Writing the values back over the same bytes changes nothing (unmapped bytes are kept).
    CHECK (writeValues (msg->data, r.values) == msg->data);

    auto changed = r.values;
    changed[(size_t) indexOfId ("mod2_depth")] = 10;
    const auto d = writeValues (msg->data, changed);
    CHECK (differing (r.values, changed).size() == 1);
    CHECK (d[250 + 4 + 2] == 10);
    CHECK (categoryName (msg->data) == "Keyboard");
}

static void testTiming()
{
    PatchRewrite pr (80, 300);
    CHECK (! pr.due (0));
    pr.touch (0);
    CHECK (! pr.due (50));   // still turning
    pr.touch (60);
    CHECK (! pr.due (100));
    CHECK (pr.due (150));    // quiet for 90 ms
    CHECK (! pr.due (151));  // sent
    pr.touch (200);
    CHECK (! pr.due (300));  // quiet, but < 300 ms since the last send
    CHECK (pr.due (460));

    FetchPlan fp ({ { 1, 0 }, { 1, 1 }, { 1, 2 } }, 1000, 1);
    auto a = fp.next (0);
    CHECK (a && a->slot == 0);
    CHECK (! fp.next (10));      // waiting for the reply
    fp.gotReply (1, 0);
    a = fp.next (20);
    CHECK (a && a->slot == 1);
    a = fp.next (1100);          // timeout: retry
    CHECK (a && a->slot == 1);
    a = fp.next (2200);          // second timeout: failed, next item
    CHECK (a && a->slot == 2);
    fp.gotReply (1, 1);          // a late reply for the failed one: ignored
    CHECK (! fp.done());
    fp.gotReply (1, 2);
    CHECK (fp.done() && fp.finished() == 3 && fp.failed() == 1);
    CHECK (fp.state (1) == FetchPlan::State::Failed);

    GateTracker g;
    g.noteOn (1, 60);
    g.noteOn (1, 64);
    g.noteOn (1, 60);  // again without an off: still one held, but counts as a trigger
    CHECK (g.held() == 2 && g.onCount() == 3);
    g.noteOff (1, 60);
    g.noteOff (1, 61);
    CHECK (g.held() == 1);
    g.allOff();
    CHECK (g.held() == 0);
    g.noteOn (17, 60);
    CHECK (g.held() == 0);

    CHECK (lfoBeatsPerCycle (25) == 1.0);  // "4th"
    CHECK (lfoBeatsPerCycle (0) == 64.0);
    CHECK (lfoBeatsPerCycle (99) == lfoBeatsPerCycle (34));
    CHECK (std::fabs (lfoSyncPhase (2.5, 25) - 0.5) < 1e-9);
    CHECK (std::fabs (lfoSyncPhase (3.0, 22) - 0.5) < 1e-9);  // "2nd" = 2 beats
    CHECK (lfoSyncPhase (-0.25, 25) > 0.7);
    CHECK (param ("lfo1_sync").choices.size() == 35);
}

static void testLink()
{
    CHECK (pickPort ({ "Microsoft GS", "Peak", "MIDIIN2 (Peak)" }, "").index == 1);
    CHECK (pickPort ({ "Microsoft GS", "MIDIIN2 (PEAK)" }, "").index == 1);
    auto p = pickPort ({ "A", "Peak" }, "Other");
    CHECK (p.index == 1 && p.savedMissing);
    CHECK (pickPort ({ "A", "B" }, "B").index == 1);
    CHECK (pickPort ({ "A" }, "").index == -1);

    auto pc = programChange (2, 3, 17);  // as the Peak sends it: CC 32 = 3 (bank C), PC 17
    CHECK (pc.size() == 2);
    CHECK (pc[0].status == 0xB1 && pc[0].d1 == 32 && pc[0].d2 == 3);
    CHECK (pc[1].status == 0xC1 && pc[1].d1 == 17);

    LinkStatus s;
    auto lines = checklist (s);
    CHECK (! lines.empty() && lines[0].level == CheckLine::Level::Bad);
    s.inName = s.outName = "Peak";
    s.inOpen = s.outOpen = true;
    s.inquirySentMs = 0;
    s.inquiryReplyMs = 100;
    s.isPeak = true;
    s.firmware = "2.1.0.8";
    s.nowMs = 5000;
    s.ccInCount = 3;
    s.lastCcChannel = s.pluginChannel = 1;
    for (const auto& l : checklist (s))
        CHECK (l.level == CheckLine::Level::Ok);
    s.lastCcChannel = 2;
    bool warned = false;
    for (const auto& l : checklist (s))
        warned |= l.level == CheckLine::Level::Warn;
    CHECK (warned);
}

static void testLibrary()
{
    CHECK (slotLabel (1, 0) == "A 000" && slotLabel (4, 127) == "D 127");
    Library lib;
    CHECK (lib.loadedCount() == 0);
    auto a = initPatch();
    setPatchName (a, "Big Bass");
    auto b = initPatch();
    setPatchName (b, "Soft Pad");
    lib.set (1, 5, a);
    lib.set (2, 0, b);
    lib.set (5, 0, b);  // no such bank: ignored
    CHECK (lib.loadedCount() == 2);
    CHECK (lib.at (1, 5).name == "Big Bass");
    CHECK (lib.filter ("bass", "", true).size() == 1);
    CHECK (lib.filter ("", "", true).size() == 2);
    CHECK (lib.filter ("", "", false).size() == 512);
    CHECK (lib.filter ("", "None", true).size() == 2);

    // .syx round trips
    auto one = parseSyx (patchFile (a));
    CHECK (one.size() == 1 && one[0].cmd == PatchCmd::EditBuffer && one[0].data == a);
    auto bank = bankFile (lib, 1);
    CHECK (bank.size() == kPatchMessageBytes);
    std::vector<uint8_t> junk { 0x90, 60, 100, 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
    junk.insert (junk.end(), bank.begin(), bank.end());
    auto parsed = parseSyx (junk);
    CHECK (parsed.size() == 1 && parsed[0].bank == 1 && parsed[0].slot == 5 && parsed[0].data == a);
}

static void testLayout()
{
    using namespace peak::ui;
    std::set<std::string> shown;
    for (const auto& p : panels())
    {
        CHECK (p.owns != nullptr);
        for (const auto& it : p.items)
        {
            if (indexOfId (it.id) < 0)
            {
                std::printf ("FAIL panel %s shows unknown parameter %s\n", p.key.c_str(), it.id.c_str());
                ++failures;
            }
            shown.insert (it.id);
        }
    }
    // Every parameter is either on a panel or in exactly one pop-out.
    std::map<int, int> inMore;
    for (const auto& p : panels())
        for (int i : moreParams (p.key))
            ++inMore[i];
    const auto& t = paramTable();
    for (int i = 0; i < (int) t.size(); ++i)
    {
        const bool onPanel = shown.count (t[(size_t) i].id) > 0;
        const int more = inMore.count (i) ? inMore[i] : 0;
        if (onPanel == (more > 0) || more > 1)
        {
            std::printf ("FAIL %s: on panel %d, in %d pop-outs\n", t[(size_t) i].id.c_str(), (int) onPanel, more);
            ++failures;
        }
    }
    CHECK (panel ("filter") != nullptr && panel ("nope") == nullptr);
    auto has = [] (const std::string& key, const char* id)
    {
        for (int i : moreParams (key))
            if (paramTable()[(size_t) i].id == id)
                return true;
        return false;
    };
    CHECK (has ("osc1", "osc1_fixed_note"));
    CHECK (has ("mixer", "noise_lpf"));
    CHECK (has ("voice", "pan"));
    CHECK (has ("reverb", "rev_mod"));
    CHECK (has ("env", "anim1_attack"));
}


static void testSync()
{
    SyncEngine e;
    auto values = defaultValues();
    e.markAllSynced (values.data(), values.size());

    std::vector<int> liveIdx, patchIdx;
    int ccCount = 0;
    auto run = [&] (double now)
    {
        e.collect (now, [&] (size_t i) { return values[i]; },
                   [&] (size_t i, int, const Cc*, int n) { liveIdx.push_back ((int) i); ccCount += n; },
                   [&] (size_t i, int) { patchIdx.push_back ((int) i); });
    };
    run (0);
    CHECK (liveIdx.empty() && patchIdx.empty());  // nothing changed

    values[(size_t) indexOfId ("flt_res")] = 99;         // CC
    values[(size_t) indexOfId ("arp_type")] = 3;         // NRPN
    values[(size_t) indexOfId ("fxmod1_dest")] = 4;      // patch only
    run (100);
    CHECK (liveIdx.size() == 2 && ccCount == 4);
    CHECK (patchIdx.size() == 1 && patchIdx[0] == indexOfId ("fxmod1_dest"));

    liveIdx.clear();
    e.markSynced ((size_t) indexOfId ("flt_res"), 50);  // the synth says 50 now; we still want 99
    run (200);
    CHECK (liveIdx.size() == 1);

    liveIdx.clear();
    patchIdx.clear();
    e.markAllDirty();
    for (int k = 0; k < 40; ++k)
        run (1000 + k * 50.0);
    CHECK ((int) (liveIdx.size() + patchIdx.size()) == (int) paramTable().size());

    // encodeParamInto agrees with encodeParam
    for (const auto& p : paramTable())
    {
        Cc c[3];
        const int n = encodeParamInto (p, p.def, c);
        const auto v = encodeParam (p, p.def);
        CHECK ((size_t) n == v.size());
        for (int k = 0; k < n; ++k)
            CHECK (c[k].controller == v[(size_t) k].controller && c[k].value == v[(size_t) k].value);
    }
}

static void testSlotWrite()
{
    using S = SlotWrite::Step;
    using A = SlotWrite::Action;
    auto patch = initPatch();
    setPatchName (patch, "New");
    auto old = initPatch();
    setPatchName (old, "Old");

    // The good path
    SlotWrite w (4, 127, patch, 1000, 100);
    CHECK (w.action (0) == A::RequestSlot);
    CHECK (w.action (10) == A::None);
    w.gotPatch (4, 126, old, 20);  // another slot: ignored
    CHECK (! w.backup());
    w.gotPatch (4, 127, old, 30);
    CHECK (w.backup() && *w.backup() == old);
    CHECK (w.action (40) == A::None);  // waits until the backup is stored
    w.backupStored (true);
    CHECK (w.action (50) == A::None);  // -> Write
    CHECK (w.action (60) == A::WriteSlot);
    CHECK (w.action (100) == A::None);
    CHECK (w.action (170) == A::None);  // settled -> ReadBack
    CHECK (w.action (180) == A::RequestSlot);
    w.gotPatch (4, 127, patch, 200);
    CHECK (w.step() == S::Done && w.finished());

    // No backup reply: nothing is written
    SlotWrite n (1, 0, patch, 1000, 100);
    n.action (0);
    for (double t = 0; t < 1200; t += 100)
        CHECK (n.action (t) != A::WriteSlot);
    CHECK (n.step() == S::Failed);

    // Backup could not be saved: nothing is written
    SlotWrite f (1, 0, patch, 1000, 100);
    f.action (0);
    f.gotPatch (1, 0, old, 10);
    f.backupStored (false);
    CHECK (f.action (20) == A::None);
    CHECK (f.step() == S::Failed);

    // Read-back differs
    SlotWrite d (1, 0, patch, 1000, 0);
    d.action (0);
    d.gotPatch (1, 0, old, 10);
    d.backupStored (true);
    d.action (20);
    CHECK (d.action (30) == A::WriteSlot);
    d.action (40);
    CHECK (d.action (50) == A::RequestSlot);
    d.gotPatch (1, 0, old, 60);
    CHECK (d.step() == S::Failed);
}


static void testModMap()
{
    using namespace peak::ui;
    const auto& dests = param ("mod1_dest").choices;
    const auto& fxDests = param ("fxmod1_dest").choices;
    // Every named knob exists, and the lists cover every destination.
    for (int d = 0; d < (int) dests.size(); ++d)
    {
        const auto id = paramForModDest (d);
        if (! id.empty() && indexOfId (id) < 0)
        {
            std::printf ("FAIL mod dest %d -> unknown %s\n", d, id.c_str());
            ++failures;
        }
        if (! id.empty())
            CHECK (modDestForParam (id) == d);
    }
    for (int d = 0; d < (int) fxDests.size(); ++d)
    {
        const auto id = paramForFxDest (d);
        if (! id.empty() && indexOfId (id) < 0)
        {
            std::printf ("FAIL fx dest %d -> unknown %s\n", d, id.c_str());
            ++failures;
        }
    }
    CHECK (paramForModDest ((int) dests.size()).empty());
    CHECK (paramForModDest (18) == "flt_freq");   // FiltFreq
    CHECK (paramForModDest (4) == "osc1_vsync");  // Osc1VSnc
    CHECK (paramForFxDest (5) == "dly_level");    // Del Lev

    CHECK (fxSourceForModSource (1) == 1);    // ModWheel
    CHECK (fxSourceForModSource (14) == 7);   // Animate1
    CHECK (fxSourceForModSource (8) == -1);   // Lfo1+/- can't reach FX
    CHECK (fxSourceForModSource (23) == 16);  // Noise

    // "Perc Up GEO": ModWheel -> FiltFreq +6, Animate2 -> Osc1VSnc +12
    const auto file = readFile (std::string (PEAK_TEST_DATA) + "/2026-11-13-editbuffer-perc-up-geo.syx");
    auto msg = parsePatchMessage (file.data(), file.size());
    CHECK (msg.has_value());
    if (! msg)
        return;
    auto v = readValues (msg->data, defaultValues()).values;
    CHECK (std::fabs (modAmountFor ("flt_freq", v) - 6.0 / 64) < 1e-9);
    CHECK (std::fabs (modAmountFor ("osc1_vsync", v) - 12.0 / 64) < 1e-9);
    CHECK (modAmountFor ("flt_res", v) == 0.0);
    CHECK (modAmountFor ("dly_level", v) > 0.0);  // FX slot 3: Animate1 -> Del Lev

    // Drop: first free slot is 5 (slots 1-4 are in use)
    auto t = slotForDrop (1, "flt_res", v);
    CHECK (! t.fx && t.slot == 5 && t.dest == 19 && t.source == 1);
    t = slotForDrop (1, "rev_time", v);
    CHECK (t.fx && t.slot == 4 && t.dest == 9);
    t = slotForDrop (8, "rev_time", v);  // LFO 1 onto an effect: no source
    CHECK (t.source < 0);
    t = slotForDrop (1, "arp_gate", v);  // not a destination
    CHECK (t.dest < 0);
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
    testPatchModel();
    testTiming();
    testLink();
    testLibrary();
    testLayout();
    testSync();
    testSlotWrite();
    testModMap();
    if (failures)
        std::printf ("%d FAILED\n", failures);
    else
        std::printf ("all passed (%zu parameters)\n", paramTable().size());
    return failures ? 1 : 0;
}
