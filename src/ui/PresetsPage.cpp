#include "PresetsPage.h"

#include "Theme.h"

using namespace peak;

namespace
{
enum Col { ColSlot = 1, ColName, ColCategory };
constexpr int kAllBanks = 9, kSingles = 10;

juce::String bankLetter (int b) { return juce::String::charToString ((juce::juce_wchar) ('A' + juce::jlimit (1, 4, b) - 1)); }

// A small dialog with drop-downs and a warning, for the writes. Calls back with the chosen ids on OK.
void askWithBoxes (const juce::String& title, const juce::String& text, const juce::String& okText,
                   std::vector<std::pair<juce::String, std::vector<std::pair<juce::String, int>>>> boxes,
                   std::vector<int> defaults, std::function<void (std::vector<int>)> onOk)
{
    auto* w = new juce::AlertWindow (title, text, juce::MessageBoxIconType::WarningIcon);
    for (size_t i = 0; i < boxes.size(); ++i)
    {
        juce::StringArray items;
        for (auto& it : boxes[i].second)
            items.add (it.first);
        int sel = 0;
        for (size_t k = 0; k < boxes[i].second.size(); ++k)
            if (boxes[i].second[k].second == defaults[i])
                sel = (int) k;
        w->addComboBox ("box" + juce::String ((int) i), items, boxes[i].first);
        w->getComboBoxComponent ("box" + juce::String ((int) i))->setSelectedItemIndex (sel, juce::dontSendNotification);
    }
    w->addButton (okText, 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, boxes, onOk] (int r)
    {
        if (r != 1)
            return;
        std::vector<int> ids;
        for (size_t i = 0; i < boxes.size(); ++i)
        {
            const int k = w->getComboBoxComponent ("box" + juce::String ((int) i))->getSelectedItemIndex();
            ids.push_back (boxes[i].second[(size_t) juce::jmax (0, k)].second);
        }
        onOk (ids);
    }), true);
}

std::vector<std::pair<juce::String, int>> bankChoices()
{
    return { { "A", 1 }, { "B", 2 }, { "C", 3 }, { "D", 4 } };
}
} // namespace

PresetsPage::PresetsPage (PeakProcessor& p) : proc (p)
{
    for (auto* t : { &peakTab, &fileTab })
    {
        t->getProperties().set ("segment", true);
        addAndMakeVisible (t);
    }
    peakTab.onClick = [this] { setSource (Source::Peak); };
    fileTab.onClick = [this] { setSource (Source::File); };

    for (int b = 1; b <= kBanks; ++b)
        bankBox.addItem ("Bank " + bankLetter (b), b);
    bankBox.addItem ("All banks", kAllBanks);
    bankBox.setSelectedId (kAllBanks, juce::dontSendNotification);
    categoryBox.addItem ("Any category", 1);
    const auto& cats = paramTable()[(size_t) indexOfId ("category")].choices;
    for (size_t i = 0; i < cats.size(); ++i)
        categoryBox.addItem (cats[i], (int) i + 2);
    categoryBox.setSelectedId (1, juce::dontSendNotification);
    search.setTextToShowWhenEmpty ("Search names", theme::ink3);
    search.setFont (theme::fontUi (13.0f, false));
    hideUnread.setToggleState (true, juce::dontSendNotification);

    auto& h = table.getHeader();
    h.addColumn ("Slot", ColSlot, 80);
    h.addColumn ("Name", ColName, 260);
    h.addColumn ("Category", ColCategory, 140);
    h.setColour (juce::TableHeaderComponent::backgroundColourId, theme::panelHi);
    h.setColour (juce::TableHeaderComponent::textColourId, theme::ink2);
    table.setModel (this);
    table.setRowHeight (24);
    table.setColour (juce::ListBox::backgroundColourId, theme::panel);

    readBank.setTooltip ("Read the selected bank from the Peak (read only) and save a backup .syx");
    readAll.setTooltip ("Read all four banks from the Peak (read only, about 1 minute) and save a backup .syx");
    saveFile.setTooltip ("Save the shown bank (or all banks) to a .syx file");
    openBtn.setTooltip ("Open a .syx: one patch, a bank or a full backup");
    copyBtn.setTooltip ("Write the selected patch into a slot on the Peak (the slot is backed up first)");
    sendBtn.setTooltip ("Write a whole bank from this file into the Peak (the bank is backed up first)");

    readBank.onClick = [this]
    {
        const int b = bankBox.getSelectedId();
        proc.library().readBanks (b == kAllBanks ? std::vector<int> { 1, 2, 3, 4 } : std::vector<int> { b });
    };
    readAll.onClick = [this] { proc.library().readBanks ({ 1, 2, 3, 4 }); };
    saveFile.onClick = [this] { saveToFile(); };
    openBtn.onClick = [this] { openFile(); };
    copyBtn.onClick = [this] { copyToPeak(); };
    sendBtn.onClick = [this] { sendBank(); };
    stop.onClick = [this] { proc.library().cancel(); };
    loadBtn.onClick = [this] { loadSelected(); };
    bankBox.onChange = fileBankBox.onChange = categoryBox.onChange = [this] { rebuild(); };
    search.onTextChange = [this] { rebuild(); };
    hideUnread.onClick = [this] { rebuild(); };

    for (juce::Component* c : { (juce::Component*) &bankBox, (juce::Component*) &readBank, (juce::Component*) &readAll,
                                (juce::Component*) &saveFile, (juce::Component*) &openBtn, (juce::Component*) &copyBtn,
                                (juce::Component*) &sendBtn, (juce::Component*) &fileBankBox, (juce::Component*) &stop,
                                (juce::Component*) &loadBtn, (juce::Component*) &categoryBox, (juce::Component*) &search,
                                (juce::Component*) &hideUnread, (juce::Component*) &table, (juce::Component*) &bar })
        addAndMakeVisible (c);
    setSource (Source::Peak);
}

