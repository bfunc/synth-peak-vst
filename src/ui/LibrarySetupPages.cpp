#include "LibrarySetupPages.h"

#include "Theme.h"

using namespace peak;

namespace
{
enum Col { ColSlot = 1, ColName, ColCategory };

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

// ---- PresetsPage ----

PresetsPage::PresetsPage (PeakProcessor& p) : proc (p)
{
    for (int b = 1; b <= kBanks; ++b)
        bankBox.addItem (juce::String ("Bank ") + juce::String::charToString ((juce::juce_wchar) ('A' + b - 1)), b);
    bankBox.addItem ("All banks", 9);
    bankBox.setSelectedId (9, juce::dontSendNotification);
    categoryBox.addItem ("Any category", 1);
    const auto& cats = paramTable()[(size_t) indexOfId ("category")].choices;
    for (size_t i = 0; i < cats.size(); ++i)
        categoryBox.addItem (cats[i], (int) i + 2);
    categoryBox.setSelectedId (1, juce::dontSendNotification);
    search.setTextToShowWhenEmpty ("Search names", theme::ink3);
    search.setFont (theme::fontUi (13.0f, false));
    loadedOnly.setToggleState (true, juce::dontSendNotification);

    auto& h = table.getHeader();
    h.addColumn ("Slot", ColSlot, 70);
    h.addColumn ("Name", ColName, 260);
    h.addColumn ("Category", ColCategory, 140);
    h.setColour (juce::TableHeaderComponent::backgroundColourId, theme::panelHi);
    h.setColour (juce::TableHeaderComponent::textColourId, theme::ink2);
    table.setModel (this);
    table.setRowHeight (24);
    table.setColour (juce::ListBox::backgroundColourId, theme::panel);

    for (juce::Component* c : { (juce::Component*) &readSel, (juce::Component*) &readAll, (juce::Component*) &stop,
                                (juce::Component*) &load, (juce::Component*) &importBtn, (juce::Component*) &exportBtn,
                                (juce::Component*) &bankBox, (juce::Component*) &categoryBox, (juce::Component*) &search,
                                (juce::Component*) &loadedOnly, (juce::Component*) &table, (juce::Component*) &bar })
        addAndMakeVisible (c);

    readSel.setTooltip ("Read the selected bank from the Peak (read only, about 1 minute per bank)");
    readSel.onClick = [this]
    {
        const int b = bankBox.getSelectedId();
        proc.library().fetchBanks (b == 9 ? std::vector<int> { 1, 2, 3, 4 } : std::vector<int> { b });
    };
    readAll.onClick = [this] { proc.library().fetchBanks ({ 1, 2, 3, 4 }); };
    stop.onClick = [this] { proc.library().cancelFetch(); };
    load.onClick = [this]
    {
        if (auto* e = selectedEntry())
            proc.session().loadSlot (e->bank, e->slot);
    };
    importBtn.onClick = [this] { importFile(); };
    exportBtn.onClick = [this] { exportBank(); };
    bankBox.onChange = categoryBox.onChange = [this] { rebuild(); };
    search.onTextChange = [this] { rebuild(); };
    loadedOnly.onClick = [this] { rebuild(); };
    rebuild();
}

void PresetsPage::rebuild()
{
    const int bank = bankBox.getSelectedId();
    const auto cat = categoryBox.getSelectedId() > 1 ? categoryBox.getText().toStdString() : std::string();
    rows.clear();
    for (const auto* e : proc.library().library().filter (search.getText().toStdString(), cat, loadedOnly.getToggleState()))
        if (bank == 9 || e->bank == bank)
            rows.push_back (e);
    seenRev = proc.library().revision();
    table.updateContent();
    table.repaint();
}

void PresetsPage::refresh()
{
    auto& lib = proc.library();
    if (lib.revision() != seenRev)
        rebuild();
    progress = lib.fetching() ? lib.fetchProgress() : 0.0;
    bar.setVisible (lib.fetching());
    stop.setEnabled (lib.fetching());
    readSel.setEnabled (! lib.fetching());
    readAll.setEnabled (! lib.fetching());
    repaint (0, getHeight() - 24, getWidth(), 24);
}

const Library::Entry* PresetsPage::selectedEntry() const
{
    const int r = table.getSelectedRow();
    return r >= 0 && r < (int) rows.size() ? rows[(size_t) r] : nullptr;
}

int PresetsPage::getNumRows() { return (int) rows.size(); }

void PresetsPage::paintRowBackground (juce::Graphics& g, int row, int, int, bool selected)
{
    g.fillAll (selected ? theme::knobHi : (row % 2 ? theme::panel : theme::panel.brighter (0.03f)));
}

void PresetsPage::paintCell (juce::Graphics& g, int row, int col, int w, int h, bool)
{
    if (row < 0 || row >= (int) rows.size())
        return;
    const auto* e = rows[(size_t) row];
    const auto& o = proc.session().origin();
    const bool current = o.kind == PatchSession::Origin::Kind::Slot && o.bank == e->bank && o.slot == e->slot;
    juce::String text;
    switch (col)
    {
        case ColSlot: text = slotLabel (e->bank, e->slot); break;
        case ColName: text = e->loaded ? juce::String (e->name) : juce::String ("(not read)"); break;
        case ColCategory: text = e->category; break;
        default: break;
    }
    g.setColour (current ? theme::accent : (e->loaded ? theme::ink : theme::ink3));
    g.setFont (col == ColSlot ? theme::fontMono (12.0f) : theme::fontUi (13.5f, false));
    g.drawText (text, 6, 0, w - 8, h, juce::Justification::centredLeft);
}

void PresetsPage::cellDoubleClicked (int row, int, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) rows.size())
        proc.session().loadSlot (rows[(size_t) row]->bank, rows[(size_t) row]->slot);
}

