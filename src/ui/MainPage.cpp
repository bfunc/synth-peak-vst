#include "MainPage.h"

#include "Theme.h"
#include "ui/Layout.h"

using namespace peak;

namespace
{
const ui::PanelSpec& spec (const char* key)
{
    const auto* p = ui::panel (key);
    jassert (p != nullptr);
    return *p;
}

void place (ParamWidget* c, juce::Rectangle<int> r)
{
    if (c != nullptr)
        c->setBounds (r);
}

// A knob centred in a cell.
void knobIn (ParamWidget* c, juce::Rectangle<int> cell)
{
    if (c == nullptr)
        return;
    auto* k = dynamic_cast<ParamKnob*> (c);
    const int w = juce::jmin (cell.getWidth(), k != nullptr && k->preferredHeight() > 90 ? 86 : 64);
    const int h = k != nullptr ? juce::jmin (cell.getHeight(), k->preferredHeight()) : cell.getHeight();
    c->setBounds (cell.withSizeKeepingCentre (w, h));
}

void caption (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
{
    g.setFont (theme::fontUi (10.0f));
    g.setColour (theme::ink3);
    g.drawText (text.toUpperCase(), r, juce::Justification::centred);
}
} // namespace

// ---- WaveSelector ----

WaveSelector::WaveSelector (UiContext& c, int osc) : ctx (c)
{
    const auto k = "osc" + std::to_string (osc) + "_";
    waveIdx = indexOfId (k + "wave");
    tableIdx = indexOfId (k + "wavetable");
    for (auto* b : { &prev, &next })
        addAndMakeVisible (b);
    prev.onClick = [this] { setPosition (position() - 1); };
    next.onClick = [this] { setPosition (position() + 1); };
    waveAtt = std::make_unique<juce::ParameterAttachment> (*ctx.params.parameter ((size_t) waveIdx), [this] (float) { repaint(); });
    tableAtt = std::make_unique<juce::ParameterAttachment> (*ctx.params.parameter ((size_t) tableIdx), [this] (float) { repaint(); });
}

int WaveSelector::position() const
{
    const int wave = ctx.params.value ((size_t) waveIdx);
    return wave < 4 ? wave : 4 + ctx.params.value ((size_t) tableIdx) - 4;
}

void WaveSelector::setPosition (int pos)
{
    const auto& tbl = paramTable()[(size_t) tableIdx];
    const int total = 4 + (tbl.max - tbl.min + 1);
    pos = ((pos % total) + total) % total;
    if (pos < 4)
        waveAtt->setValueAsCompleteGesture ((float) pos);
    else
    {
        waveAtt->setValueAsCompleteGesture (4.0f);
        tableAtt->setValueAsCompleteGesture ((float) (pos - 4));  // choice index
    }
}

void WaveSelector::resized()
{
    auto r = getLocalBounds();
    prev.setBounds (r.removeFromLeft (20));
    next.setBounds (r.removeFromRight (20));
}

void WaveSelector::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds().reduced (20, 0);
    g.setColour (well);
    g.fillRect (r);
    g.setColour (juce::Colours::black);
    g.drawRect (r);
    const int pos = position();
    const auto& wave = paramTable()[(size_t) waveIdx];
    const auto& tbl = paramTable()[(size_t) tableIdx];
    juce::String text = pos < 4 ? juce::String (formatValue (wave, pos)) : juce::String (formatValue (tbl, ctx.params.value ((size_t) tableIdx)));
    if (pos >= 4 && r.getWidth() > 150)  // the "More" caption only when there is room next to the name
    {
        g.setFont (fontUi (9.5f, false));
        g.setColour (ink3);
        g.drawText ("MORE", r.reduced (5, 0), juce::Justification::centredLeft);
    }
    g.setFont (fontUi (12.0f));
    g.setColour (ink);
    g.drawText (text, r, juce::Justification::centred);
}

