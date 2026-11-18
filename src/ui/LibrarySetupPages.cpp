#include "LibrarySetupPages.h"

#include "Theme.h"

using namespace peak;

namespace
{
constexpr int kLeftW = 440;
constexpr int kGap = 40;
constexpr int kVerdictH = 96;
constexpr int kMonitorH = 214;
constexpr size_t kMonitorKept = 200;

void sectionTitle (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r)
{
    g.setColour (theme::ink);
    g.setFont (theme::fontUi (13.0f));
    g.drawText (t.toUpperCase(), r, juce::Justification::centredLeft);
}

void label (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r)
{
    g.setColour (theme::ink3);
    g.setFont (theme::fontUi (11.0f));
    g.drawText (t.toUpperCase(), r, juce::Justification::centredLeft);
}

juce::Colour colourOf (Health h)
{
    switch (h)
    {
        case Health::Ok:      return theme::good;
        case Health::Warn:    return theme::led;
        case Health::Bad:     return theme::bad;
        case Health::Unknown: break;
    }
    return juce::Colour (0xff56616c);
}

void panelBox (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r.toFloat(), 5.0f);
    g.setColour (theme::edge);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);
}

juce::TextLayout layout (const juce::String& title, const juce::String& detail, const juce::String& fix, float width)
{
    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::byWord);
    as.append (title, theme::fontBody (13.5f).boldened(), theme::ink);
    if (detail.isNotEmpty())
        as.append ("\n" + detail, theme::fontBody (12.5f), theme::ink2);
    if (fix.isNotEmpty())
        as.append (juce::String (juce::CharPointer_UTF8 ("\n\xe2\x86\x92 ")) + fix, theme::fontBody (12.5f), juce::Colour (0xffffd2ad));
    juce::TextLayout tl;
    tl.createLayout (as, width);
    return tl;
}
} // namespace

// ---- SetupPage ----

SetupPage::SetupPage (PeakProcessor& p) : proc (p)
{
    for (int ch = 1; ch <= 16; ++ch)
        channelBox.addItem ("Channel " + juce::String (ch), ch);
    modeBox.addItem ("Straight to the Peak's port", 1);
    modeBox.addItem ("Through the DAW's MIDI out", 2);
    fillPorts();

    const auto s = proc.getSettings();
    channelBox.setSelectedId (s.channel, juce::dontSendNotification);
    modeBox.setSelectedId (s.hostOutput ? 2 : 1, juce::dontSendNotification);
    modeBox.setEnabled (! juce::JUCEApplicationBase::isStandaloneApp());
    notesThru.setToggleState (s.notesThru, juce::dontSendNotification);
    playheads.setToggleState (s.playheads, juce::dontSendNotification);
    follow.setToggleState (s.followChannel, juce::dontSendNotification);
    follow.setTooltip ("Take the MIDI channel the Peak sends on. The Peak's own channel setting can't be asked for, "
                       "so the plugin learns it from the first knob move.");
    follow.onClick = [this] { proc.setFollowChannel (follow.getToggleState()); };

    channelBox.onChange = [this]
    {
        proc.setChannel (channelBox.getSelectedId());
        proc.setFollowChannel (false);  // a channel picked by hand stays
        follow.setToggleState (false, juce::dontSendNotification);
    };
    modeBox.onChange = [this] { proc.setHostOutput (modeBox.getSelectedId() == 2); fillPorts(); };
    auto portsChanged = [this]
    {
        proc.setPorts (inBox.getSelectedId() > 1 ? inBox.getText() : juce::String(),
                       outBox.getSelectedId() > 1 ? outBox.getText() : juce::String());
    };
    inBox.onChange = portsChanged;
    outBox.onChange = portsChanged;
    notesThru.onClick = [this] { proc.setNotesThru (notesThru.getToggleState()); };
    playheads.onClick = [this] { proc.setPlayheads (playheads.getToggleState()); };
    check.setTooltip ("Ask the Peak who it is again (SysEx). Changes nothing on the Peak.");
    check.onClick = [this] { proc.link().probe(); };
    sendAll.setTooltip ("Send every parameter to the Peak again (live ones as CC / NRPN)");
    sendAll.onClick = [this] { proc.sendAllParameters(); };
    rescan.onClick = [this] { fillPorts(); };
    testNote.setTooltip ("Play C3 on the plugin's channel for half a second, then say whether you heard it. "
                         "Tells \"nothing reaches the Peak\" from \"the Peak's replies don't come back\".");
    testNote.onClick = [this] { proc.link().playTestNote (proc.midiChannel()); };

    clearMonitor.onClick = [this]
    {
        monitorLines.clear();
        layoutMonitor (0);
        repaint (monitorArea);
    };
    copyMonitor.setTooltip ("Copy every line of the monitor, to paste into a message or a note.");
    copyMonitor.onClick = [this]
    {
        juce::StringArray all;
        for (const auto& l : monitorLines)
            all.add (MonitorList::text (l));
        juce::SystemClipboard::copyTextToClipboard (all.joinIntoString ("\n"));
    };
    monitorView.setViewedComponent (&monitorList, false);
    monitorView.setScrollBarsShown (true, false);
    monitorView.setScrollBarThickness (10);
    addAndMakeVisible (monitorView);

    for (size_t i = 0; i < verdictButtons.size(); ++i)
    {
        verdictButtons[i].setClickingTogglesState (false);
        verdictButtons[i].onClick = [this, i]
        {
            if (verdictActions[i])
                verdictActions[i]();
        };
        addChildComponent (verdictButtons[i]);
    }

    for (juce::Component* c : { (juce::Component*) &inBox, (juce::Component*) &outBox, (juce::Component*) &channelBox,
                                (juce::Component*) &modeBox, (juce::Component*) &follow, (juce::Component*) &notesThru,
                                (juce::Component*) &playheads, (juce::Component*) &check, (juce::Component*) &sendAll,
                                (juce::Component*) &rescan, (juce::Component*) &testNote, (juce::Component*) &clearMonitor,
                                (juce::Component*) &copyMonitor })
        addAndMakeVisible (c);
}

