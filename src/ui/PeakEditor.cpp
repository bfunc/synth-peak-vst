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
// Top bar: logo | page tabs | help display | ‹ patch › | Fetch Send Compare Save | Patch Level.
// The help display shows the help of the control under the mouse, short messages, or the connection when idle.

class TopBar : public juce::Component, public Refreshable
{
public:
    TopBar (PeakProcessor& p, UiContext& c, const juce::StringArray& pageNames, std::function<void (int)> onPage)
        : proc (p), names (pageNames), pageChanged (std::move (onPage))
    {
        prev.onClick = [this] { proc.session().step (-1); };
        next.onClick = [this] { proc.session().step (1); };
        prev.setTooltip ("Previous patch in the bank (sends Program Change).");
        next.setTooltip ("Next patch in the bank (sends Program Change).");
        fetch.setTooltip ("Fetch: read the Peak's edit buffer into the editor.");
        send.setTooltip ("Send: write this patch to the Peak's edit buffer (not its memory), read it back and report whether the Peak took it.");
        saveBtn.setTooltip ("Save: pick a slot, back up what is there, then write the patch. Or save to a .syx file.");
        fetch.onClick = [this] { proc.session().fetch(); };
        send.onClick = [this] { proc.session().sendAll(); };
        saveBtn.onClick = [this]
        {
            juce::CallOutBox::launchAsynchronously (std::make_unique<SavePanel> (proc), saveBtn.getScreenBounds(), nullptr);
        };
        compare.setTooltip ("Compare: hold to hear the saved patch, release to go back to your edit.");
        compare.onPress = [this] { proc.beginCompare(); repaint(); };
        compare.onRelease = [this] { proc.endCompare(); repaint(); };
        level = makeWidget (c, { ui::Widget::SmallKnob, "patch_level", "Patch Lvl" });
        if (auto* k = dynamic_cast<ParamKnob*> (level.get()))
            k->setShowValue (false);
        for (juce::Component* comp : { (juce::Component*) &prev, (juce::Component*) &next, (juce::Component*) &fetch,
                                       (juce::Component*) &send, (juce::Component*) &saveBtn, (juce::Component*) &compare,
                                       (juce::Component*) level.get() })
            addAndMakeVisible (comp);
    }