void WaveSelector::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().reduced (20, 0).contains (e.getPosition()))
        return;
    juce::PopupMenu m, tables;
    const auto& wave = paramTable()[(size_t) waveIdx];
    const auto& tbl = paramTable()[(size_t) tableIdx];
    const int pos = position();
    for (int i = 0; i < 4; ++i)
        m.addItem (i + 1, formatValue (wave, i), true, pos == i);
    for (int i = 0; i <= tbl.max - tbl.min; ++i)
        tables.addItem (5 + i, formatValue (tbl, tbl.min + i), true, pos == 4 + i);
    m.addSubMenu ("Wavetables", tables);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        if (r > 0)
            setPosition (r - 1);
    });
}

// ---- OscPanel ----

OscPanel::OscPanel (UiContext& c, int osc) : PanelFrame (c, {}, "osc" + std::to_string (osc)), n (osc), w (c), wave (c, osc), graph (c.params, osc)
{
    std::vector<ui::Item> items;
    for (const auto& it : spec (("osc" + std::to_string (osc)).c_str()).items)
        if (it.widget != ui::Widget::Stepper)  // wave + wavetable are one WaveSelector
            items.push_back (it);
    w.add (*this, items);
    if (auto* seg = dynamic_cast<ParamSegment*> (w.get ("osc" + std::to_string (osc) + "_range")))
        seg->setColumns (2);
    addAndMakeVisible (wave);
    addAndMakeVisible (graph);
    refresh();
}

void OscPanel::paint (juce::Graphics& g)
{
    using namespace theme;
    PanelFrame::paint (g);
    auto label = getLocalBounds().withTrimmedTop (2).removeFromLeft (24);
    g.setColour (panelHi);
    g.fillRect (label);
    g.setColour (juce::Colours::black);
    g.drawVerticalLine (label.getRight(), (float) label.getY(), (float) label.getBottom());
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, (float) label.getCentreX(), (float) label.getCentreY()));
    g.setColour (ink);
    g.setFont (fontUi (12.0f));
    g.drawText ("OSC " + juce::String (n), juce::Rectangle<int> (label.getCentreX() - 50, label.getCentreY() - 8, 100, 16),
                juce::Justification::centred);
    g.restoreState();
    const auto k = "osc" + std::to_string (n) + "_";
    if (auto* r = w.get (k + "range"))
        caption (g, "Range", r->getBounds().withY (r->getY() - 16).withHeight (14));
    if (auto* e = w.get (k + "env2_pitch"))
        caption (g, "Pitch Mod", juce::Rectangle<int> (e->getX(), e->getBottom() + 2, 112, 14));
}

void OscPanel::resized()
{
    PanelFrame::resized();
    const auto k = "osc" + std::to_string (n) + "_";
    auto r = getLocalBounds().withTrimmedTop (2);
    r.removeFromLeft (24 + 10);
    r.removeFromRight (10);
    const int cy = r.getCentreY();
    auto col = [&] (int width) { auto c = r.removeFromLeft (width); r.removeFromLeft (10); return c; };

    auto range = col (74);
    place (w.get (k + "range"), juce::Rectangle<int> (range.getX(), cy - 22, 74, 46));

    auto pitch = col (112);
    knobIn (w.get (k + "coarse"), pitch.removeFromLeft (56));
    knobIn (w.get (k + "fine"), pitch);

    auto shapeCol = r.removeFromRight (4 * 50 + 10 + 112 + 10 + 112);
    auto waveCol = r;
    wave.setBounds (waveCol.getX(), waveCol.getY() + 10, waveCol.getWidth(), 22);
    graph.setBounds (waveCol.getX(), waveCol.getY() + 36, waveCol.getWidth(), waveCol.getHeight() - 46);

    shapeCol.removeFromLeft (10);
    auto shape = shapeCol.removeFromLeft (112);
    const auto shapeCell = shape.removeFromLeft (56);
    for (const char* id : { "shape", "env1_shape", "lfo1_shape" })
        knobIn (w.get (k + id), shapeCell);
    place (w.get (k + "shape_src"), shape.withSizeKeepingCentre (54, 66));
    shapeCol.removeFromLeft (10);
    auto mods = shapeCol.removeFromLeft (112).withTrimmedBottom (14);
    knobIn (w.get (k + "env2_pitch"), mods.removeFromLeft (56));
    knobIn (w.get (k + "lfo2_pitch"), mods);
    shapeCol.removeFromLeft (10);
    static const char* fm[] = { "fm_3to1", "fm_1to2", "fm_2to3" };
    const auto level = "mix_osc" + std::to_string (n);
    layoutRow (shapeCol, { w.get (k + "vsync"), w.get (k + "saw_density"), w.get (fm[(n - 1) % 3]), w.get (level) });
    for (const auto* id : { "vsync", "saw_density" })
        knobIn (w.get (k + id), w.get (k + id)->getBounds());
    knobIn (w.get (fm[(n - 1) % 3]), w.get (fm[(n - 1) % 3])->getBounds());
    knobIn (w.get (level), w.get (level)->getBounds());
}