void SetupPage::fillPorts()
{
    const auto s = proc.getSettings();
    auto fill = [] (juce::ComboBox& box, const juce::StringArray& names, const juce::String& chosen)
    {
        box.clear (juce::dontSendNotification);
        box.addItem ("Automatic (\"Peak\")", 1);
        for (int i = 0; i < names.size(); ++i)
            box.addItem (names[i], i + 2);
        const int at = names.indexOf (chosen);
        box.setSelectedId (chosen.isEmpty() || at < 0 ? 1 : at + 2, juce::dontSendNotification);
    };
    fill (inBox, PeakLink::inputNames(), s.inPort);
    fill (outBox, PeakLink::outputNames(), s.outPort);
    outBox.setEnabled (! s.hostOutput);
}

void SetupPage::refresh()
{
    const auto settings = proc.getSettings();
    if (channelBox.getSelectedId() != settings.channel)
        channelBox.setSelectedId (settings.channel, juce::dontSendNotification);
    if (follow.getToggleState() != settings.followChannel)
        follow.setToggleState (settings.followChannel, juce::dontSendNotification);

    const auto st = proc.linkStatus();
    testNote.setEnabled (st.hostOutput || st.outOpen);

    report = diagnose (st);
    juce::String key;
    key << (int) report.verdictHealth << report.verdict << report.verdictText << report.useChannel;
    for (const auto& a : report.actions)
        key << "a" << (int) a;
    for (const auto* list : { &report.steps, &report.notes })
        for (const auto& i : *list)
            key << (int) i.health << (int) i.blocked << i.title << i.detail << i.fix << "\n";
    if (key != reportKey)
    {
        reportKey = key;
        updateVerdictButtons();
        repaint (verdictArea);
        repaint (stepsArea);
    }

    std::vector<PeakLink::MonitorLine> fresh;
    monitorSeen = proc.link().readMonitor (monitorSeen, fresh);
    if (! fresh.empty())
    {
        for (auto& l : fresh)
            monitorLines.push_front (std::move (l));
        while (monitorLines.size() > kMonitorKept)
            monitorLines.pop_back();
        layoutMonitor ((int) fresh.size());
    }
}