    void setPage (int p)
    {
        page = p;
        repaint();
    }
    void say (const juce::String& m)
    {
        message = m;
        messageUntil = juce::Time::getMillisecondCounterHiRes() + 4000;
        repaint (helpRect());
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
        // Help of the control under the mouse (the nearest tooltip client with text, inside this editor).
        juce::String h;
        if (auto* under = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
            if (auto* ed = getParentComponent(); ed != nullptr && ed->isParentOf (under))
                for (auto* c = under; c != nullptr && c != ed; c = c->getParentComponent())
                    if (auto* tc = dynamic_cast<juce::TooltipClient*> (c))
                        if (auto t = tc->getTooltip(); t.isNotEmpty())
                        {
                            h = t;
                            break;
                        }
        if (h != hover)
        {
            hover = h;
            repaint (helpRect());
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
        prev.setBounds (o.getX() - 22, o.getY(), 22, o.getHeight());
        next.setBounds (o.getRight(), o.getY(), 22, o.getHeight());
        auto a = juce::Rectangle<int> (next.getRight() + 14, 15, 274, 26);
        for (juce::Component* b : { (juce::Component*) &fetch, (juce::Component*) &send, (juce::Component*) &compare, (juce::Component*) &saveBtn })
        {
            b->setBounds (a.removeFromLeft (64));
            a.removeFromLeft (6);
        }
        if (level != nullptr)
            level->setBounds (getWidth() - 22 - 46, 2, 46, 54);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1f25), 0, 0, juce::Colour (0xff12161a), 0, (float) getHeight(), false));
        g.fillAll();
        g.setColour (juce::Colours::black);
        g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());

        // Logo: a mountain line (small peak, valley, tall peak) in a 26 px box.
        juce::Path logo;
        const float lx = 22, ly = 15;
        logo.startNewSubPath (lx + 2, ly + 22);
        logo.lineTo (lx + 7, ly + 10);
        logo.lineTo (lx + 10, ly + 14.5f);
        logo.lineTo (lx + 14, ly + 3);
        logo.lineTo (lx + 24, ly + 22);
        g.setColour (accent);
        g.strokePath (logo, juce::PathStrokeType (2.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

        // Tabs
        for (int i = 0; i < names.size(); ++i)
        {
            auto t = tabRect (i);
            const bool on = i == page;
            g.setColour (on ? juce::Colour (0xff20262d) : juce::Colour (0xff15191d));
            g.fillRect (t);
            g.setColour (juce::Colours::black);
            g.drawVerticalLine (t.getX(), 0, (float) getHeight());
            if (i == names.size() - 1)
                g.drawVerticalLine (t.getRight(), 0, (float) getHeight());
            g.setColour (on ? accent : juce::Colour (0xff2a3038));
            g.fillEllipse ((float) t.getCentreX() - 2.5f, 16, 5, 5);
            g.setColour (on ? ink : ink3);
            g.setFont (fontUi (12.0f).withExtraKerningFactor (0.06f));
            g.drawText (names[i].toUpperCase(), t.withTrimmedTop (16), juce::Justification::centred);
        }

        paintHelp (g);

        // Patch display
        auto o = oledRect();
        g.setColour (well);
        g.fillRect (o);
        g.setColour (juce::Colours::black);
        g.drawRect (o);
        auto in = o.reduced (10, 4);
        const auto cat = categoryText();
        g.setFont (fontMono (9.5f));
        g.setColour (proc.comparing() ? led : ink3);
        g.drawText (proc.comparing() ? juce::String ("HEARING THE SAVED PATCH")
                                     : juce::String (proc.session().originText()).toUpperCase() + (cat.isNotEmpty() ? "  " + cat : ""),
                    in.removeFromTop (13), juce::Justification::centredLeft, true);
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
    }

private:
    juce::Rectangle<int> tabRect (int i) const { return { 62 + i * 62, 0, 62, getHeight() - 1 }; }
    juce::Rectangle<int> helpRect() const { return { tabRect (names.size() - 1).getRight() + 14, 8, 286, 40 }; }
    juce::Rectangle<int> oledRect() const { return { helpRect().getRight() + 14 + 22, 8, 260, 40 }; }

    void paintHelp (juce::Graphics& g)
    {
        using namespace theme;
        auto r = helpRect();
        g.setColour (well);
        g.fillRect (r);
        g.setColour (juce::Colours::black);
        g.drawRect (r);
        auto in = r.reduced (10, 3);
        if (hover.isNotEmpty())
        {
            g.setColour (accent);
            g.fillRect (r.getX(), r.getY(), 2, r.getHeight());
            g.setColour (ink);
            g.setFont (fontBody (11.5f));
            g.drawFittedText (hover, in, juce::Justification::centredLeft, 3, 0.85f);
            return;
        }
        if (messageUntil > 0)
        {
            g.setColour (ledSoft);
            g.setFont (fontBody (11.5f));
            g.drawFittedText (message, in, juce::Justification::centredLeft, 3, 0.85f);
            return;
        }
        const auto st = proc.link().status (proc.midiChannel());
        const bool ok = (st.hostOutput || st.outOpen) && st.inOpen;
        auto line = in.removeFromTop (in.getHeight() / 2);
        g.setColour (ok ? accent : juce::Colour (0xff3a2a1f));
        g.fillEllipse ((float) line.getX(), (float) line.getCentreY() - 3.5f, 7, 7);
        const juce::String dot = juce::String::charToString ((juce::juce_wchar) 0x00B7);
        juce::String t = ok ? "PEAK" : "OFFLINE";
        t << " " << dot << " CH " << proc.midiChannel();
        if (! st.firmware.empty())
            t << " " << dot << " FW " << juce::String (st.firmware);
        if (st.hostOutput)
            t << " " << dot << " DAW";
        g.setColour (ink2);
        g.setFont (fontMono (11.0f));
        g.drawText (t, line.withTrimmedLeft (14), juce::Justification::centredLeft);
        g.setColour (ink3);
        g.setFont (fontBody (11.0f));
        g.drawText ("Hover a control for help " + dot + " drag a source onto a knob", in, juce::Justification::centredLeft, true);
    }

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
    std::unique_ptr<ParamWidget> level;
    juce::String message, lastStatus, lastLib, lastKey, hover;
    double messageUntil = 0;
};