void OscPanel::refresh()
{
    graph.refresh();
    // One Shape knob, as on the Peak: it sets the amount of whichever Shape Source is selected.
    const auto k = "osc" + std::to_string (n) + "_";
    const int src = ctx.params.value ((size_t) indexOfId (k + "shape_src"));
    if (src == lastSource)
        return;
    lastSource = src;
    w[k + "shape"].setVisible (src == 0);
    w[k + "env1_shape"].setVisible (src == 1);
    w[k + "lfo1_shape"].setVisible (src == 2);
}

// ---- MixerPanel ----

MixerPanel::MixerPanel (UiContext& c) : PanelFrame (c, "Mixer", "mixer"), w (c) { w.add (*this, spec ("mixer").items); }

void MixerPanel::resized()
{
    PanelFrame::resized();
    auto r = content().reduced (8, 6);
    const auto& items = spec ("mixer").items;
    auto row = r.withSizeKeepingCentre (r.getWidth(), 90);
    for (size_t i = 0; i < items.size(); ++i)
        knobIn (w.get (items[i].id), row.removeFromLeft (row.getWidth() / (int) (items.size() - i)));
}

// ---- FilterPanel ----

FilterPanel::FilterPanel (UiContext& c) : PanelFrame (c, "Filter", "filter"), w (c), graph (c.params)
{
    w.add (*this, spec ("filter").items);
    addAndMakeVisible (graph);
    refresh();
}

void FilterPanel::refresh()
{
    graph.refresh();
    // One Env Depth knob, as on the Peak: it sets the depth of the envelope picked by Env Src.
    const int env = ctx.params.value ((size_t) indexOfId ("flt_env_sel"));
    if (env == lastEnv)
        return;
    lastEnv = env;
    w["flt_ampenv"].setVisible (env == 0);
    w["flt_modenv1"].setVisible (env == 1);
}

void FilterPanel::paint (juce::Graphics& g)
{
    PanelFrame::paint (g);
    if (auto* e = w.get ("flt_env_sel"))
        caption (g, "Env Src", e->getBounds().withX (e->getX() - 56).withWidth (52));
}