// Newest lines go on top. Scrolled down to read older ones: keep the same lines in view; at the top: follow live.
void SetupPage::layoutMonitor (int added)
{
    const int y = monitorView.getViewPositionY();
    monitorList.setSize (juce::jmax (1, monitorView.getMaximumVisibleWidth()),
                         juce::jmax (monitorView.getHeight(), 12 + (int) monitorLines.size() * MonitorList::kRowH));
    if (y > 0)
        monitorView.setViewPosition (0, y + added * MonitorList::kRowH);
    monitorList.repaint();
}

void SetupPage::updateVerdictButtons()
{
    for (size_t i = 0; i < verdictButtons.size(); ++i)
    {
        verdictButtons[i].setVisible (false);
        verdictButtons[i].setToggleState (false, juce::dontSendNotification);
        verdictActions[i] = nullptr;
    }
    size_t n = 0;
    auto add = [&] (const juce::String& text, bool primary, std::function<void()> fn)
    {
        if (n >= verdictButtons.size())
            return;
        verdictButtons[n].setButtonText (text);
        verdictButtons[n].setToggleState (primary, juce::dontSendNotification);  // lit = the suggested one
        verdictButtons[n].setVisible (true);
        verdictActions[n] = std::move (fn);
        ++n;
    };
    for (const auto a : report.actions)
        switch (a)
        {
            case Action::CheckAgain:
                add ("Check again", n == 0, [this] { proc.link().probe(); });
                break;
            case Action::PlayTestNote:
                add ("Play test note", n == 0, [this] { proc.link().playTestNote (proc.midiChannel()); });
                break;
            case Action::UseChannel:
            {
                const int ch = report.useChannel;
                add ("Use channel " + juce::String (ch), true, [this, ch]
                {
                    proc.setChannel (ch);
                    proc.setFollowChannel (true);
                    channelBox.setSelectedId (ch, juce::dontSendNotification);
                    follow.setToggleState (true, juce::dontSendNotification);
                });
                break;
            }
            case Action::AnswerTestNote:
                add ("I heard it", true, [this] { proc.link().answerTestNote (true); });
                add ("No sound", false, [this] { proc.link().answerTestNote (false); });
                break;
        }
    resized();
}

void SetupPage::resized()
{
    auto r = getLocalBounds().reduced (24, 16);
    auto left = r.removeFromLeft (kLeftW);
    left.removeFromTop (36);
    auto row = [&left] { auto x = left.removeFromTop (30); left.removeFromTop (8); return x.withTrimmedLeft (130); };
    inBox.setBounds (row());
    outBox.setBounds (row());
    rescan.setBounds (row().removeFromLeft (120));
    modeBox.setBounds (row());
    {
        auto chRow = row();
        channelBox.setBounds (chRow.removeFromLeft (120));
        chRow.removeFromLeft (12);
        follow.setBounds (chRow);
    }
    left.removeFromTop (16);
    notesThru.setBounds (left.removeFromTop (26).withTrimmedLeft (130));
    playheads.setBounds (left.removeFromTop (26).withTrimmedLeft (130));
    left.removeFromTop (16);
    {
        auto b = left.removeFromTop (28).withTrimmedLeft (130);
        check.setBounds (b.removeFromLeft (120));
        b.removeFromLeft (8);
        sendAll.setBounds (b.removeFromLeft (170));
    }
    left.removeFromTop (8);
    testNote.setBounds (left.removeFromTop (28).withTrimmedLeft (130).removeFromLeft (120));

    // Right: status title, verdict card, steps, monitor
    r.removeFromLeft (kGap);
    r.removeFromTop (36);
    verdictArea = r.removeFromTop (kVerdictH);
    r.removeFromTop (12);
    monitorArea = r.removeFromBottom (kMonitorH);
    r.removeFromBottom (12);
    stepsArea = r;

    auto buttons = verdictArea.reduced (16, 0).removeFromRight (2 * 124 + 8);
    for (int i = (int) verdictButtons.size() - 1; i >= 0; --i)
        if (verdictButtons[(size_t) i].isVisible())
        {
            verdictButtons[(size_t) i].setBounds (buttons.removeFromRight (124).withSizeKeepingCentre (124, 28));
            buttons.removeFromRight (8);
        }
    auto monBar = monitorArea.withHeight (32);
    clearMonitor.setBounds (monBar.removeFromRight (80).reduced (10, 5));
    copyMonitor.setBounds (monBar.removeFromRight (70).reduced (4, 5).translated (8, 0));
    monitorView.setBounds (monitorArea.withTrimmedTop (32).reduced (1, 0).withTrimmedBottom (1));
    layoutMonitor (0);
}