// ---------------------------------------------------------------------------------------------------------------
// A modulated knob's slots: pick one (Alt + wheel then sets its depth), change depth, remove; Open in Matrix.

class ModBubble : public juce::Component
{
public:
    ModBubble (UiContext& c, const std::string& paramId, std::function<juce::String (const ui::SlotRef&)> describe,
               std::function<void()> openMatrix)
        : ctx (c), id (paramId), text (std::move (describe)), matrix (std::move (openMatrix))
    {
        const int i = indexOfId (id);
        title = i >= 0 ? juce::String (paramTable()[(size_t) i].name) : juce::String (id);
        link.onClick = [this]
        {
            if (matrix)
                matrix();
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        };
        addAndMakeVisible (link);
        rebuild();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.fillAll (juce::Colour (0xff1a1f25));
        g.setColour (led);
        g.fillRect (0, 0, getWidth(), 2);
        g.setColour (ink);
        g.setFont (fontUi (11.0f).withExtraKerningFactor (0.1f));
        g.drawText (title.toUpperCase(), 10, 4, getWidth() - 20, 22, juce::Justification::centredLeft);
        g.setColour (ink3);
        g.setFont (fontBody (10.5f));
        g.drawText (juce::String ((int) rows.size()) + (rows.size() == 1 ? " slot" : " slots") + " " + juce::String::charToString ((juce::juce_wchar) 0x00B7)
                        + " click a name to select it",
                    10, 4, getWidth() - 20, 22, juce::Justification::centredRight);
        for (size_t r = 0; r < rows.size(); ++r)
        {
            auto line = rowRect ((int) r);
            g.setColour (juce::Colour (0xff0d1013));
            g.drawHorizontalLine (line.getY(), 0.0f, (float) getWidth());
            const bool sel = ctx.focus == rows[r].ref;
            auto name = line.withWidth (getWidth() - 10 - 150).withTrimmedLeft (10);
            if (sel)
            {
                g.setColour (led.withAlpha (0.08f));
                g.fillRect (name.withTrimmedLeft (-4).reduced (0, 3));
                g.setColour (led);
                g.fillRect (name.getX() - 4, name.getY() + 3, 2, name.getHeight() - 6);
            }
            g.setColour (ink);
            g.setFont (fontUi (11.5f));
            g.drawText (rows[r].label, name.removeFromTop (name.getHeight() / 2 + 2).withTrimmedTop (4), juce::Justification::bottomLeft);
            g.setColour (ink3);
            g.setFont (fontUi (9.5f, false));
            g.drawText (juce::String (rows[r].ref.fx ? "FX slot " : "Slot ") + juce::String (rows[r].ref.slot) + (sel ? juce::String (" " "Â·" " selected") : juce::String()),
                        name.withTrimmedTop (2), juce::Justification::topLeft);
        }
        g.setColour (juce::Colour (0xff0b0d10));
        g.drawHorizontalLine (getHeight() - 30, 0.0f, (float) getWidth());
        g.setColour (ink3);
        g.setFont (fontBody (11.0f));
        g.drawText ("Alt + wheel on the knob sets the selected slot.", 10, getHeight() - 30, 220, 30, juce::Justification::centredLeft);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (size_t r = 0; r < rows.size(); ++r)
            if (rowRect ((int) r).withWidth (getWidth() - 160).contains (e.getPosition()))
            {
                ctx.focus = rows[r].ref;
                repaint();
            }
    }

