#include "PeakEditor.h"

#include "Theme.h"
#include "core/Library.h"
#include "ui/FxMatrixPages.h"
#include "ui/Graphs.h"
#include "ui/LibrarySetupPages.h"
#include "ui/MainPage.h"
#include "ui/PresetsPage.h"
#include "ui/ModMap.h"

using namespace peak;

namespace
{
const juce::Identifier kPage { "editorPage" };
}

// ---------------------------------------------------------------------------------------------------------------
// Save pop-out: name, slot, Save (writes the synth's memory after a backup), or files.

class SavePanel : public juce::Component
{
public:
    explicit SavePanel (PeakProcessor& p) : proc (p)
    {
        name.setText (proc.session().name(), juce::dontSendNotification);
        name.setInputRestrictions (16);
        name.setFont (theme::fontUi (14.0f, false));
        for (int b = 1; b <= kBanks; ++b)
            bank.addItem (juce::String::charToString ((juce::juce_wchar) ('A' + b - 1)), b);
        for (int s = 0; s < kSlots; ++s)
            slot.addItem (juce::String (s).paddedLeft ('0', 3), s + 1);
        const auto& o = proc.session().origin();
        bank.setSelectedId (o.kind == PatchSession::Origin::Kind::Slot ? o.bank : 1, juce::dontSendNotification);
        slot.setSelectedId ((o.kind == PatchSession::Origin::Kind::Slot ? o.slot : 0) + 1, juce::dontSendNotification);
        save.onClick = [this] { confirmSave(); };
        exportBtn.onClick = [this] { exportFile(); };
        importBtn.onClick = [this] { importFile(); };
        for (juce::Component* c : { (juce::Component*) &name, (juce::Component*) &bank, (juce::Component*) &slot,
                                    (juce::Component*) &save, (juce::Component*) &exportBtn, (juce::Component*) &importBtn })
            addAndMakeVisible (c);
        setSize (360, 214);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.fillAll (panel);
        g.setColour (ink3);
        g.setFont (fontUi (11.0f));
        g.drawText ("NAME", 12, 12, 60, 26, juce::Justification::centredLeft);
        g.drawText ("SLOT", 12, 48, 60, 26, juce::Justification::centredLeft);
        g.setFont (fontUi (11.5f, false));
        g.drawFittedText ("Saving writes the Peak's memory. The slot's old patch is saved first to\n"
                              + LibraryService::backupFolder().getFullPathName(),
                          12, 116, getWidth() - 24, 40, juce::Justification::topLeft, 3);
    }

    void resized() override
    {
        name.setBounds (76, 12, getWidth() - 88, 26);
        bank.setBounds (76, 48, 60, 26);
        slot.setBounds (142, 48, 72, 26);
        save.setBounds (getWidth() - 132, 48, 120, 26);
        exportBtn.setBounds (12, getHeight() - 40, 160, 28);
        importBtn.setBounds (getWidth() - 172, getHeight() - 40, 160, 28);
    }

private:
    void confirmSave()
    {
        const int b = bank.getSelectedId(), s = slot.getSelectedId() - 1;
        const auto& e = proc.library().library().at (b, s);
        const auto old = e.loaded ? " (now \"" + juce::String (e.name) + "\")" : juce::String();
        juce::AlertWindow::showOkCancelBox (
            juce::MessageBoxIconType::WarningIcon, "Save to " + juce::String (slotLabel (b, s)),
            "Write \"" + name.getText() + "\" into " + slotLabel (b, s) + old + "?\nThe old patch is backed up first.",
            "Save", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([p = &proc, b, s, n = name.getText().toStdString()] (int ok)
            {
                if (ok == 1)
                    p->library().saveToSlot (b, s, n);
            }));
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }

    void exportFile()
    {
        proc.session().setName (name.getText().toStdString());
        auto file = LibraryService::userFolder().getChildFile (juce::File::createLegalFileName (name.getText().trim()) + ".syx");
        chooser = std::make_unique<juce::FileChooser> ("Export patch", file, "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [p = &proc] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
                p->library().exportPatch (fc.getResult().withFileExtension ("syx"));
        });
    }

    void importFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a patch", LibraryService::userFolder(), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [p = &proc] (const juce::FileChooser& fc)
        {
            // One patch: play it now (edit buffer). More: they show on the Presets tab under File.
            if (fc.getResult().existsAsFile() && p->library().openFile (fc.getResult()) && p->library().file().size() == 1)
                p->library().tryPatch (0);
        });
    }

    PeakProcessor& proc;
    juce::TextEditor name;
    juce::ComboBox bank, slot;
    juce::TextButton save { "Save to slot" }, exportBtn { "Export .syx..." }, importBtn { "Load .syx..." };
    std::unique_ptr<juce::FileChooser> chooser;
};

