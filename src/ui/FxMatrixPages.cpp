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

void lineCaption (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
{
    const auto f = theme::fontUi (9.5f).withExtraKerningFactor (0.1f);
    const int tw = (int) juce::GlyphArrangement::getStringWidth (f, text.toUpperCase()) + 12;
    g.setColour (theme::edgeHi);
    g.drawHorizontalLine (r.getCentreY(), (float) r.getX(), (float) (r.getCentreX() - tw / 2));
    g.drawHorizontalLine (r.getCentreY(), (float) (r.getCentreX() + tw / 2), (float) r.getRight());
    g.setFont (f);
    g.setColour (theme::ink3);
    g.drawText (text.toUpperCase(), r, juce::Justification::centred);
}

// FX Routing values in the manual's order (to confirm on the synth).
const char* kRoutes[] = { "Parallel", "D > R > C", "D > C > R", "R > D > C", "R > C > D", "C > D > R", "C > R > D" };
const char* kRouteOrder[][3] = { { "", "", "" }, { "DEL", "REV", "CHO" }, { "DEL", "CHO", "REV" }, { "REV", "DEL", "CHO" },
                                 { "REV", "CHO", "DEL" }, { "CHO", "DEL", "REV" }, { "CHO", "REV", "DEL" } };
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
    PanelFrame::paint (g);
    g.setColour (theme::led);
    const float tw = juce::GlyphArrangement::getStringWidth (theme::fontUi (12.0f).withExtraKerningFactor (0.1f), title.toUpperCase());
    g.fillEllipse (8 + tw + 8, 2 + kTitleH / 2.0f - 3.5f, 7, 7);
    for (const auto& gr : groups)
        lineCaption (g, gr.first, gr.second);
}

juce::Rectangle<int> FxRow::span (juce::Rectangle<int> grid, int from, int to)
{
    constexpr int gap = 6;
    const float cw = (grid.getWidth() - 9 * gap) / 10.0f;
    const int x0 = grid.getX() + (int) ((from - 1) * (cw + gap));
    const int x1 = grid.getX() + (int) ((to - 1) * (cw + gap) - gap);
    return { x0, grid.getY(), x1 - x0, grid.getHeight() };
}

void FxRow::resized()
{
    PanelFrame::resized();
    auto r = content().reduced (16, 10);
    graph.setBounds (r.removeFromLeft (220));
    r.removeFromLeft (24);
    auto grid = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 104));
    groups.clear();
    auto group = [&] (const char* caption, int from, int to, std::vector<juce::Component*> items)
    {
        auto a = span (grid, from, to);
        groups.push_back ({ caption, a.removeFromTop (14) });
        a.removeFromTop (4);
        const int n = (int) items.size();
        for (int i = 0; i < n; ++i)
        {
            auto cell = a.removeFromLeft (a.getWidth() / (n - i));
            auto* c = items[(size_t) i];
            if (dynamic_cast<ParamKnob*> (c) != nullptr)
                knobIn (c, cell);
            else if (c != nullptr)
                c->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 6, 100), juce::jmin (cell.getHeight(), 66)));
        }
    };
    auto wg = [this] (const char* id) { return (juce::Component*) w.get (id); };
    if (key == "chorus")
    {
        group ("Modulation", 1, 4, { wg ("cho_rate"), wg ("cho_depth"), wg ("cho_feedback") });
        group ("Mode / Type", 4, 8, { wg ("cho_mode"), wg ("cho_type") });
        group ("Tone", 8, 10, { wg ("cho_lp"), wg ("cho_hp") });
        group ("Output", 10, 11, { wg ("cho_level") });
    }
    else if (key == "delay")
    {
        auto head = juce::Rectangle<int> (getWidth() - 28 - 70, 4, 70, 18);
        if (auto* s = w.get ("dly_sync"))
            s->setBounds (head);
        if (auto* m = w.get ("dly_time_mode"))
            m->setBounds (head.getX() - 8 - 110, 3, 110, 20);
        group ("Time", 1, 4, { wg ("dly_time"), wg ("dly_feedback"), wg ("dly_slew") });
        if (auto* t = w.get ("dly_time"))
            w["dly_sync_time"].setBounds (t->getBounds());
        group ("Stereo", 4, 6, { wg ("dly_width") });
        group ("Damping", 6, 8, { wg ("dly_lp_damp"), wg ("dly_hp_damp") });
        group ("Output", 10, 11, { wg ("dly_level") });
    }
    else if (key == "reverb")
    {
        if (auto* t = w.get ("rev_type"))
            t->setBounds (getWidth() - 28 - 96, 4, 96, 18);
        group ("Space", 1, 4, { wg ("rev_time"), wg ("rev_size"), wg ("rev_predelay") });
        group ("Movement", 4, 6, { wg ("rev_mod"), wg ("rev_mod_rate") });
        group ("Damping", 6, 8, { wg ("rev_damp_lp"), wg ("rev_damp_hp") });
        group ("EQ", 8, 10, { wg ("rev_lowpass"), wg ("rev_highpass") });
        group ("Output", 10, 11, { wg ("rev_level") });
    }
}

// ---- RoutingView ----

RoutingView::RoutingView (UiContext& c) : ctx (c), idx (indexOfId ("fx_routing"))
{
    for (auto* b : { &prev, &next })
        addAndMakeVisible (b);
    prev.onClick = [this] { setValue (value() - 1); };
    next.onClick = [this] { setValue (value() + 1); };
    att = std::make_unique<juce::ParameterAttachment> (*ctx.params.parameter ((size_t) idx), [this] (float) { repaint(); });
    setTooltip (juce::String::fromUTF8 ("Routing: chorus, delay and reverb in parallel, or in series in any order. Delay before reverb gives each echo its own reverb tail."));
}