    void resized() override
    {
        for (size_t r = 0; r < rows.size(); ++r)
        {
            auto line = rowRect ((int) r);
            rows[r].remove->setBounds (line.removeFromRight (30).withSizeKeepingCentre (22, 22));
            rows[r].depth->setBounds (line.removeFromRight (128).withSizeKeepingCentre (128, 22));
        }
        link.setBounds (getWidth() - 110, getHeight() - 26, 100, 22);
    }

private:
    struct Row
    {
        ui::SlotRef ref;
        juce::String label;
        std::unique_ptr<ParamBar> depth;
        std::unique_ptr<juce::TextButton> remove;
    };

    juce::Rectangle<int> rowRect (int r) const { return { 0, 30 + r * 42, getWidth(), 42 }; }

    void rebuild()
    {
        rows.clear();
        for (const auto& ref : ui::slotsFor (id, ctx.params.values()))
        {
            Row r;
            r.ref = ref;
            r.label = text ? text (ref) : juce::String();
            const auto k = (ref.fx ? "fxmod" : "mod") + std::to_string (ref.slot) + "_";
            r.depth = std::make_unique<ParamBar> (ctx, k + "depth");
            r.remove = std::make_unique<juce::TextButton> (juce::CharPointer_UTF8 ("â"));
            r.remove->setTooltip ("Remove this slot.");
            r.remove->onClick = [this, k, self = juce::Component::SafePointer<ModBubble> (this)]
            {
                for (const char* f : { "src_a", "src_b", "dest" })
                    ctx.params.setByUser ((size_t) indexOfId (k + f), 0);
                ctx.params.setByUser ((size_t) indexOfId (k + "depth"), 64);
                juce::MessageManager::callAsync ([safe = self]
                {
                    if (safe == nullptr)
                        return;
                    safe->rebuild();
                    if (safe->rows.empty())
                        if (auto* box = safe->findParentComponentOfClass<juce::CallOutBox>())
                            box->dismiss();
                });
            };
            addAndMakeVisible (*r.depth);
            addAndMakeVisible (*r.remove);
            rows.push_back (std::move (r));
        }
        setSize (330, 30 + (int) rows.size() * 42 + 30);
        resized();
        repaint();
    }

    UiContext& ctx;
    std::string id;
    std::function<juce::String (const ui::SlotRef&)> text;
    std::function<void()> matrix;
    juce::String title;
    std::vector<Row> rows;
    juce::TextButton link { juce::CharPointer_UTF8 ("Open in Matrix âº") };
};

// ---------------------------------------------------------------------------------------------------------------

PeakEditor::PeakEditor (PeakProcessor& p)
    : AudioProcessorEditor (p), proc (p), ctx { p.params(), {}, {} }
{
    setLookAndFeel (&look);
    juce::LookAndFeel::setDefaultLookAndFeel (&look);
    ctx.onSourceDropped = [this] (int s, const std::string& id) { sourceDropped (s, id); };
    ctx.onModClick = [this] (const std::string& id, juce::Component& knob) { showSlots (id, knob); };
    ctx.onModWheel = [this] (const std::string& id, int steps) { modWheel (id, steps); };
    ctx.savedValue = [this] (int i)
    {
        const auto& b = proc.session().baselineValues();
        return i >= 0 && (size_t) i < b.size() ? b[(size_t) i] : -1;
    };

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
    top = std::make_unique<TopBar> (proc, ctx, names, [this] (int i) { showPage (i); });
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
    const auto& saved = proc.session().baselineValues();
    if (v == lastValues && saved == lastSaved && ctx.focus == lastFocus)
        return;
    lastValues = std::move (v);
    lastSaved = saved;
    lastFocus = ctx.focus;
    for (auto* w : ctx.registry)
    {
        const auto& id = w->def().id;
        ModInfo m;
        const auto r = ui::modRangeFor (id, lastValues);
        m.lo = r.lo;
        m.hi = r.hi;
        for (const auto& ref : ui::slotsFor (id, lastValues))
            if (ref == ctx.focus)
            {
                const auto f = ui::slotRange (ref, lastValues);
                m.focus = true;
                m.flo = f.lo;
                m.fhi = f.hi;
            }
        w->setModInfo (m);
        w->setSaved (lastSaved.empty() ? -1 : lastSaved[(size_t) w->index()]);
    }
}