void SetupPage::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (chassis);
    auto r = getLocalBounds().reduced (24, 16);
    auto left = r.removeFromLeft (kLeftW);
    sectionTitle (g, "Connection", left.removeFromTop (28));
    left.removeFromTop (8);
    for (const char* t : { "MIDI input", "MIDI output", "", "Output mode", "MIDI channel" })
    {
        label (g, t, left.removeFromTop (30).withWidth (124));
        left.removeFromTop (8);
    }
    left.removeFromTop (16);
    label (g, "Options", left.removeFromTop (26).withWidth (124));
    left.removeFromTop (26 + 16);
    label (g, "Actions", left.removeFromTop (28).withWidth (124));
    left.removeFromTop (8);
    {
        auto t = left.removeFromTop (28);
        label (g, "Test", t.removeFromLeft (124));
        g.setColour (ink3);
        g.setFont (fontBody (12.0f));
        g.drawText ("C3, half a second", t.withTrimmedLeft (6 + 120 + 10), juce::Justification::centredLeft);
    }
    left.removeFromTop (16);
    g.setFont (fontBody (12.0f));
    g.setColour (ink3);
    g.drawFittedText ("On the Peak: Settings > CC/NRPN = Rec+Trans, and close Novation Components so the port is free. "
                      "The MIDI channel matters for knob moves, notes and program changes; SysEx (Check, patch reading, "
                      "saving) has no channel. The playhead dot is an estimate from the DAW position (Sync range only).",
                      left.removeFromTop (96).withTrimmedLeft (130), juce::Justification::topLeft, 7);

    r.removeFromLeft (kGap);
    sectionTitle (g, "Status", r.removeFromTop (28));
    paintVerdict (g);
    paintSteps (g);
    paintMonitor (g);
}

void SetupPage::paintVerdict (juce::Graphics& g)
{
    using namespace theme;
    panelBox (g, verdictArea);
    auto card = verdictArea;
    g.setColour (colourOf (report.verdictHealth));
    g.fillRoundedRectangle (card.removeFromLeft (6).toFloat().reduced (0, 1), 3.0f);

    int buttonsW = 0;
    for (auto& b : verdictButtons)
        if (b.isVisible())
            buttonsW += 124 + 8;
    auto text = card.reduced (16, 12).withTrimmedRight (buttonsW);

    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::byWord);
    as.append (report.verdict, fontUi (20.0f), ink);
    as.append ("\n" + juce::String (report.verdictText), fontBody (13.0f), ink2);
    juce::TextLayout tl;
    tl.createLayout (as, (float) text.getWidth());
    const float h = juce::jmin ((float) text.getHeight(), tl.getHeight());
    tl.draw (g, text.toFloat().withSizeKeepingCentre ((float) text.getWidth(), h));
}