void FilterPanel::resized()
{
    PanelFrame::resized();
    auto r = content().reduced (10, 6);
    auto left = r.removeFromLeft (196);
    r.removeFromLeft (12);
    auto segs = left.removeFromTop (24);
    place (w.get ("flt_shape"), segs.removeFromLeft (segs.getWidth() * 3 / 5).reduced (0, 1));
    segs.removeFromLeft (6);
    place (w.get ("flt_slope"), segs.reduced (0, 1));
    auto env = left.removeFromBottom (24);
    env.removeFromLeft (56);
    place (w.get ("flt_env_sel"), env.reduced (0, 1));
    graph.setBounds (left.reduced (0, 6));

    auto big = r.removeFromLeft (86);
    knobIn (w.get ("flt_freq"), big.withSizeKeepingCentre (86, 106));
    const char* grid[2][5] = { { "flt_res", "flt_overdrive", "flt_postdrive", "flt_keytrack", "flt_divergence" },
                               { "flt_ampenv", "flt_lfo1", "flt_osc3", nullptr, nullptr } };
    auto rows = r.reduced (0, 4);
    for (int row = 0; row < 2; ++row)
    {
        auto line = rows.removeFromTop (rows.getHeight() / (2 - row));
        for (int c2 = 0; c2 < 5; ++c2)
        {
            auto cell = line.removeFromLeft (line.getWidth() / (5 - c2));
            if (grid[row][c2] != nullptr)
                knobIn (w.get (grid[row][c2]), cell);
            if (grid[row][c2] != nullptr && std::string (grid[row][c2]) == "flt_ampenv")
                knobIn (w.get ("flt_modenv1"), cell);  // the same place: Env Src picks which one shows
        }
    }
}

// ---- SideTabs ----

SideTabs::SideTabs (const juce::StringArray& n, const std::vector<int>& src) : names (n)
{
    for (int i = 0; i < names.size(); ++i)
    {
        auto* d = grips.add (new DragSource (src[(size_t) i], names[i], DragSource::Style::Grip));
        addAndMakeVisible (d);
    }
}

void SideTabs::resized()
{
    for (int i = 0; i < grips.size(); ++i)
        grips[i]->setBounds (getWidth() - 20, i * kTabH + 8, 16, kTabH - 16);
}

juce::Rectangle<int> SideTabs::extraArea() const { return getLocalBounds().withTrimmedTop (names.size() * kTabH + 4); }

void SideTabs::paint (juce::Graphics& g)
{
    using namespace theme;
    g.setColour (panelHi);
    g.fillRect (getLocalBounds());
    for (int i = 0; i < names.size(); ++i)
    {
        juce::Rectangle<int> t (0, i * kTabH, getWidth(), kTabH);
        if (i == sel)
        {
            g.setColour (panel);
            g.fillRect (t);
            g.setColour (accent);
            g.fillRect (t.removeFromLeft (2));
        }
        g.setColour (juce::Colours::black);
        g.drawHorizontalLine ((i + 1) * kTabH - 1, 0.0f, (float) getWidth());
        g.setColour (i == sel ? ink : ink3);
        g.setFont (fontUi (11.0f));
        g.drawText (names[i].toUpperCase(), juce::Rectangle<int> (8, i * kTabH, getWidth() - 28, kTabH), juce::Justification::centredLeft);
    }
    g.setColour (juce::Colours::black);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
}

void SideTabs::mouseUp (const juce::MouseEvent& e)
{
    const int i = e.y / kTabH;
    if (i >= 0 && i < names.size() && i != sel)
    {
        sel = i;
        repaint();
        if (onSelect)
            onSelect (i);
    }
}

// ---- EnvPanel ----

EnvPanel::EnvPanel (UiContext& c, const UiFeed& f, std::function<bool()> on)
    : PanelFrame (c, {}, "env"), w (c), tabs ({ "Amp", "Mod 1", "Mod 2" }, { 11, 12, 13 }), graph (c.params), feed (f),
      playheadsOn (std::move (on))
{
    addAndMakeVisible (tabs);
    addAndMakeVisible (graph);
    for (const char* k : { "amp", "mod1", "mod2" })
        w.add (*this, ui::envItems (k));
    tabs.onSelect = [this] (int i) { show (i); };
    show (0);
}

void EnvPanel::show (int tab)
{
    static const char* keys[] = { "amp", "mod1", "mod2" };
    for (int i = 0; i < 3; ++i)
        w.setVisible (ui::envItems (keys[i]), i == tab);
    graph.setKey (keys[tab]);
    resized();
    repaint();
}