juce::String PeakEditor::slotText (const ui::SlotRef& r) const
{
    const auto k = (r.fx ? "fxmod" : "mod") + std::to_string (r.slot) + "_";
    const auto& p = proc.params();
    const auto& defA = paramTable()[(size_t) indexOfId (k + "src_a")];
    const int a = p.value ((size_t) indexOfId (k + "src_a")), b = p.value ((size_t) indexOfId (k + "src_b"));
    juce::String t (formatValue (defA, a));
    if (b > 0)
        t << " " << juce::String::charToString ((juce::juce_wchar) 0x00D7) << " " << juce::String (formatValue (defA, b));
    return t;
}

void PeakEditor::showSlots (const std::string& paramId, juce::Component& knob)
{
    auto bubble = std::make_unique<ModBubble> (ctx, paramId, [this] (const ui::SlotRef& r) { return slotText (r); },
                                               [this] { showPage (2); });
    juce::CallOutBox::launchAsynchronously (std::move (bubble), knob.getScreenBounds(), nullptr);
}

void PeakEditor::modWheel (const std::string& paramId, int steps)
{
    const auto values = proc.params().values();
    const auto slots = ui::slotsFor (paramId, values);
    const int i = indexOfId (paramId);
    const juce::String name = i >= 0 ? juce::String (paramTable()[(size_t) i].name) : juce::String (paramId);
    ui::SlotRef ref;
    for (const auto& s : slots)
        if (s == ctx.focus)
            ref = s;
    if (! ref.valid() && slots.size() == 1)
        ref = slots[0];
    if (! ref.valid())
    {
        if (slots.empty())
            say (ui::modDestForParam (paramId) < 0 && ui::fxDestForParam (paramId) < 0
                     ? name + " can't be a mod destination on the Peak."
                     : name + " has no modulation yet. Drag a source onto it.");
        else
            say (juce::String ((int) slots.size()) + " slots on " + name + ": click the knob and pick one first.");
        return;
    }
    const auto k = (ref.fx ? "fxmod" : "mod") + std::to_string (ref.slot) + "_";
    const size_t di = (size_t) indexOfId (k + "depth");
    int d = juce::jlimit (0, 127, proc.params().value (di) + steps);
    if (d == 64)
        d += steps > 0 ? 1 : -1;  // depth 0 would empty the slot
    proc.params().setByUser (di, d);
    ctx.focus = ref;
    say (name + " " + juce::String::charToString ((juce::juce_wchar) 0x00B7) + " " + (ref.fx ? "FX slot " : "slot ") + juce::String (ref.slot) + " "
         + juce::String::charToString ((juce::juce_wchar) 0x00B7) + " " + slotText (ref) + " " + juce::String::charToString ((juce::juce_wchar) 0x00B7)
         + " depth " + (d > 64 ? "+" : "") + juce::String (d - 64));
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
    if (t.existing)
    {
        ctx.focus = { t.fx, t.slot };
        say ("That source already drives " + target + " (" + (t.fx ? "FX slot " : "slot ") + juce::String (t.slot)
             + "). Selected: Alt + wheel on the knob changes its depth.");
        return;
    }
    const auto k = (t.fx ? "fxmod" : "mod") + std::to_string (t.slot) + "_";
    auto& prm = proc.params();
    prm.setByUser ((size_t) indexOfId (k + "src_a"), t.source);
    prm.setByUser ((size_t) indexOfId (k + "dest"), t.dest);
    prm.setByUser ((size_t) indexOfId (k + "depth"), 64 + 24);
    ctx.focus = { t.fx, t.slot };
    const auto& srcList = paramTable()[(size_t) indexOfId (k + "src_a")].choices;
    say (juce::String (t.fx ? "FX slot " : "Slot ") + juce::String (t.slot) + ": " + juce::String (srcList[(size_t) t.source])
         + " > " + target + ", +24. Alt + wheel on the knob sets the depth.");
}