void PresetsPage::setSource (Source s)
{
    source = s;
    peakTab.setToggleState (s == Source::Peak, juce::dontSendNotification);
    fileTab.setToggleState (s == Source::File, juce::dontSendNotification);
    const bool peak = s == Source::Peak;
    for (juce::Component* c : { (juce::Component*) &bankBox, (juce::Component*) &readBank, (juce::Component*) &readAll,
                                (juce::Component*) &saveFile, (juce::Component*) &hideUnread })
        c->setVisible (peak);
    for (juce::Component* c : { (juce::Component*) &openBtn, (juce::Component*) &copyBtn, (juce::Component*) &sendBtn,
                                (juce::Component*) &fileBankBox })
        c->setVisible (! peak);
    loadBtn.setButtonText (peak ? "Load" : "Try");
    loadBtn.setTooltip (peak ? "Switch the Peak to this patch (program change)"
                             : "Play this patch through the edit buffer; the Peak's memory is not changed");
    table.getHeader().setColumnName (ColSlot, peak ? "Slot" : "From");
    table.deselectAllRows();
    rebuild();
    repaint();
}

void PresetsPage::rebuild()
{
    auto& lib = proc.library();
    const auto text = search.getText().toLowerCase();
    const auto cat = categoryBox.getSelectedId() > 1 ? categoryBox.getText() : juce::String();
    auto keep = [&] (const Library::Entry& e)
    {
        if (text.isNotEmpty() && ! juce::String (e.name).toLowerCase().contains (text))
            return false;
        return cat.isEmpty() || juce::String (e.category) == cat;
    };
    rows.clear();
    if (source == Source::Peak)
    {
        const int bank = bankBox.getSelectedId();
        for (const auto* e : lib.library().filter ("", "", hideUnread.getToggleState()))
            if ((bank == kAllBanks || e->bank == bank) && keep (*e))
                rows.push_back (e->bank * kSlots + e->slot);
    }
    else
    {
        // The file's bank choices follow what is in the file.
        const auto banks = lib.fileBanks();
        const int sel = fileBankBox.getSelectedId();
        fileBankBox.clear (juce::dontSendNotification);
        fileBankBox.addItem ("Everything", kAllBanks);
        for (int b : banks)
            fileBankBox.addItem ("Bank " + bankLetter (b), b);
        bool singles = false;
        for (const auto& e : lib.file())
            singles |= e.bank == 0;
        if (singles)
            fileBankBox.addItem ("Single patches", kSingles);
        fileBankBox.setSelectedId (fileBankBox.indexOfItemId (sel) >= 0 ? sel : kAllBanks, juce::dontSendNotification);
        const int show = fileBankBox.getSelectedId();
        const auto& f = lib.file();
        for (size_t i = 0; i < f.size(); ++i)
        {
            const bool inBank = show == kAllBanks || (show == kSingles ? f[i].bank == 0 : f[i].bank == show);
            if (inBank && keep (f[i]))
                rows.push_back ((int) i);
        }
    }
    seenRev = lib.revision();
    table.updateContent();
    table.repaint();
    updateButtons();
}