void EnvPanel::refresh()
{
    graph.refresh();
    const bool vis = playheadsOn && playheadsOn();
    const int h = feed.notesHeld.load();
    const uint32_t o = feed.noteOns.load();
    const double t = juce::Time::getMillisecondCounterHiRes();
    if (o != ons)
    {
        ons = o;
        flashUntil = t + 90;
    }
    if (vis != gateVisible || h != held || t < flashUntil + 40)
    {
        gateVisible = vis;
        held = h;
        repaint (tabs.getRight(), 0, getWidth() - tabs.getRight(), 24);
    }
}

void EnvPanel::paint (juce::Graphics& g)
{
    using namespace theme;
    PanelFrame::paint (g);
    static const char* titles[] = { "Amp Envelope", "Mod Envelope 1", "Mod Envelope 2" };
    auto bar = juce::Rectangle<int> (tabs.getRight() + 10, 4, getWidth() - tabs.getRight() - 20, 18);
    g.setColour (ink3);
    g.setFont (fontUi (10.5f));
    g.drawText (juce::String (titles[tabs.selected()]).toUpperCase(), bar, juce::Justification::centredLeft);
    if (gateVisible)
    {
        // Gate light: lit while notes passing through the plugin are held; flashes on each note-on.
        const bool flash = juce::Time::getMillisecondCounterHiRes() < flashUntil;
        const bool lit = held > 0 || flash;
        auto dot = juce::Rectangle<float> ((float) bar.getRight() - 64, (float) bar.getCentreY() - 4, 8, 8);
        if (lit)
        {
            g.setColour (led.withAlpha (flash ? 0.6f : 0.3f));
            g.fillEllipse (dot.expanded (flash ? 4.0f : 2.5f));
        }
        g.setColour (lit ? led : juce::Colour (0xff3a2a1f));
        g.fillEllipse (dot);
        g.setColour (ink3);
        g.drawText ("GATE" + (held > 0 ? " " + juce::String (held) : juce::String()),
                    juce::Rectangle<int> ((int) dot.getRight() + 5, bar.getY(), 52, bar.getHeight()), juce::Justification::centredLeft);
    }
}

void EnvPanel::resized()
{
    PanelFrame::resized();
    static const char* keys[] = { "amp", "mod1", "mod2" };
    auto r = getLocalBounds().withTrimmedTop (2);
    tabs.setBounds (r.removeFromLeft (72));
    auto main = r.reduced (10, 4);
    main.removeFromTop (20);
    graph.setBounds (main.removeFromTop (main.getHeight() - 90));
    main.removeFromTop (6);
    const auto k = std::string (keys[tabs.selected()]) + "_";
    layoutRow (main, { w.get (k + "attack"), w.get (k + "decay"), w.get (k + "sustain"), w.get (k + "release") });
    for (const char* id : { "attack", "decay", "sustain", "release" })
        knobIn (w.get (k + id), w.get (k + id)->getBounds());
    auto extra = tabs.getBounds().withTrimmedTop (3 * 32 + 6).reduced (4, 4);
    knobIn (w.get (k + "velocity"), extra.removeFromTop (72));
    place (w.get (k + "trigger"), extra.removeFromTop (44).withSizeKeepingCentre (60, 40));
    if (auto* seg = dynamic_cast<ParamSegment*> (w.get (k + "trigger")))
        seg->setColumns (1);
}

// ---- LfoPanel ----

LfoPanel::LfoPanel (UiContext& c, const UiFeed& f, std::function<bool()> on)
    : PanelFrame (c, {}, "lfo"), w (c), tabs ({ "LFO 1", "LFO 2", "LFO 3", "LFO 4" }, { 8, 10, 18, 20 }), graph (c.params, f, std::move (on))
{
    addAndMakeVisible (tabs);
    addAndMakeVisible (graph);
    for (int n = 1; n <= 4; ++n)
        w.add (*this, ui::lfoItems (n));
    tabs.onSelect = [this] (int i) { show (i); };
    show (0);
}