void PresetsPage::importFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a .syx file", LibraryService::userFolder(), "*.syx");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile())
            proc.library().importFile (fc.getResult());
    });
}

void PresetsPage::exportBank()
{
    const int b = bankBox.getSelectedId();
    if (b < 1 || b > kBanks)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Export bank", "Pick one bank (A-D) first.");
        return;
    }
    const auto name = juce::String ("Peak bank ") + juce::String::charToString ((juce::juce_wchar) ('A' + b - 1)) + ".syx";
    chooser = std::make_unique<juce::FileChooser> ("Export bank", LibraryService::userFolder().getChildFile (name), "*.syx");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, b] (const juce::FileChooser& fc)
    {
        if (fc.getResult() != juce::File())
            proc.library().exportBank (fc.getResult().withFileExtension ("syx"), b);
    });
}

void PresetsPage::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    r.removeFromTop (28);
    auto row = r.removeFromTop (28);
    bankBox.setBounds (row.removeFromLeft (130).reduced (0, 2));
    row.removeFromLeft (8);
    readSel.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (6);
    readAll.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (6);
    stop.setBounds (row.removeFromLeft (64));
    row.removeFromLeft (12);
    bar.setBounds (row.removeFromLeft (200).reduced (0, 5));
    exportBtn.setBounds (row.removeFromRight (110));
    row.removeFromRight (6);
    importBtn.setBounds (row.removeFromRight (110));
    r.removeFromTop (10);
    auto f = r.removeFromTop (28);
    search.setBounds (f.removeFromLeft (260).reduced (0, 2));
    f.removeFromLeft (8);
    categoryBox.setBounds (f.removeFromLeft (170).reduced (0, 2));
    f.removeFromLeft (12);
    loadedOnly.setBounds (f.removeFromLeft (120));
    load.setBounds (f.removeFromRight (90));
    r.removeFromTop (8);
    r.removeFromBottom (24);
    table.setBounds (r);
}

void PresetsPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::chassis);
    sectionTitle (g, "Patches on the Peak", getLocalBounds().reduced (16, 12).removeFromTop (24));
    g.setColour (theme::ink2);
    g.setFont (theme::fontUi (12.0f, false));
    auto status = getLocalBounds().reduced (16, 0).removeFromBottom (24);
    g.drawText (proc.library().statusText().isNotEmpty() ? proc.library().statusText()
                                                          : juce::String (juce::String (proc.library().library().loadedCount())
                                                                          + " of 512 patches read. Double-click to load one (program change)."),
                status, juce::Justification::centredLeft);
}

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

    channelBox.onChange = [this] { proc.setChannel (channelBox.getSelectedId()); };
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
    lines = checklist (proc.link().status (proc.getSettings().channel));
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
    channelBox.setBounds (row().removeFromLeft (160));
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