void SetupPage::paintSteps (juce::Graphics& g)
{
    using namespace theme;
    panelBox (g, stepsArea);
    auto area = stepsArea.reduced (16, 10);
    auto stepsCol = area.removeFromLeft ((int) (area.getWidth() * 0.56f));
    area.removeFromLeft (24);
    auto notesCol = area;

    auto column = [&g] (juce::Rectangle<int> col, const char* heading, const std::vector<CheckItem>& items, bool numbered)
    {
        g.setColour (ink3);
        g.setFont (fontUi (11.5f));
        g.drawText (juce::String (heading).toUpperCase(), col.removeFromTop (20), juce::Justification::centredLeft);
        bool fixShown = false;
        int n = 0;
        for (const auto& item : items)
        {
            ++n;
            // Only the first failing step shows its fix: the later ones may sort themselves out.
            const bool showFix = ! item.fix.empty() && ! item.blocked && (! numbered || ! fixShown);
            if (showFix && numbered)
                fixShown = true;
            juce::String title = (numbered ? juce::String (n) + ". " : juce::String()) + juce::String (item.title);
            if (item.optional && numbered)
                title << "  (optional)";
            const float textX = (float) col.getX() + 20.0f;
            const float textW = (float) col.getWidth() - 20.0f;
            auto tl = layout (title, item.detail, showFix ? juce::String (item.fix) : juce::String(), textW);
            const float h = tl.getHeight();
            if ((float) col.getHeight() < h)
                return;
            if (item.blocked)
                g.beginTransparencyLayer (0.4f);
            g.setColour (colourOf (item.health));
            g.fillEllipse ((float) col.getX() + 1.0f, (float) col.getY() + 4.0f, 9.0f, 9.0f);
            tl.draw (g, { textX, (float) col.getY(), textW, h });
            if (item.blocked)
                g.endTransparencyLayer();
            col.removeFromTop ((int) std::ceil (h) + 9);
        }
    };
    column (stepsCol, "Steps, in order", report.steps, true);
    column (notesCol, "Also note", report.notes, false);
}

void SetupPage::paintMonitor (juce::Graphics& g)
{
    using namespace theme;
    panelBox (g, monitorArea);
    auto bar = monitorArea.withHeight (32).withTrimmedRight (150).reduced (16, 0);
    g.setColour (ink);
    g.setFont (fontUi (13.0f));
    g.drawText ("FROM THE PEAK", bar, juce::Justification::centredLeft);
    g.setColour (ink3);
    g.setFont (fontBody (12.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("newest first \xc2\xb7 scroll for older lines \xc2\xb7 turn a knob on the Peak")),
                bar.withTrimmedLeft (120), juce::Justification::centredLeft, true);
}

juce::String MonitorList::text (const PeakLink::MonitorLine& l)
{
    return juce::Time (l.timeMs).formatted ("%H:%M:%S") + "  " + (l.channel > 0 ? "ch " + juce::String (l.channel) : juce::String ("--"))
         + "  " + l.what + "  " + l.name;
}

void MonitorList::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (well);
    auto lines = getLocalBounds().reduced (16, 6);
    if (lines.isEmpty())
        return;
    if (this->lines.empty())
    {
        g.setColour (ink3);
        g.setFont (fontBody (13.0f));
        g.drawText ("Nothing from the Peak yet.", lines.removeFromTop (22), juce::Justification::centredLeft);
        return;
    }
    const int pluginCh = channel ? channel() : 0;
    const auto clip = g.getClipBounds();
    g.setFont (fontMono (12.0f));
    for (const auto& l : this->lines)
    {
        auto row = lines.removeFromTop (kRowH);
        if (row.getBottom() < clip.getY())
            continue;
        if (row.getY() > clip.getBottom())
            break;
        const bool otherCh = l.channel > 0 && l.channel != pluginCh;
        g.setColour (ink3);
        g.drawText (juce::Time (l.timeMs).formatted ("%H:%M:%S"), row.removeFromLeft (78), juce::Justification::centredLeft);
        g.setColour (otherCh ? led : ink2);
        g.drawText (l.channel > 0 ? "ch " + juce::String (l.channel) : juce::String ("--"), row.removeFromLeft (52),
                    juce::Justification::centredLeft);
        g.setColour (ink2);
        g.drawText (l.what, row.removeFromLeft (170), juce::Justification::centredLeft, true);
        g.setColour (otherCh ? led : ink);
        g.drawText (l.name, row, juce::Justification::centredLeft, true);
    }
}