void LfoPanel::show (int tab)
{
    for (int i = 0; i < 4; ++i)
        w.setVisible (ui::lfoItems (i + 1), i == tab);
    graph.setLfo (tab + 1);
    lastRange = -1;
    refresh();
    resized();
    repaint();
}

void LfoPanel::refresh()
{
    graph.refresh();
    const int n = tabs.selected() + 1;
    if (n > 2)
        return;
    const auto k = "lfo" + std::to_string (n) + "_";
    const int range = ctx.params.value ((size_t) indexOfId (k + "range"));
    if (range != lastRange)
    {
        // Rate and Sync share one place: Sync range shows the sync rate.
        lastRange = range;
        w[k + "rate"].setVisible (range != 2);
        w[k + "sync"].setVisible (range == 2);
    }
}

void LfoPanel::resized()
{
    PanelFrame::resized();
    auto r = getLocalBounds().withTrimmedTop (2);
    tabs.setBounds (r.removeFromLeft (72));
    const int n = tabs.selected() + 1;
    const auto k = "lfo" + std::to_string (n) + "_";
    auto main = r.reduced (10, 6);
    if (n <= 2)
    {
        auto bar = main.removeFromTop (22);
        place (w.get (k + "wave"), bar.removeFromLeft (bar.getWidth() / 2 - 3));
        bar.removeFromLeft (6);
        place (w.get (k + "range"), bar);
    }
    else
        main.removeFromTop (22);
    main.removeFromTop (6);
    graph.setBounds (main.removeFromTop (main.getHeight() - 90));
    main.removeFromTop (6);
    if (n <= 2)
    {
        auto fm = main.removeFromRight (96);
        place (w.get (k + "fade_mode"), fm.withSizeKeepingCentre (96, 22).translated (0, 6));
        auto cells = main;
        auto rate = cells.removeFromLeft (cells.getWidth() / 4);
        knobIn (w.get (k + "rate"), rate);
        knobIn (w.get (k + "sync"), rate);
        layoutRow (cells, { w.get (k + "fade"), w.get (k + "phase"), w.get (k + "slew") });
        for (const char* id : { "fade", "phase", "slew" })
            knobIn (w.get (k + id), w.get (k + id)->getBounds());
        auto extra = tabs.getBounds().withTrimmedTop (4 * 32 + 10).reduced (8, 0);
        place (w.get (k + "oneshot"), extra.removeFromTop (22));
        place (w.get (k + "common"), extra.removeFromTop (22));
    }
    else
    {
        layoutRow (main.removeFromLeft (main.getWidth() * 3 / 4), { w.get (k + "fade"), w.get (k + "phase"), w.get (k + "slew") });
        for (const char* id : { "fade", "phase", "slew" })
            knobIn (w.get (k + id), w.get (k + id)->getBounds());
    }
}

// ---- VoicePanel / ArpPanel ----

VoicePanel::VoicePanel (UiContext& c) : PanelFrame (c, "Voice", "voice"), w (c) { w.add (*this, spec ("voice").items); }

void VoicePanel::resized()
{
    PanelFrame::resized();
    auto r = content().reduced (8, 4);
    place (w.get ("voice_mode"), r.removeFromTop (24).reduced (0, 1));
    place (w.get ("glide_on"), r.removeFromTop (24).reduced (0, 2));
    layoutRow (r, { w.get ("unison"), w.get ("unison_detune"), w.get ("spread"), w.get ("glide_time") });
    for (const char* id : { "unison", "unison_detune", "spread", "glide_time" })
        knobIn (w.get (id), w.get (id)->getBounds());
}

ArpPanel::ArpPanel (UiContext& c) : PanelFrame (c, "Arp", "arp"), w (c) { w.add (*this, spec ("arp").items); }

