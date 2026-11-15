#include "FxMatrixPages.h"

#include "Theme.h"
#include "ui/Layout.h"

using namespace peak;

namespace
{
void knobIn (juce::Component* c, juce::Rectangle<int> cell)
{
    if (c != nullptr)
        c->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 64), juce::jmin (cell.getHeight(), 82)));
}
} // namespace

// ---- FxRow ----

FxRow::FxRow (UiContext& c, const char* k, const char* titleText, FxGraph::Type t)
    : PanelFrame (c, titleText, k), key (k), w (c), graph (c.params, t)
{
    w.add (*this, ui::panel (k)->items);
    addAndMakeVisible (graph);
    refresh();
}

void FxRow::refresh()
{
    graph.refresh();
    if (key != "delay")
        return;
    const int sync = ctx.params.value ((size_t) indexOfId ("dly_sync"));
    if (sync != lastSync)
    {
        lastSync = sync;
        w["dly_time"].setVisible (sync == 0);
        w["dly_sync_time"].setVisible (sync != 0);
    }
}

void FxRow::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds();
    g.setColour (panel);
    g.fillRect (r);
    g.setColour (accent);
    g.fillRect (r.removeFromTop (2));
    auto tab = r.removeFromLeft (118);
    g.setColour (panelHi);
    g.fillRect (tab);
    g.setColour (juce::Colours::black);
    g.drawVerticalLine (tab.getRight(), (float) tab.getY(), (float) tab.getBottom());
    g.setColour (ink);
    g.setFont (fontUi (13.0f));
    g.drawText (title.toUpperCase(), tab.reduced (12, 0), juce::Justification::centredLeft);
}

void FxRow::resized()
{
    PanelFrame::resized();
    auto r = getLocalBounds().withTrimmedTop (2);
    r.removeFromLeft (118 + 12);
    graph.setBounds (r.removeFromLeft (120).reduced (0, 10));
    r.removeFromLeft (12);
    r.removeFromRight (24);
    // Choice-type widgets in a column first, then the knobs.
    std::vector<juce::Component*> knobs;
    auto opts = juce::Rectangle<int>();
    for (const auto& it : ui::panel (key)->items)
    {
        auto* c = w.get (it.id);
        if (c == nullptr)
            continue;
        if (dynamic_cast<ParamKnob*> (c) != nullptr)
        {
            if (! (key == "delay" && it.id == "dly_sync_time"))
                knobs.push_back (c);
        }
        else
        {
            if (opts.isEmpty())
                opts = r.removeFromLeft (124).reduced (0, 8);
            c->setBounds (opts.removeFromTop (26).reduced (0, 2));
        }
    }
    const int cell = juce::jmin (68, r.getWidth() / juce::jmax (1, (int) knobs.size()));
    for (auto* k : knobs)
        knobIn (k, r.removeFromLeft (cell));
    if (key == "delay")
        if (auto* t = w.get ("dly_time"))
            w["dly_sync_time"].setBounds (t->getBounds());
}

// ---- FxPage ----

FxPage::FxPage (UiContext& c) : top (c, {}, "fx"), w (c)
{
    addAndMakeVisible (top);
    w.add (top, ui::panel ("fx")->items);
    rows[0] = std::make_unique<FxRow> (c, "dist", "Distortion", FxGraph::Type::Distortion);
    rows[1] = std::make_unique<FxRow> (c, "chorus", "Chorus", FxGraph::Type::Chorus);
    rows[2] = std::make_unique<FxRow> (c, "delay", "Delay", FxGraph::Type::Delay);
    rows[3] = std::make_unique<FxRow> (c, "reverb", "Reverb", FxGraph::Type::Reverb);
    for (auto& r : rows)
        addAndMakeVisible (*r);
}

void FxPage::refresh()
{
    for (auto& r : rows)
        r->refresh();
}

void FxPage::resized()
{
    auto r = getLocalBounds();
    top.setBounds (r.removeFromTop (88));
    auto t = top.getLocalBounds().reduced (14, 8);
    w["fx_bypass"].setBounds (t.removeFromLeft (90).withSizeKeepingCentre (90, 24));
    t.removeFromLeft (20);
    w["fx_routing"].setBounds (t.removeFromLeft (190).withSizeKeepingCentre (190, 24));
    auto dw = t.removeFromRight (140);
    knobIn (&w["fx_dry"], dw.removeFromLeft (70));
    knobIn (&w["fx_wet"], dw);
    r.removeFromTop (8);
    const int heights[] = { 100, 130, 140, 140 };
    for (size_t i = 0; i < rows.size(); ++i)
    {
        rows[i]->setBounds (r.removeFromTop (heights[i]));
        r.removeFromTop (8);
    }
}

// ---- MatrixPage ----