// ---------------------------------------------------------------------------------------------------------------
// Top bar

class TopBar : public juce::Component, public Refreshable
{
public:
    TopBar (PeakProcessor& p, const juce::StringArray& pageNames, std::function<void (int)> onPage)
        : proc (p), names (pageNames), pageChanged (std::move (onPage))
    {
        prev.onClick = [this] { proc.session().step (-1); };
        next.onClick = [this] { proc.session().step (1); };
        prev.setTooltip ("Previous patch (program change)");
        next.setTooltip ("Next patch (program change)");
        fetch.setTooltip ("Read the Peak's current patch");
        send.setTooltip ("Write this patch into the Peak's edit buffer (not its memory)");
        saveBtn.setTooltip ("Save into a slot, or to a file");
        fetch.onClick = [this] { proc.session().fetch(); };
        send.onClick = [this] { proc.session().sendAll(); };
        saveBtn.onClick = [this]
        {
            juce::CallOutBox::launchAsynchronously (std::make_unique<SavePanel> (proc), saveBtn.getScreenBounds(), nullptr);
        };
        compare.setTooltip ("Hold to hear the saved patch; release to go back to your edit");
        compare.onPress = [this] { proc.beginCompare(); repaint(); };
        compare.onRelease = [this] { proc.endCompare(); repaint(); };
        for (juce::Component* c : { (juce::Component*) &prev, (juce::Component*) &next, (juce::Component*) &fetch,
                                    (juce::Component*) &send, (juce::Component*) &saveBtn, (juce::Component*) &compare })
            addAndMakeVisible (c);
    }

    void setPage (int p)
    {
        page = p;
        repaint();
    }
    void say (const juce::String& m)
    {
        message = m;
        messageUntil = juce::Time::getMillisecondCounterHiRes() + 6000;
        repaint();
    }

    void refresh() override
    {
        const auto st = proc.link().status (proc.midiChannel());
        auto text = proc.session().statusText() + "|" + proc.library().statusText();
        const int edited = proc.session().editedCount();
        compare.setEnabled (compare.isHeld() || (proc.session().hasBaseline() && edited > 0));
        juce::String key;
        key << (int) proc.comparing() << (int) st.inOpen << (int) st.outOpen << (int) st.hostOutput << (int) st.isPeak << st.firmware << edited << proc.session().name()
            << proc.session().originText() << proc.midiChannel();
        if (text != lastStatus)
        {
            lastStatus = text;
            const auto& s1 = proc.session().statusText();
            const auto& s2 = proc.library().statusText();
            if (s2 != lastLib && s2.isNotEmpty())
                say (s2);
            else if (s1.isNotEmpty())
                say (s1);
            lastLib = s2;
        }
        if (key != lastKey || (messageUntil > 0 && juce::Time::getMillisecondCounterHiRes() > messageUntil))
        {
            if (juce::Time::getMillisecondCounterHiRes() > messageUntil)
                messageUntil = 0;
            lastKey = key;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < names.size(); ++i)
            if (tabRect (i).contains (e.getPosition()))
            {
                if (pageChanged)
                    pageChanged (i);
                return;
            }
        if (oledRect().contains (e.getPosition()))
            rename();
    }

