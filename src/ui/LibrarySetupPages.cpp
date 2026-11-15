#include "LibrarySetupPages.h"

#include "Theme.h"

using namespace peak;

namespace
{

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
    follow.onClick = [this] { proc.setFollowChannel (follow.getToggleState()); };
    addAndMakeVisible (follow);

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
    check.onClick = [this] { proc.link().probe(); };
    sendAll.setTooltip ("Send every parameter to the Peak again (live ones as CC / NRPN)");
    sendAll.onClick = [this] { proc.sendAllParameters(); };
    rescan.onClick = [this] { fillPorts(); };

    for (juce::Component* c : { (juce::Component*) &inBox, (juce::Component*) &outBox, (juce::Component*) &channelBox,
                                (juce::Component*) &modeBox, (juce::Component*) &notesThru, (juce::Component*) &playheads,
                                (juce::Component*) &check, (juce::Component*) &sendAll, (juce::Component*) &rescan })
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
    const int ch = proc.getSettings().channel;
    if (channelBox.getSelectedId() != ch)
        channelBox.setSelectedId (ch, juce::dontSendNotification);
    lines = checklist (proc.link().status (ch));
    juce::String key;
    for (const auto& l : lines)
        key << (int) l.level << l.text << "\n";
    if (key != linesKey)
    {
        linesKey = key;
        repaint();
    }
}

void SetupPage::resized()
{
    auto r = getLocalBounds().reduced (24, 16);
    auto left = r.removeFromLeft (440);
    left.removeFromTop (36);
    auto row = [&left] { auto x = left.removeFromTop (30); left.removeFromTop (8); return x.withTrimmedLeft (130); };
    inBox.setBounds (row());
    outBox.setBounds (row());
    rescan.setBounds (row().removeFromLeft (120));
    modeBox.setBounds (row());
    {
        auto chRow = row();
        channelBox.setBounds (chRow.removeFromLeft (160));
        chRow.removeFromLeft (12);
        follow.setBounds (chRow);
    }
    left.removeFromTop (16);
    notesThru.setBounds (left.removeFromTop (26).withTrimmedLeft (130));
    playheads.setBounds (left.removeFromTop (26).withTrimmedLeft (130));
    left.removeFromTop (16);
    auto b = left.removeFromTop (28).withTrimmedLeft (130);
    check.setBounds (b.removeFromLeft (120));
    b.removeFromLeft (8);
    sendAll.setBounds (b.removeFromLeft (170));
}

void SetupPage::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (chassis);
    auto r = getLocalBounds().reduced (24, 16);
    auto left = r.removeFromLeft (440);
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
    left.removeFromTop (16);
    g.setFont (fontUi (12.0f, false));
    g.setColour (ink3);
    g.drawFittedText ("On the Peak: Settings > CC/NRPN = Rec+Trans, the same MIDI channel, and close Novation Components "
                      "so the port is free. The playhead dot is an estimate from the DAW position (Sync range only).",
                      left.removeFromTop (80), juce::Justification::topLeft, 5);

    r.removeFromLeft (40);
    sectionTitle (g, "Checklist", r.removeFromTop (28));
    r.removeFromTop (8);
    for (const auto& l : lines)
    {
        auto line = r.removeFromTop (26);
        const auto col = l.level == CheckLine::Level::Ok ? good
                       : l.level == CheckLine::Level::Bad ? bad
                       : l.level == CheckLine::Level::Warn ? led : ink3;
        g.setColour (col);
        g.fillEllipse (line.removeFromLeft (18).withSizeKeepingCentre (8, 8).toFloat());
        g.setColour (ink);
        g.setFont (fontUi (13.0f, false));
        g.drawFittedText (l.text, line, juce::Justification::centredLeft, 2);
    }
}