MatrixPage::MatrixPage (UiContext& c)
    : ctx (c), mod (c, "Mod Matrix", 16, "16 slots: Source A x Source B x Amount > Destination"),
      fx (c, "FX Mod Matrix", 4, "4 slots: LFO 1/2 and the envelopes can't reach the effects")
{
    addAndMakeVisible (mod);
    addAndMakeVisible (fx);
    for (int s = 1; s <= 16; ++s)
        addRow ("mod" + std::to_string (s) + "_");
    for (int s = 1; s <= 4; ++s)
        addRow ("fxmod" + std::to_string (s) + "_");
}

void MatrixPage::addRow (const std::string& prefix)
{
    Row r;
    r.prefix = prefix;
    auto& parent = prefix.rfind ("fx", 0) == 0 ? fx : mod;
    r.a = makeWidget (ctx, { ui::Widget::Choice, prefix + "src_a", "" });
    r.b = makeWidget (ctx, { ui::Widget::Choice, prefix + "src_b", "" });
    r.dest = makeWidget (ctx, { ui::Widget::Choice, prefix + "dest", "" });
    r.depth = std::make_unique<ParamBar> (ctx, prefix + "depth");
    r.clear = std::make_unique<juce::TextButton> (juce::CharPointer_UTF8 ("\xe2\x9c\x95"));
    r.clear->setTooltip ("Clear this slot");
    r.clear->onClick = [this, prefix]
    {
        for (const char* f : { "src_a", "src_b", "dest" })
            ctx.params.setByUser ((size_t) indexOfId (prefix + f), 0);
        ctx.params.setByUser ((size_t) indexOfId (prefix + "depth"), 64);
    };
    for (juce::Component* c : { (juce::Component*) r.a.get(), (juce::Component*) r.b.get(), (juce::Component*) r.dest.get(),
                                (juce::Component*) r.depth.get(), (juce::Component*) r.clear.get() })
        parent.addAndMakeVisible (c);
    rows.push_back (std::move (r));
}

void MatrixPage::paint (juce::Graphics&) {}

void MatrixFrame::paint (juce::Graphics& g)
{
    using namespace theme;
    PanelFrame::paint (g);
    g.setFont (fontUi (10.5f, false));
    g.setColour (ink3);
    g.drawText (note, getLocalBounds().withHeight (kTitleH).withTrimmedRight (12), juce::Justification::centredRight);
    auto area = content().reduced (10, 0);
    auto head = area.removeFromTop (kHeadH);
    g.setFont (fontUi (10.0f));
    auto col = [&] (int w, const char* text) { g.drawText (text, head.removeFromLeft (w), juce::Justification::centredLeft); };
    col (40, "#");
    col (204, "SOURCE A");
    col (196, "SOURCE B");
    g.drawText ("DESTINATION", head.withTrimmedRight (34).removeFromRight (220), juce::Justification::centredLeft);
    g.drawText ("AMOUNT", head, juce::Justification::centredLeft);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (head.getBottom(), (float) area.getX(), (float) area.getRight());
    g.setFont (fontMono (11.0f));
    for (int i = 0; i < slots; ++i)
    {
        auto line = area.removeFromTop (kRowH);
        if (i % 2 == 1)
        {
            g.setColour (juce::Colours::white.withAlpha (0.012f));
            g.fillRect (line);
        }
        g.setColour (ink3);
        g.drawText (juce::String (i + 1), line.withWidth (28), juce::Justification::centredRight);
        g.drawText (juce::String::charToString ((juce::juce_wchar) 0x00D7), line.withX (line.getX() + 40 + 180).withWidth (24), juce::Justification::centred);
        g.setColour (juce::Colour (0xff0d1013));
        g.drawHorizontalLine (line.getBottom() - 1, (float) area.getX(), (float) area.getRight());
    }
}

void MatrixPage::resized()
{
    auto r = getLocalBounds();
    const int rowH = MatrixFrame::kRowH;
    mod.setBounds (r.removeFromTop (PanelFrame::kTitleH + 4 + MatrixFrame::kHeadH + 16 * rowH + 6));
    r.removeFromTop (8);
    fx.setBounds (r);

    auto lay = [rowH] (Row& row, juce::Rectangle<int> line)
    {
        line.removeFromLeft (40);  // slot number (painted by the frame's paint)
        row.a->setBounds (line.removeFromLeft (180).reduced (0, 3));
        line.removeFromLeft (24);
        row.b->setBounds (line.removeFromLeft (180).reduced (0, 3));
        line.removeFromLeft (16);
        row.clear->setBounds (line.removeFromRight (26).reduced (2, 4));
        line.removeFromRight (8);
        row.dest->setBounds (line.removeFromRight (220).reduced (0, 3));
        line.removeFromRight (16);
        row.depth->setBounds (line);
        juce::ignoreUnused (rowH);
    };
    auto area = mod.content().reduced (10, 0).withTrimmedTop (26);
    for (size_t i = 0; i < 16; ++i)
        lay (rows[i], area.removeFromTop (rowH));
    auto fxArea = fx.content().reduced (10, 0).withTrimmedTop (26);
    for (size_t i = 16; i < rows.size(); ++i)
        lay (rows[i], fxArea.removeFromTop (rowH));
}