    void resized() override
    {
        auto o = oledRect();
        prev.setBounds (o.getX() - 24, o.getY(), 22, o.getHeight());
        next.setBounds (o.getRight() + 2, o.getY(), 22, o.getHeight());
        auto a = juce::Rectangle<int> (next.getRight() + 12, 15, 312, 26);
        fetch.setBounds (a.removeFromLeft (70));
        a.removeFromLeft (6);
        send.setBounds (a.removeFromLeft (64));
        a.removeFromLeft (6);
        saveBtn.setBounds (a.removeFromLeft (64));
        a.removeFromLeft (6);
        compare.setBounds (a.removeFromLeft (84));
    }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1f25), 0, 0, juce::Colour (0xff12161a), 0, (float) getHeight(), false));
        g.fillAll();
        g.setColour (juce::Colours::black);
        g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());

        // Brand
        juce::Path logo;
        logo.startNewSubPath (24, 41);
        logo.lineTo (31, 28);
        logo.lineTo (35, 34);
        logo.lineTo (39, 25);
        logo.lineTo (46, 41);
        g.setColour (accent);
        g.strokePath (logo, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (led);
        g.fillEllipse (37, 23, 4, 4);
        g.setColour (ink);
        g.setFont (fontUi (17.0f).withExtraKerningFactor (0.14f));
        g.drawText ("PEAK", 54, 0, 60, getHeight(), juce::Justification::centredLeft);
        g.setColour (ink3);
        g.setFont (fontUi (11.0f, false).withExtraKerningFactor (0.2f));
        g.drawText ("CONTROL", 104, 0, 70, getHeight(), juce::Justification::centredLeft);

        // Tabs
        for (int i = 0; i < names.size(); ++i)
        {
            auto t = tabRect (i);
            const bool on = i == page;
            g.setColour (on ? juce::Colour (0xff20262d) : juce::Colour (0xff15191d));
            g.fillRect (t);
            g.setColour (juce::Colours::black);
            g.drawVerticalLine (t.getX(), 0, (float) getHeight());
            g.setColour (on ? accent : juce::Colour (0xff2a3038));
            g.fillEllipse ((float) t.getCentreX() - 2.5f, 16, 5, 5);
            g.setColour (on ? ink : ink3);
            g.setFont (fontUi (12.0f).withExtraKerningFactor (0.12f));
            g.drawText (names[i].toUpperCase(), t.withTrimmedTop (16), juce::Justification::centred);
        }

        // Patch display
        auto o = oledRect();
        g.setColour (well);
        g.fillRect (o);
        g.setColour (juce::Colours::black);
        g.drawRect (o);
        auto in = o.reduced (10, 4);
        const bool showMsg = messageUntil > 0;
        const auto cat = categoryText();
        g.setFont (fontMono (10.0f));
        g.setColour (showMsg ? led.withAlpha (0.9f) : ink3);
        g.drawText (proc.comparing() ? juce::String ("HEARING THE SAVED PATCH")
                    : showMsg ? message : (juce::String (proc.session().originText()).toUpperCase() + (cat.isNotEmpty() ? "  " + cat : "")),
                    in.removeFromTop (13), juce::Justification::centredLeft);
        g.setFont (fontMono (15.0f));
        g.setColour (accent);
        const auto name = juce::String (proc.session().name());
        g.drawText (name, in, juce::Justification::centredLeft);
        const int edited = proc.session().editedCount();
        if (edited > 0)
        {
            const float nameW = juce::GlyphArrangement::getStringWidth (fontMono (15.0f), name);
            g.setColour (led);
            g.setFont (fontMono (12.0f));
            g.drawText ("* " + juce::String (edited), in.withTrimmedLeft ((int) nameW + 8), juce::Justification::centredLeft);
        }

        // Connection
        const auto st = proc.link().status (proc.midiChannel());
        const bool ok = (st.hostOutput || st.outOpen) && st.inOpen;
        auto c = juce::Rectangle<int> (compare.getRight() + 16, 0, getWidth() - compare.getRight() - 24, getHeight());
        g.setColour (ok ? accent : juce::Colour (0xff3a2a1f));
        g.fillEllipse ((float) c.getX(), (float) c.getCentreY() - 3.5f, 7, 7);
        g.setColour (ink2);
        g.setFont (fontUi (11.5f, false));
        juce::String t = ok ? "PEAK" : "OFFLINE";
        t << " " << juce::String::charToString ((juce::juce_wchar) 0x00B7) << " CH " << proc.midiChannel();
        if (st.hostOutput)
            t << " DAW";
        g.drawText (t, c.withTrimmedLeft (14), juce::Justification::centredLeft);
    }

private:
    juce::Rectangle<int> tabRect (int i) const { return { 176 + i * 88, 0, 88, getHeight() - 1 }; }
    juce::Rectangle<int> oledRect() const { return { 176 + names.size() * 88 + 34, 8, 222, 40 }; }

    juce::String categoryText() const
    {
        const int i = indexOfId ("category");
        return i < 0 ? juce::String() : juce::String (formatValue (paramTable()[(size_t) i], proc.params().value ((size_t) i))).toUpperCase();
    }

    void rename()
    {
        auto* w = new juce::AlertWindow ("Patch name", "Up to 16 characters", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.session().name());
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            if (r == 1)
                proc.session().setName (w->getTextEditorContents ("name").substring (0, 16).toStdString());
            repaint();
        }), true);
    }

    PeakProcessor& proc;
    juce::StringArray names;
    std::function<void (int)> pageChanged;
    int page = 0;
    juce::TextButton prev { juce::CharPointer_UTF8 ("\xe2\x80\xb9") }, next { juce::CharPointer_UTF8 ("\xe2\x80\xba") };
    juce::TextButton fetch { "Fetch" }, send { "Send" }, saveBtn { "Save" };
    HoldButton compare { "Compare" };
    juce::String message, lastStatus, lastLib, lastKey;
    double messageUntil = 0;
};

// ---------------------------------------------------------------------------------------------------------------