int PresetsPage::selectedRow() const
{
    const int r = table.getSelectedRow();
    return r >= 0 && r < (int) rows.size() ? r : -1;
}

void PresetsPage::updateButtons()
{
    auto& lib = proc.library();
    const bool busy = lib.busy();
    const bool sel = selectedRow() >= 0;
    readBank.setEnabled (! busy);
    readAll.setEnabled (! busy);
    saveFile.setEnabled (! busy && lib.library().loadedCount() > 0);
    openBtn.setEnabled (! busy);
    copyBtn.setEnabled (! busy && sel && source == Source::File);
    sendBtn.setEnabled (! busy && ! lib.fileBanks().empty());
    loadBtn.setEnabled (! busy && sel);
    stop.setVisible (busy);
    bar.setVisible (busy && lib.progress() >= 0);
    fileTab.setButtonText (lib.hasFile() ? "File: " + lib.fileName() : juce::String ("File"));
}

void PresetsPage::refresh()
{
    auto& lib = proc.library();
    if (lib.revision() != seenRev)
        rebuild();
    const auto origin = proc.session().originText();
    if (origin != seenOrigin)
    {
        seenOrigin = origin;
        table.repaint();  // only the current patch is highlighted
    }
    progress = juce::jmax (0.0f, lib.progress());
    if (lib.busy() != wasBusy)
    {
        wasBusy = lib.busy();
        updateButtons();
    }
    repaint (0, getHeight() - 26, getWidth(), 26);
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
    const Library::Entry* e = nullptr;
    if (source == Source::Peak)
        e = &proc.library().library().at (rows[(size_t) row] / kSlots, rows[(size_t) row] % kSlots);
    else if ((size_t) rows[(size_t) row] < proc.library().file().size())
        e = &proc.library().file()[(size_t) rows[(size_t) row]];
    if (e == nullptr)
        return;
    const auto& o = proc.session().origin();
    const bool current = source == Source::Peak && o.kind == PatchSession::Origin::Kind::Slot && o.bank == e->bank
                      && o.slot == e->slot;
    juce::String text;
    switch (col)
    {
        case ColSlot: text = e->bank >= 1 ? juce::String (slotLabel (e->bank, e->slot)) : "patch " + juce::String (e->slot + 1); break;
        case ColName: text = e->loaded ? juce::String (e->name) : juce::String ("(not read)"); break;
        case ColCategory: text = e->category; break;
        default: break;
    }
    g.setColour (current ? theme::accent : (e->loaded ? theme::ink : theme::ink3));
    g.setFont (col == ColSlot ? theme::fontMono (12.0f) : theme::fontUi (13.5f, false));
    g.drawText (text, 6, 0, w - 8, h, juce::Justification::centredLeft);
}

void PresetsPage::cellDoubleClicked (int, int, const juce::MouseEvent&) { loadSelected(); }

void PresetsPage::loadSelected()
{
    const int r = selectedRow();
    if (r < 0 || proc.library().busy())
        return;
    if (source == Source::Peak)
        proc.session().loadSlot (rows[(size_t) r] / kSlots, rows[(size_t) r] % kSlots);
    else
        proc.library().tryPatch ((size_t) rows[(size_t) r]);
}

void PresetsPage::saveToFile()
{
    const int b = bankBox.getSelectedId();
    const auto banks = b == kAllBanks ? std::vector<int> { 1, 2, 3, 4 } : std::vector<int> { b };
    const auto name = juce::String ("Peak ") + (b == kAllBanks ? juce::String ("A-D") : bankLetter (b)) + ".syx";
    chooser = std::make_unique<juce::FileChooser> ("Save to file", LibraryService::userFolder().getChildFile (name), "*.syx");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, banks] (const juce::FileChooser& fc)
    {
        if (fc.getResult() != juce::File())
            proc.library().saveToFile (fc.getResult().withFileExtension ("syx"), banks);
    });
}

void PresetsPage::openFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a .syx file", LibraryService::backupFolder().exists()
                                                                           ? LibraryService::backupFolder()
                                                                           : LibraryService::userFolder(), "*.syx");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile() && proc.library().openFile (fc.getResult()))
            setSource (Source::File);
    });
}