void ArpPanel::resized()
{
    PanelFrame::resized();
    auto r = content().reduced (8, 4);
    layoutRow (r.removeFromTop (24), { w.get ("arp_on"), w.get ("arp_latch"), w.get ("arp_keysync") });
    auto ch = r.removeFromTop (26).reduced (0, 2);
    place (w.get ("arp_type"), ch.removeFromLeft (ch.getWidth() / 2 - 3));
    ch.removeFromLeft (6);
    place (w.get ("arp_sync"), ch);
    layoutRow (r, { w.get ("arp_gate"), w.get ("arp_swing"), w.get ("arp_octaves"), w.get ("arp_chance") });
    for (const char* id : { "arp_gate", "arp_swing", "arp_octaves", "arp_chance" })
        knobIn (w.get (id), w.get (id)->getBounds());
}

// ---- SourceRail ----

SourceRail::SourceRail()
{
    struct S { int src; const char* label; DragSource::Style style; const char* pad; };
    const S list[] = { { 14, "Animate 1", DragSource::Style::Pad, "1" }, { 15, "Animate 2", DragSource::Style::Pad, "2" },
                       { 1, "Mod Wheel", DragSource::Style::Meter, "" }, { 2, "Aftertouch", DragSource::Style::Meter, "" },
                       { 5, "Velocity", DragSource::Style::Meter, "" }, { 3, "Expression", DragSource::Style::Meter, "" },
                       { 6, "Keyboard", DragSource::Style::Meter, "" } };
    for (const auto& s : list)
        addAndMakeVisible (sources.add (new DragSource (s.src, s.label, s.style, s.pad)));
}

void SourceRail::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (20);
    for (auto* s : sources)
    {
        const bool pad = s == sources[0] || s == sources[1];
        s->setBounds (r.removeFromTop (pad ? 78 : 58));
        r.removeFromTop (6);
    }
}

void SourceRail::paint (juce::Graphics& g)
{
    using namespace theme;
    g.setFont (fontUi (10.0f));
    g.setColour (ink3);
    g.drawText ("SOURCES", getLocalBounds().removeFromTop (18), juce::Justification::centred);
    auto hint = getLocalBounds().removeFromBottom (48);
    g.setFont (fontUi (10.0f, false));
    g.drawFittedText ("Drag a source onto a knob. Orange ring = modulated.", hint, juce::Justification::centred, 4);
}

// ---- MainPage ----

MainPage::MainPage (UiContext& c, const UiFeed& f, std::function<bool()> on)
    : mixer (c), filter (c), env (c, f, on), lfo (c, f, on), voice (c), arp (c)
{
    addAndMakeVisible (rail);
    for (int i = 0; i < 3; ++i)
    {
        oscs[(size_t) i] = std::make_unique<OscPanel> (c, i + 1);
        addAndMakeVisible (*oscs[(size_t) i]);
    }
    for (juce::Component* p : { (juce::Component*) &mixer, (juce::Component*) &filter, (juce::Component*) &env,
                                (juce::Component*) &lfo, (juce::Component*) &voice, (juce::Component*) &arp })
        addAndMakeVisible (p);
}

void MainPage::refresh()
{
    for (auto& o : oscs)
        o->refresh();
    filter.refresh();
    env.refresh();
    lfo.refresh();
}

void MainPage::resized()
{
    // Columns of the mockup: 76 | 812 | rest, 8 px gaps.
    auto r = getLocalBounds();
    rail.setBounds (r.removeFromLeft (76));
    r.removeFromLeft (8);
    auto centre = r.removeFromLeft (812);
    r.removeFromLeft (8);
    for (auto& o : oscs)
    {
        o->setBounds (centre.removeFromTop (136));
        centre.removeFromTop (8);
    }
    mixer.setBounds (centre.removeFromLeft (196));
    centre.removeFromLeft (8);
    filter.setBounds (centre);

    env.setBounds (r.removeFromTop (262));
    r.removeFromTop (8);
    lfo.setBounds (r.removeFromTop (266));
    r.removeFromTop (8);
    voice.setBounds (r.removeFromLeft (r.getWidth() / 2 - 4));
    r.removeFromLeft (8);
    arp.setBounds (r);
}