PeakEditor::PeakEditor (PeakProcessor& p)
    : AudioProcessorEditor (p), proc (p), ctx { p.params(), {}, {} }
{
    setLookAndFeel (&look);
    juce::LookAndFeel::setDefaultLookAndFeel (&look);
    ctx.onSourceDropped = [this] (int s, const std::string& id) { sourceDropped (s, id); };

    auto playheadsOn = [this] { return proc.getSettings().playheads; };
    pages.push_back ({ "Main", std::make_unique<MainPage> (ctx, proc.feed(), playheadsOn) });
    pages.push_back ({ "FX", std::make_unique<FxPage> (ctx) });
    pages.push_back ({ "Matrix", std::make_unique<MatrixPage> (ctx) });
    pages.push_back ({ "Presets", std::make_unique<PresetsPage> (proc) });
    pages.push_back ({ "Setup", std::make_unique<SetupPage> (proc) });

    juce::StringArray names;
    for (auto& pg : pages)
    {
        names.add (pg.name);
        canvas.addChildComponent (*pg.comp);
    }
    top = std::make_unique<TopBar> (proc, names, [this] (int i) { showPage (i); });
    canvas.addAndMakeVisible (*top);
    addAndMakeVisible (canvas);
    canvas.setSize (theme::kWidth, theme::kHeight);

    showPage (juce::jlimit (0, (int) pages.size() - 1, (int) proc.apvts.state.getProperty (kPage, 0)));

    setResizable (true, true);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) theme::kWidth / theme::kHeight);
    setResizeLimits (theme::kWidth / 2, theme::kHeight / 2, theme::kWidth * 2, theme::kHeight * 2);
    setSize (theme::kWidth * 9 / 10, theme::kHeight * 9 / 10);
    startTimerHz (30);
}

PeakEditor::~PeakEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void PeakEditor::showPage (int i)
{
    current = juce::jlimit (0, (int) pages.size() - 1, i);
    for (int k = 0; k < (int) pages.size(); ++k)
        pages[(size_t) k].comp->setVisible (k == current);
    top->setPage (current);
    proc.apvts.state.setProperty (kPage, current, nullptr);
}

void PeakEditor::say (const juce::String& m) { top->say (m); }

void PeakEditor::paint (juce::Graphics& g) { g.fillAll (theme::voidBg); }

void PeakEditor::resized()
{
    const float s = (float) getWidth() / (float) theme::kWidth;
    canvas.setTransform (juce::AffineTransform::scale (s));
    canvas.setBounds (0, 0, theme::kWidth, theme::kHeight);
    top->setBounds (0, 0, theme::kWidth, 56);
    const auto body = juce::Rectangle<int> (18, 64, theme::kWidth - 36, theme::kHeight - 64 - 10);
    for (auto& pg : pages)
        pg.comp->setBounds (body);
}

void PeakEditor::refreshNow() { timerCallback(); }

void PeakEditor::timerCallback()
{
    top->refresh();
    if (auto* r = dynamic_cast<Refreshable*> (pages[(size_t) current].comp.get()))
        r->refresh();
    updateModRings();
}

void PeakEditor::updateModRings()
{
    auto v = proc.params().values();
    if (v == lastValues)
        return;
    lastValues = std::move (v);
    for (auto* w : ctx.registry)
        w->setModAmount (ui::modAmountFor (w->def().id, lastValues));
}

void PeakEditor::sourceDropped (int modSource, const std::string& paramId)
{
    const auto t = ui::slotForDrop (modSource, paramId, proc.params().values());
    const int i = indexOfId (paramId);
    const juce::String target = i >= 0 ? juce::String (paramTable()[(size_t) i].name) : juce::String (paramId);
    if (t.dest < 0)
    {
        say (target + " can't be a mod destination");
        return;
    }
    if (t.source < 0)
    {
        say ("That source can't modulate effects. Use LFO 3/4 or a performance source.");
        return;
    }
    if (t.slot == 0)
    {
        say (t.fx ? "All 4 FX mod slots are in use" : "All 16 mod slots are in use: clear one on the Matrix tab");
        return;
    }
    const auto k = (t.fx ? "fxmod" : "mod") + std::to_string (t.slot) + "_";
    auto& prm = proc.params();
    prm.setByUser ((size_t) indexOfId (k + "src_a"), t.source);
    prm.setByUser ((size_t) indexOfId (k + "dest"), t.dest);
    prm.setByUser ((size_t) indexOfId (k + "depth"), 64 + 24);
    const auto& srcList = paramTable()[(size_t) indexOfId (k + "src_a")].choices;
    say (juce::String (t.fx ? "FX slot " : "Slot ") + juce::String (t.slot) + ": " + juce::String (srcList[(size_t) t.source])
         + " > " + target + ", +24");
}