void PresetsPage::copyToPeak()
{
    const int r = selectedRow();
    if (r < 0)
        return;
    const auto index = (size_t) rows[(size_t) r];
    const auto& e = proc.library().file()[index];
    std::vector<std::pair<juce::String, int>> slots;
    for (int s = 0; s < kSlots; ++s)
        slots.push_back ({ juce::String (s).paddedLeft ('0', 3), s });
    askWithBoxes ("Copy to Peak", "Write \"" + juce::String (e.name) + "\" into a slot on the Peak.\n"
                                  "The slot's current patch is saved to Backups first.",
                  "Copy", { { "Bank", bankChoices() }, { "Slot", slots } },
                  { e.bank >= 1 ? e.bank : 1, e.bank >= 1 ? e.slot : 0 },
                  [this, index] (std::vector<int> ids) { proc.library().copyToPeak (index, ids[0], ids[1]); });
}

void PresetsPage::sendBank()
{
    const auto banks = proc.library().fileBanks();
    if (banks.empty())
        return;
    std::vector<std::pair<juce::String, int>> from;
    for (int b : banks)
        from.push_back ({ "File bank " + bankLetter (b), b });
    const int sel = fileBankBox.getSelectedId();
    const int defFrom = sel >= 1 && sel <= 4 ? sel : banks.front();
    askWithBoxes ("Send bank to Peak",
                  "This overwrites up to 128 patches in the chosen Peak bank with the patches from " + proc.library().fileName()
                      + ".\nThe Peak bank is read and saved to Backups first, then written, then read back and checked.",
                  "Send bank", { { "From", from }, { "To Peak bank", bankChoices() } }, { defFrom, defFrom },
                  [this] (std::vector<int> ids)
    {
        const int fromBank = ids[0], toBank = ids[1];
        juce::AlertWindow::showOkCancelBox (
            juce::MessageBoxIconType::WarningIcon, "Overwrite bank " + bankLetter (toBank) + "?",
            "Last check: bank " + bankLetter (toBank) + " on the Peak will be replaced by file bank " + bankLetter (fromBank) + ".",
            "Overwrite", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([this, fromBank, toBank] (int ok)
            {
                if (ok == 1)
                {
                    proc.library().sendBank (fromBank, toBank);
                    setSource (Source::Peak);
                    bankBox.setSelectedId (toBank);
                }
            }));
    });
}

void PresetsPage::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    auto top = r.removeFromTop (30);
    peakTab.setBounds (top.removeFromLeft (90));
    fileTab.setBounds (top.removeFromLeft (220));
    top.removeFromLeft (16);
    // Peak actions
    {
        auto a = top;
        bankBox.setBounds (a.removeFromLeft (120).reduced (0, 2));
        a.removeFromLeft (8);
        readBank.setBounds (a.removeFromLeft (100));
        a.removeFromLeft (6);
        readAll.setBounds (a.removeFromLeft (90));
        saveFile.setBounds (a.removeFromRight (120));
    }
    // File actions
    {
        auto a = top;
        openBtn.setBounds (a.removeFromLeft (110));
        a.removeFromLeft (8);
        fileBankBox.setBounds (a.removeFromLeft (150).reduced (0, 2));
        sendBtn.setBounds (a.removeFromRight (160));
        a.removeFromRight (6);
        copyBtn.setBounds (a.removeFromRight (130));
    }
    r.removeFromTop (10);
    auto f = r.removeFromTop (28);
    search.setBounds (f.removeFromLeft (260).reduced (0, 2));
    f.removeFromLeft (8);
    categoryBox.setBounds (f.removeFromLeft (170).reduced (0, 2));
    f.removeFromLeft (12);
    hideUnread.setBounds (f.removeFromLeft (120));
    loadBtn.setBounds (f.removeFromRight (90));
    r.removeFromTop (8);
    auto bottom = r.removeFromBottom (26);
    stop.setBounds (bottom.removeFromRight (70).reduced (0, 2));
    bottom.removeFromRight (8);
    bar.setBounds (bottom.removeFromRight (220).reduced (0, 6));
    r.removeFromBottom (4);
    table.setBounds (r);
}

void PresetsPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::chassis);
    auto& lib = proc.library();
    g.setColour (theme::ink2);
    g.setFont (theme::fontUi (12.5f, false));
    juce::String s = lib.statusText();
    if (s.isEmpty())
        s = source == Source::Peak ? juce::String (lib.library().loadedCount()) + " of 512 patches read. Double-click to load one."
                                   : juce::String ("Open a .syx file: a single patch, a bank or a backup.");
    g.drawText (s, getLocalBounds().reduced (16, 12).removeFromBottom (26).withTrimmedRight (310), juce::Justification::centredLeft);
}