int RoutingView::value() const { return ctx.params.value ((size_t) idx); }

void RoutingView::setValue (int v)
{
    const auto& p = paramTable()[(size_t) idx];
    const int n = p.max - p.min + 1;
    v = ((v - p.min) % n + n) % n + p.min;
    att->setValueAsCompleteGesture ((float) v);
}

void RoutingView::resized()
{
    prev.setBounds (0, 16, 22, 24);
    next.setBounds (getWidth() - 22, 16, 22, 24);
}

void RoutingView::mouseUp (const juce::MouseEvent& e)
{
    if (! juce::Rectangle<int> (22, 16, getWidth() - 44, 24).contains (e.getPosition()))
        return;
    juce::PopupMenu m;
    for (int i = 0; i < 7; ++i)
        m.addItem (i + 1, kRoutes[i], true, value() == i);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        if (r > 0)
            setValue (r - 1);
    });
}

void RoutingView::paint (juce::Graphics& g)
{
    using namespace theme;
    const int v = juce::jlimit (0, 6, value());
    g.setFont (fontUi (9.5f).withExtraKerningFactor (0.1f));
    g.setColour (ink3);
    g.drawText ("ROUTING", 0, 0, getWidth(), 14, juce::Justification::centredLeft);
    auto name = juce::Rectangle<int> (22, 16, getWidth() - 44, 24);
    g.setColour (well);
    g.fillRect (name);
    g.setColour (juce::Colours::black);
    g.drawRect (name);
    g.setColour (ink);
    g.setFont (fontUi (12.5f));
    g.drawText (kRoutes[v], name, juce::Justification::centred);

    // Flow picture, top to bottom.
    auto box = getLocalBounds().withTrimmedTop (50).toFloat();
    g.setColour (well);
    g.fillRoundedRectangle (box, 2.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (box.reduced (0.5f), 2.0f, 1.0f);
    const auto wire = ink3;
    auto node = [&] (juce::Rectangle<float> r, const char* text, bool fx)
    {
        g.setColour (panelHi);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (fx ? accent : edgeHi);
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
        g.setColour (fx ? ink : ink2);
        g.setFont (fontUi (10.0f).withExtraKerningFactor (0.08f));
        g.drawText (text, r, juce::Justification::centred);
    };
    const float cx = box.getCentreX(), nw = 46, nh = 20;
    const float top = box.getCentreY() - (v == 0 ? 74 : 112);
    auto at = [&] (float y) { return juce::Rectangle<float> (cx - nw / 2, y, nw, nh); };
    g.setColour (wire);
    if (v == 0)
    {
        auto in = at (top), out = at (top + 128);
        const float y1 = top + nh + 22, y2 = y1 + 16 + nh + 16;
        g.drawVerticalLine ((int) cx, in.getBottom(), y1);
        g.drawHorizontalLine ((int) y1, cx - 56, cx + 56);
        g.drawHorizontalLine ((int) y2, cx - 56, cx + 56);
        g.drawVerticalLine ((int) cx, y2, out.getY());
        const char* names[] = { "CHO", "DEL", "REV" };
        for (int i = 0; i < 3; ++i)
        {
            const float x = cx + (i - 1) * 56.0f;
            g.setColour (wire);
            g.drawVerticalLine ((int) x, y1, y2);
            node ({ x - 24, y1 + 16, 48, nh }, names[i], true);
        }
        node (in, "IN", false);
        node (out, "OUT", false);
    }
    else
    {
        float y = top;
        g.drawVerticalLine ((int) cx, top + nh, top + 4 * 46.0f);
        node (at (y), "IN", false);
        for (int i = 0; i < 3; ++i)
        {
            y += 46;
            node (at (y), kRouteOrder[v][i], true);
        }
        node (at (y + 46), "OUT", false);
    }
}

// ---- FxGlobal / FxPage ----

FxGlobal::FxGlobal (UiContext& c) : PanelFrame (c, "FX Global", "fx"), w (c), routing (c)
{
    std::vector<ui::Item> items;
    for (const auto& it : ui::panel ("fx")->items)
        if (it.id != "fx_routing")
            items.push_back (it);
    w.add (*this, items);
    addAndMakeVisible (routing);
}

void FxGlobal::paint (juce::Graphics& g) { PanelFrame::paint (g); }

void FxGlobal::resized()
{
    PanelFrame::resized();
    if (auto* b = w.get ("fx_bypass"))
        b->setBounds (getWidth() - 28 - 76, 4, 76, 18);
    auto r = content().reduced (12, 10);
    auto dw = r.removeFromBottom (84);
    knobIn (w.get ("fx_dry"), dw.removeFromLeft (dw.getWidth() / 2));
    knobIn (w.get ("fx_wet"), dw);
    r.removeFromBottom (10);
    routing.setBounds (r);
}

FxPage::FxPage (UiContext& c) : global (c)
{
    addAndMakeVisible (global);
    rows[0] = std::make_unique<FxRow> (c, "chorus", "Chorus", FxGraph::Type::Chorus);
    rows[1] = std::make_unique<FxRow> (c, "delay", "Delay", FxGraph::Type::Delay);
    rows[2] = std::make_unique<FxRow> (c, "reverb", "Reverb", FxGraph::Type::Reverb);
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
    // The mock: FX Global (232) | a rack of three equal rows.
    auto r = getLocalBounds();
    global.setBounds (r.removeFromLeft (232));
    r.removeFromLeft (8);
    const int h = (r.getHeight() - 16) / 3;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        rows[i]->setBounds (i == 2 ? r : r.removeFromTop (h));
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
