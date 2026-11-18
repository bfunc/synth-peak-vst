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

void place (juce::Component* c, juce::Rectangle<int> r)
{
    if (c != nullptr)
        c->setBounds (r);
}

// A knob centred in a cell.
void knobIn (juce::Component* c, juce::Rectangle<int> cell)
{
    if (c == nullptr)
        return;
    auto* k = dynamic_cast<ParamKnob*> (c);
    const int pref = k != nullptr ? k->preferredHeight() : cell.getHeight();
    const int w = juce::jmin (cell.getWidth(), pref > 90 ? 86 : (pref < 75 ? 56 : 64));
    const int h = juce::jmin (cell.getHeight(), pref);
    c->setBounds (cell.withSizeKeepingCentre (w, h));
}

// Knobs spread evenly across a row.
void knobRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items)
{
    const int n = (int) items.size();
    for (int i = 0; i < n; ++i)
        knobIn (items[(size_t) i], area.removeFromLeft (area.getWidth() / (n - i)));
}

void caption (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r,
              juce::Justification j = juce::Justification::centredLeft)
{
    g.setFont (theme::fontUi (9.5f).withExtraKerningFactor (0.1f));
    g.setColour (theme::ink3);
    g.drawText (text.toUpperCase(), r, j);
}

// A group caption with a thin line on each side: ─── CUTOFF ───
void lineCaption (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
{
    const auto f = theme::fontUi (9.5f).withExtraKerningFactor (0.1f);
    const int tw = (int) juce::GlyphArrangement::getStringWidth (f, text.toUpperCase()) + 12;
    g.setColour (theme::edgeHi);
    const int y = r.getCentreY();
    g.drawHorizontalLine (y, (float) r.getX(), (float) (r.getCentreX() - tw / 2));
    g.drawHorizontalLine (y, (float) (r.getCentreX() + tw / 2), (float) r.getRight());
    g.setFont (f);
    g.setColour (theme::ink3);
    g.drawText (text.toUpperCase(), r, juce::Justification::centred);
}

// A small outlined label at the right of a header (AMP ENVELOPE, PER VOICE).
void badge (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> header)
{
    const auto f = theme::fontUi (9.5f).withExtraKerningFactor (0.1f);
    const int w = (int) juce::GlyphArrangement::getStringWidth (f, text.toUpperCase()) + 14;
    auto b = juce::Rectangle<int> (header.getRight() - 8 - w, header.getCentreY() - 8, w, 16);
    g.setColour (theme::edgeHi);
    g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 2.0f, 1.0f);
    g.setFont (f);
    g.setColour (theme::ink3);
    g.drawText (text.toUpperCase(), b, juce::Justification::centred);
}

// "Wired to …": what an envelope or LFO drives without the matrix.
void wiredNote (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
{
    juce::AttributedString s;
    s.append ("Wired to ", theme::fontBody (11.0f), theme::ink2);
    s.append (text, theme::fontBody (11.0f), theme::ink3);
    s.setJustification (juce::Justification::centredLeft);
    s.setWordWrap (juce::AttributedString::byWord);
    s.draw (g, r.toFloat());
}

// ---- wave picker ----

// Short descriptions of the factory wavetables (third-party, docs/WAVETABLES.md), in the manual's order.
const char* kTableInfo[60] = {
    "soft organ-like tone evolving to a harsh square-like wave", "random harmonics, harsh and dissonant, PPG-like",
    "almost like FM noise, very harsh and bass-like", "bell-like at the ends, vocal-like in the middle",
    "the first two harmonics shift up an octave for each next wave", "from a square wave to FM noise",
    "crystalline, glassy tones that change chords across the waves", "PPG-vox-like; sweeping it fast gives an \"Eee-ooo\"",
    "square-like to harsh to bell-like", "soft to harsh tonewheel organ", "more Wurlitzer than DX7, a bit vocal",
    "formant \"ooo-ee\", PPG-like", "formant \"yah-ee\"", "wind-instrument waves, voxy at the end",
    "clavinet-ish, vocal or FM-like", "bowed-string-like, formant-heavy", "buzzy organ with a bell wave in the middle",
    "squeezy, alien, formant-like", "vocal to bright", "soft to glassy, Shepard-tone-like", "chord-like vocal",
    "starts as a saw, becomes chord-like", "analogue-like, harsh to soft", "muffled formant, a submerged choir",
    "soft square-like to harsh e-piano-like", "formant, thin and buzzy", "phased, harmonised saw, Animoog-like",
    "thin, like water swishing", "soft tone that whistles like throat singing", "bell-like tone becoming a muffled formant",
    "bright and glassy to soft, piano-like", "chime-like becoming vocal", "glassy, organ-like",
    "piano-like becoming bright and glassy", "analogue mixed with FM / formant", "glassy formant",
    "e-piano-like to PPG bell", "formant to chime-like", "digital, very vocal", "almost all bell or chime",
    "pleasant bell-like", "square or FM-like, odd harmonics", "thin, becoming glassy", "chime, formant and organ mix",
    "more bell than organ", "saw-like, then dissonant chords", "soft square-like to chime", "shimmering organ",
    "thin and glassy", "piano-like becoming glassy", "bell to metallic steel", "square-like to e-piano to chimey organ",
    "bright PPG / organ-like", "thin to full bell, good for percussion", "chords with analogue-like waves, chiptuney",
    "bell-like", "PPG-like formant", "e-piano with vocal undertones", "thin glassy vox, more bell", "formant \"yeah\"" };

// Groups by sound: a judgement call from those descriptions (the 1-60 view keeps the hardware order).
struct Group { const char* name; std::vector<const char*> tables; };
const std::vector<Group>& groups()
{
    static const std::vector<Group> g = {
        { "Soft / Organ", { "BS Sine", "Octaves", "Organ", "BassOrgan", "Carousel", "Sunrise", "Shmorgan", "Dub", "Monster", "Vowelled" } },
        { "Bell / Glassy", { "Tubey", "Chords", "Glassy", "Granular", "Grime", "Grow", "GlassSaw", "Hungry", "Modeling", "Modem",
                             "Steels", "Thicker", "Thinner", "Tides", "Tops", "Variance", "V.Chord", "Swell" } },
        { "Vocal / Formant", { "Didgery", "VoxOooEe", "VoxYahEe", "Winds", "String", "Choral", "Deep", "Eee", "Hedge", "Lead",
                               "Vocaloid", "WeirdVox", "Yeah", "Ladders", "Buzzy", "SeaBase" } },
        { "Harsh / Buzzy", { "Random", "Zing", "Wobbler", "Harsh", "E.Piano", "SoftClav", "Acid", "Screech", "Flame", "Further",
                             "Tokyo", "Heavy" } },
        { "Analog / Saw-like", { "Climbing", "CoinFlip", "Eris", "Spirals" } } };
    return g;
}

class Chip : public juce::Component
{
public:
    Chip (juce::String t, int num, bool selected) : text (std::move (t)), number (num), on (selected)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    std::function<void()> onClick;
    std::function<void (bool)> onHover;

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        auto r = getLocalBounds().toFloat();
        g.setColour (on ? knobHi : (isMouseOver() ? panelHi : panel));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (isMouseOver() ? edgeHi : juce::Colour (0xff0b0d10));
        g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);
        if (on)
        {
            g.setColour (led);
            g.fillRect (r.removeFromBottom (2.0f));
        }
        auto t = getLocalBounds().reduced (6, 0);
        if (number > 0)
        {
            g.setFont (fontMono (9.0f));
            g.setColour (ink3);
            g.drawText (juce::String (number), t.removeFromLeft (16), juce::Justification::centredLeft);
        }
        g.setFont (fontUi (11.0f, false));
        g.setColour (on || isMouseOver() ? ink : ink2);
        g.drawFittedText (text, t, juce::Justification::centredLeft, 1, 0.8f);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); if (onHover) onHover (true); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); if (onHover) onHover (false); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && onClick)
            onClick();
    }

private:
    juce::String text;
    int number;
    bool on;
};

class WavePicker : public juce::Component
{
public:
    WavePicker (int osc, int current, const juce::StringArray& basics, const juce::StringArray& tables,
                std::function<void (int)> choose)
        : n (osc), cur (current), basic (basics), names (tables), set (std::move (choose))
    {
        for (auto* b : { &bySoundBtn, &numberBtn })
        {
            b->getProperties().set ("segment", true);
            b->setClickingTogglesState (false);
            addAndMakeVisible (b);
        }
        bySoundBtn.onClick = [this] { bySound() = true; rebuild(); };
        numberBtn.onClick = [this] { bySound() = false; rebuild(); };
        rebuild();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.fillAll (juce::Colour (0xff1a1f25));
        g.setColour (led);
        g.fillRect (0, 0, getWidth(), 2);
        g.setColour (ink);
        g.setFont (fontUi (11.5f).withExtraKerningFactor (0.1f));
        g.drawText ("OSC " + juce::String (n) + " WAVE", 10, 4, 200, 26, juce::Justification::centredLeft);
        for (const auto& c : captions)
            lineCaption (g, c.first, c.second);
        auto info = getLocalBounds().removeFromBottom (30).reduced (10, 0);
        g.setColour (juce::Colour (0xff0b0d10));
        g.drawHorizontalLine (info.getY(), 0.0f, (float) getWidth());
        g.setFont (fontBody (11.0f));
        g.setColour (hoverText.isNotEmpty() ? ink2 : ink3);
        g.drawFittedText (hoverText.isNotEmpty() ? hoverText : juce::String ("Hover a wavetable to read about it. The arrows next to the name step one at a time."),
                          info, juce::Justification::centredLeft, 2, 0.85f);
    }

    void resized() override
    {
        numberBtn.setBounds (getWidth() - 10 - 56, 8, 56, 20);
        bySoundBtn.setBounds (numberBtn.getX() - 76, 8, 76, 20);
    }

private:
    static bool& bySound()
    {
        static bool b = true;
        return b;
    }

    void rebuild()
    {
        chips.clear();
        captions.clear();
        bySoundBtn.setToggleState (bySound(), juce::dontSendNotification);
        numberBtn.setToggleState (! bySound(), juce::dontSendNotification);
        constexpr int cols = 7, chipH = 22, gap = 3, w = 612;
        const int cw = (w - 20 - gap * (cols - 1)) / cols;
        int y = 38;
        auto section = [&] (const juce::String& title, const std::vector<int>& positions)
        {
            captions.push_back ({ title, juce::Rectangle<int> (10, y, w - 20, 14) });
            y += 17;
            for (size_t i = 0; i < positions.size(); ++i)
            {
                const int pos = positions[i];
                const bool table = pos >= 4;
                const int idx = pos - 4;
                auto* c = chips.add (new Chip (table ? names[idx] : basic[pos], table ? idx + 1 : 0, pos == cur));
                c->setBounds (10 + (int) (i % cols) * (cw + gap), y + (int) (i / cols) * (chipH + gap), cw, chipH);
                c->onClick = [this, pos]
                {
                    set (pos);
                    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                        box->dismiss();
                };
                c->onHover = [this, pos, table, idx] (bool in)
                {
                    hoverText = ! in ? juce::String()
                              : table ? "Wavetable " + juce::String (idx + 1) + " " + juce::String::charToString ((juce::juce_wchar) 0x00B7) + " "
                                            + names[idx] + ": " + juce::String (idx < 60 ? kTableInfo[idx] : "a user wavetable") + "."
                                      : basic[pos] + (pos == 3 ? ": Shape sets the pulse width." : ": classic wave; Shape adds harmonics.");
                    repaint (getLocalBounds().removeFromBottom (30));
                };
                addAndMakeVisible (c);
            }
            y += (int) ((positions.size() + cols - 1) / cols) * (chipH + gap) + 6;
        };
        section ("Basic", { 0, 1, 2, 3 });
        if (bySound())
        {
            for (const auto& gr : groups())
            {
                std::vector<int> pos;
                for (const char* t : gr.tables)
                    if (const int i = names.indexOf (t); i >= 0)
                        pos.push_back (4 + i);
                section (gr.name, pos);
            }
        }
        else
        {
            std::vector<int> pos;
            for (int i = 0; i < juce::jmin (60, names.size()); ++i)
                pos.push_back (4 + i);
            section ("Wavetables 1-60", pos);
        }
        setSize (w, y + 30);
        repaint();
    }

    int n, cur;
    juce::StringArray basic, names;
    std::function<void (int)> set;
    juce::TextButton bySoundBtn { "By sound" }, numberBtn { "1-60" };
    juce::OwnedArray<Chip> chips;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> captions;
    juce::String hoverText;
};

// Fixed Note + Bend Range, from the cog next to "OSC n" (menu-only settings on the Peak).
class OscSettings : public juce::Component
{
public:
    OscSettings (UiContext& c, int osc) : n (osc)
    {
        const auto k = "osc" + std::to_string (osc) + "_";
        fixed = makeWidget (c, { ui::Widget::Knob, k + "fixed_note", "Fixed Note" });
        bend = makeWidget (c, { ui::Widget::Knob, k + "bend", "Bend" });
        addAndMakeVisible (*fixed);
        addAndMakeVisible (*bend);
        setSize (200, 132);
    }
    void paint (juce::Graphics& g) override
    {
        using namespace theme;
        g.fillAll (juce::Colour (0xff1a1f25));
        g.setColour (accent);
        g.fillRect (0, 0, getWidth(), 2);
        g.setColour (ink);
        g.setFont (fontUi (11.5f).withExtraKerningFactor (0.1f));
        g.drawText ("OSC " + juce::String (n) + " SETTINGS", 10, 4, 180, 20, juce::Justification::centredLeft);
        g.setColour (ink3);
        g.setFont (fontBody (10.5f));
        g.drawText ("From the Peak's Osc menu, saved with the patch.", 10, getHeight() - 20, 190, 16, juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().withTrimmedTop (26).withTrimmedBottom (22);
        knobIn (fixed.get(), r.removeFromLeft (r.getWidth() / 2));
        knobIn (bend.get(), r);
    }

private:
    int n;
    std::unique_ptr<ParamWidget> fixed, bend;
};
} // namespace

// ---- WaveCell ----

WaveCell::WaveCell (UiContext& c, int osc) : ctx (c), n (osc), icon (c.params, osc)
{
    const auto k = "osc" + std::to_string (osc) + "_";
    waveIdx = indexOfId (k + "wave");
    tableIdx = indexOfId (k + "wavetable");
    for (auto* b : { &prev, &next })
        addAndMakeVisible (b);
    addAndMakeVisible (icon);
    icon.setInterceptsMouseClicks (false, false);
    prev.onClick = [this] { setPosition (position() - 1); };
    next.onClick = [this] { setPosition (position() + 1); };
    prev.setTooltip ("Previous wave.");
    next.setTooltip ("Next wave.");
    waveAtt = std::make_unique<juce::ParameterAttachment> (*ctx.params.parameter ((size_t) waveIdx), [this] (float) { repaint(); });
    tableAtt = std::make_unique<juce::ParameterAttachment> (*ctx.params.parameter ((size_t) tableIdx), [this] (float) { repaint(); });
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int WaveCell::position() const
{
    const int wave = ctx.params.value ((size_t) waveIdx);
    return wave < 4 ? wave : 4 + ctx.params.value ((size_t) tableIdx) - paramTable()[(size_t) tableIdx].min;
}

void WaveCell::setPosition (int pos)
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

juce::Rectangle<int> WaveCell::nameArea() const { return { 20, 0, getWidth() - 40, 22 }; }

void WaveCell::resized()
{
    prev.setBounds (0, 0, 20, 22);
    next.setBounds (getWidth() - 20, 0, 20, 22);
    icon.setBounds (0, 26, getWidth(), 38);
}

void WaveCell::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = nameArea();
    g.setColour (well);
    g.fillRect (r);
    g.setColour (juce::Colours::black);
    g.drawRect (r);
    const int pos = position();
    const auto& wave = paramTable()[(size_t) waveIdx];
    const auto& tbl = paramTable()[(size_t) tableIdx];
    const juce::String text = pos < 4 ? juce::String (formatValue (wave, pos))
                                      : juce::String (formatValue (tbl, ctx.params.value ((size_t) tableIdx)));
    const bool hot = isMouseOver (false) && r.contains (getMouseXYRelative());
    const auto f = fontUi (12.0f);
    const int tw = (int) juce::GlyphArrangement::getStringWidth (f, text);
    g.setFont (f);
    g.setColour (hot ? accent : ink);
    auto tr = r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 14, tw + 14), r.getHeight());
    g.drawText (text, tr.withTrimmedRight (12), juce::Justification::centred);
    juce::Path tri;
    const float cx = (float) tr.getRight() - 5, cy = (float) r.getCentreY();
    tri.addTriangle (cx - 3.5f, cy - 2, cx + 3.5f, cy - 2, cx, cy + 2.5f);
    g.setColour (ink3);
    g.fillPath (tri);
}

void WaveCell::mouseUp (const juce::MouseEvent& e)
{
    if (! nameArea().contains (e.getPosition()))
        return;
    const auto& wave = paramTable()[(size_t) waveIdx];
    const auto& tbl = paramTable()[(size_t) tableIdx];
    juce::StringArray basic, names;
    for (int i = 0; i < 4; ++i)
        basic.add (juce::String (formatValue (wave, i)));
    for (const auto& s : tbl.choices)
        names.add (juce::String (s));
    auto picker = std::make_unique<WavePicker> (n, position(), basic, names, [safe = juce::Component::SafePointer<WaveCell> (this)] (int pos)
    {
        if (safe != nullptr)
            safe->setPosition (pos);
    });
    juce::CallOutBox::launchAsynchronously (std::move (picker), localAreaToGlobal (nameArea()), nullptr);
}

// ---- CogButton ----

void CogButton::paintButton (juce::Graphics& g, bool over, bool)
{
    using namespace theme;
    const auto c = getLocalBounds().toFloat().getCentre();
    const float s = juce::jmin (getWidth(), getHeight()) / 12.0f;
    g.setColour (lit ? accent : (over ? ink : ink3));
    g.drawEllipse (c.x - 2.6f * s, c.y - 2.6f * s, 5.2f * s, 5.2f * s, 1.3f * s);
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) i * juce::MathConstants<float>::pi / 4.0f;
        const auto p1 = c.getPointOnCircumference (3.3f * s, a), p2 = c.getPointOnCircumference (5.2f * s, a);
        g.drawLine ({ p1, p2 }, 1.3f * s);
    }
}

// ---- OscRow ----

OscRow::OscRow (UiContext& c, int osc) : ctx (c), n (osc), w (c), wave (c, osc)
{
    std::vector<ui::Item> items;
    for (const auto& it : ui::oscItems (osc))
    {
        const bool popout = it.id.size() > 10 && (it.id.compare (it.id.size() - 10, 10, "fixed_note") == 0);
        const bool bend = it.id.size() >= 4 && it.id.compare (it.id.size() - 4, 4, "bend") == 0;
        if (it.widget != ui::Widget::Stepper && ! popout && ! bend)  // wave + wavetable are the WaveCell
            items.push_back (it);
    }
    w.add (*this, items);
    addAndMakeVisible (wave);
    addAndMakeVisible (cog);
    cog.onClick = [this]
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<OscSettings> (ctx, n), cog.getScreenBounds(), nullptr);
    };
    refresh();
}

void OscRow::refresh()
{
    wave.iconRef().refresh();
    const auto k = "osc" + std::to_string (n) + "_";
    auto val = [&] (const std::string& id) { return ctx.params.value ((size_t) indexOfId (k + id)); };

    // One Shape knob, as on the Peak: it sets the amount of whichever Shape Source is selected.
    const int src = val ("shape_src");
    if (src != lastSource)
    {
        lastSource = src;
        w[k + "shape"].setVisible (src == 0);
        w[k + "env1_shape"].setVisible (src == 1);
        w[k + "lfo1_shape"].setVisible (src == 2);
        repaint();  // the hint under the Shape knob
    }
    // Saw density / detune only with the saw wave; detune greyed while density is 0.
    const int wv = val ("wave");
    if (wv != lastWave)
    {
        lastWave = wv;
        w[k + "saw_density"].setVisible (wv == 2);
        w[k + "saw_detune"].setVisible (wv == 2);
        resized();
        repaint();  // the hint under the Shape knob
    }
    const int dens = val ("saw_density");
    if (dens != lastDensity)
    {
        lastDensity = dens;
        if (auto* s = dynamic_cast<ParamSlim*> (w.get (k + "saw_detune")))
            s->setDim (dens == 0);
    }
    const auto& fx = paramTable()[(size_t) indexOfId (k + "fixed_note")];
    const auto& bd = paramTable()[(size_t) indexOfId (k + "bend")];
    const int cogLit = (val ("fixed_note") != fx.def || val ("bend") != bd.def) ? 1 : 0;
    if (cogLit != lastCog)
    {
        lastCog = cogLit;
        cog.lit = cogLit == 1;
        cog.setTooltip ("Osc " + juce::String (n) + " settings: Fixed Note and Bend Range (Osc menu)."
                        + juce::String (cogLit ? " Lit: changed from the default." : ""));
        cog.repaint();
    }
}

void OscRow::resized()
{
    const auto k = "osc" + std::to_string (n) + "_";
    const auto col = OscSection::columns (getLocalBounds());
    const int cy = getHeight() / 2;

    // Range: "OSC n" + cog, the four ranges stacked below.
    const int y0 = cy - 46;
    cog.setBounds (col[0].getRight() - 14, y0 + 1, 14, 14);
    place (w.get (k + "range"), juce::Rectangle<int> (col[0].getX(), y0 + 20, col[0].getWidth(), 72));

    auto pitch = col[1];
    knobIn (w.get (k + "coarse"), pitch.removeFromLeft (pitch.getWidth() / 2));
    knobIn (w.get (k + "fine"), pitch);
    auto pmod = col[2];
    knobIn (w.get (k + "env2_pitch"), pmod.removeFromLeft (pmod.getWidth() / 2));
    knobIn (w.get (k + "lfo2_pitch"), pmod);

    const bool saw = ctx.params.value ((size_t) indexOfId (k + "wave")) == 2;
    const int waveH = 64 + (saw ? 30 : 0);
    const int wy = cy - waveH / 2;
    wave.setBounds (col[3].getX(), wy, col[3].getWidth(), 64);
    place (w.get (k + "saw_density"), juce::Rectangle<int> (col[3].getX(), wy + 67, col[3].getWidth(), 13));
    place (w.get (k + "saw_detune"), juce::Rectangle<int> (col[3].getX(), wy + 81, col[3].getWidth(), 13));

    auto shape = col[4];
    const auto shapeCell = shape.removeFromLeft (shape.getWidth() - 58);
    for (const char* id : { "shape", "env1_shape", "lfo1_shape" })
        knobIn (w.get (k + id), shapeCell);
    place (w.get (k + "shape_src"), shape.withSizeKeepingCentre (54, 66));

    static const char* fm[] = { "fm_3to1", "fm_1to2", "fm_2to3" };
    auto ex = col[5];
    knobIn (w.get (k + "vsync"), ex.removeFromLeft (ex.getWidth() / 2));
    knobIn (w.get (fm[(n - 1) % 3]), ex);
}

void OscRow::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto col = OscSection::columns (getLocalBounds());
    g.setColour (ink);
    g.setFont (fontUi (11.0f).withExtraKerningFactor (0.06f));
    g.drawText ("OSC " + juce::String (n), juce::Rectangle<int> (col[0].getX(), getHeight() / 2 - 46, col[0].getWidth() - 14, 16),
                juce::Justification::centredLeft);
    g.setColour (edge);
    for (int i = 1; i < 6; ++i)
        g.drawVerticalLine (col[(size_t) i].getX() - 6, 8.0f, (float) getHeight() - 8);

    // Under the Shape knob: what it does now. The knob keeps the Peak's name; this depends on the wave and on the
    // Shape source (with Mod Env 1 or LFO 1 it is a modulation depth, orange like the mod rings).
    const auto k = "osc" + std::to_string (n) + "_";
    auto val = [&] (const std::string& id) { return ctx.params.value ((size_t) indexOfId (k + id)); };
    const int src = juce::jlimit (0, 2, val ("shape_src")), wv = val ("wave");
    if (auto* knob = w.get (k + (src == 0 ? "shape" : (src == 1 ? "env1_shape" : "lfo1_shape"))))
    {
        static const char* kind[3][3] = { { "harmonics", "shape \xc2\xb7 env 1", "shape \xc2\xb7 LFO 1" },
                                          { "pulse width", "PWM \xc2\xb7 env 1", "PWM \xc2\xb7 LFO 1" },
                                          { "morph", "morph \xc2\xb7 env 1", "morph \xc2\xb7 LFO 1" } };
        const int row = wv == 3 ? 1 : (wv == 4 ? 2 : 0);
        g.setColour (src == 0 ? accent.withAlpha (0.75f) : led);
        g.setFont (fontUi (10.0f, false).withExtraKerningFactor (0.04f));
        g.drawText (juce::String (juce::CharPointer_UTF8 (kind[row][src])),
                    knob->getBounds().withY (knob->getBottom()).withHeight (13).expanded (16, 0), juce::Justification::centred);
    }
}

// ---- OscSection ----

std::array<juce::Rectangle<int>, 6> OscSection::columns (juce::Rectangle<int> row)
{
    auto r = row.reduced (10, 0);
    constexpr int gap = 12, rangeW = 48, waveW = 140;
    const float rest = (float) (r.getWidth() - rangeW - waveW - 5 * gap);
    const float fr = rest / 4.1f;
    std::array<juce::Rectangle<int>, 6> c;
    c[0] = r.removeFromLeft (rangeW);
    r.removeFromLeft (gap);
    c[1] = r.removeFromLeft ((int) fr);
    r.removeFromLeft (gap);
    c[2] = r.removeFromLeft ((int) fr);
    r.removeFromLeft (gap);
    c[3] = r.removeFromLeft (waveW);
    r.removeFromLeft (gap);
    c[4] = r.removeFromLeft ((int) (fr * 1.1f));
    r.removeFromLeft (gap);
    c[5] = r;
    return c;
}

juce::Rectangle<int> OscSection::rowArea (juce::Rectangle<int> panel, int i)
{
    const int top = PanelFrame::kTitleH + 2;
    const int h = (panel.getHeight() - top) / 3;
    return { panel.getX(), panel.getY() + top + i * h, panel.getWidth(), i == 2 ? panel.getHeight() - top - 2 * h : h };
}

OscSection::OscSection (UiContext& c) : PanelFrame (c, {}, "osc1")
{
    for (int i = 0; i < 3; ++i)
    {
        rows[(size_t) i] = std::make_unique<OscRow> (c, i + 1);
        addAndMakeVisible (*rows[(size_t) i]);
    }
}

void OscSection::refresh()
{
    for (auto& r : rows)
        r->refresh();
}

void OscSection::resized()
{
    PanelFrame::resized();
    for (int i = 0; i < 3; ++i)
        rows[(size_t) i]->setBounds (rowArea (getLocalBounds(), i));
}

void OscSection::paint (juce::Graphics& g)
{
    PanelFrame::paint (g);
    const auto col = columns (juce::Rectangle<int> (0, 2, getWidth(), kTitleH));
    const char* names[] = { "Range", "Pitch", "Pitch Mod", "Wave", "Shape", "Sync " "\xc2\xb7" " FM" };
    for (int i = 0; i < 6; ++i)
        lineCaption (g, juce::String::fromUTF8 (names[i]), col[(size_t) i]);
    g.setColour (juce::Colour (0xff0b0d10));
    for (int i = 1; i < 3; ++i)
        g.drawHorizontalLine (rowArea (getLocalBounds(), i).getY(), 0.0f, (float) getWidth());
}

// ---- MixerPanel ----

MixerPanel::MixerPanel (UiContext& c) : PanelFrame (c, "Mixer", "mixer"), w (c) { w.add (*this, spec ("mixer").items); }

void MixerPanel::resized()
{
    PanelFrame::resized();
    const char* left[] = { "mix_osc1", "mix_osc2", "mix_osc3" };
    const char* right[] = { "mix_ring", "mix_noise", "vca_gain" };
    for (int i = 0; i < 3; ++i)
    {
        auto row = OscSection::rowArea (getLocalBounds(), i);
        knobIn (w.get (left[i]), row.removeFromLeft (getWidth() / 2));
        knobIn (w.get (right[i]), row);
    }
}

void MixerPanel::paint (juce::Graphics& g)
{
    PanelFrame::paint (g);
    auto body = getLocalBounds().withTrimmedTop (kTitleH + 2);
    g.setColour (theme::accent.withAlpha (0.035f));
    g.fillRect (body.removeFromLeft (getWidth() / 2));
    g.setColour (juce::Colour (0xff0b0d10));
    for (int i = 1; i < 3; ++i)
        g.drawHorizontalLine (OscSection::rowArea (getLocalBounds(), i).getY(), 0.0f, (float) getWidth());
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
    lineCaption (g, "Cutoff", cutoffCap);
    lineCaption (g, "Cutoff Modulation", modCap);
    caption (g, "Env Src", envCap, juce::Justification::centred);
}

void FilterPanel::resized()
{
    PanelFrame::resized();
    place (w.get ("flt_slope"), juce::Rectangle<int> (getWidth() - 10 - 72 - (getWidth() > 300 ? 0 : 0), 4, 72, 18));
    place (w.get ("flt_shape"), juce::Rectangle<int> (getWidth() - 10 - 72 - 6 - 84, 4, 84, 18));
    auto r = content().reduced (10, 8);
    auto mod = r.removeFromBottom (82);
    r.removeFromBottom (6);
    auto cut = r.removeFromBottom (112);
    r.removeFromBottom (8);
    graph.setBounds (r);

    cutoffCap = cut.removeFromTop (14);
    cut.removeFromTop (2);
    auto mid = cut.withSizeKeepingCentre (64 + 10 + 86 + 10 + 64, cut.getHeight());
    knobIn (w.get ("flt_res"), mid.removeFromLeft (64));
    mid.removeFromLeft (10);
    knobIn (w.get ("flt_freq"), mid.removeFromLeft (86));
    mid.removeFromLeft (10);
    knobIn (w.get ("flt_keytrack"), mid);

    modCap = mod.removeFromTop (14);
    mod.removeFromTop (2);
    auto src = mod.removeFromLeft (84);
    envCap = src.removeFromTop (14);
    place (w.get ("flt_env_sel"), src.withSizeKeepingCentre (80, 44));
    mod.removeFromLeft (6);
    auto depth = mod.removeFromLeft (mod.getWidth() / 3);
    knobIn (w.get ("flt_ampenv"), depth);
    knobIn (w.get ("flt_modenv1"), depth);
    knobRow (mod, { w.get ("flt_lfo1"), w.get ("flt_osc3") });
}

// ---- HeaderTabs ----

HeaderTabs::HeaderTabs (const juce::StringArray& n, const std::vector<int>& src) : names (n)
{
    for (int i = 0; i < names.size(); ++i)
        addAndMakeVisible (grips.add (new DragSource (src[(size_t) i], names[i], DragSource::Style::Grip)));
}

juce::Rectangle<int> HeaderTabs::tabRect (int i) const
{
    const auto f = theme::fontUi (10.5f).withExtraKerningFactor (0.1f);
    int x = 0;
    for (int k = 0; k <= i; ++k)
    {
        const int w = (int) juce::GlyphArrangement::getStringWidth (f, names[k].toUpperCase()) + 46;
        if (k == i)
            return { x, 0, w, getHeight() };
        x += w;
    }
    return {};
}

int HeaderTabs::preferredWidth() const { return names.isEmpty() ? 0 : tabRect (names.size() - 1).getRight(); }

void HeaderTabs::resized()
{
    for (int i = 0; i < names.size(); ++i)
    {
        const auto t = tabRect (i);
        grips[i]->setBounds (t.getRight() - 22, t.getCentreY() - 7, 14, 14);
    }
}

void HeaderTabs::paint (juce::Graphics& g)
{
    using namespace theme;
    for (int i = 0; i < names.size(); ++i)
    {
        const auto t = tabRect (i);
        g.setColour (i == sel ? ink : ink3);
        g.setFont (fontUi (10.5f).withExtraKerningFactor (0.1f));
        g.drawText (names[i].toUpperCase(), t.withTrimmedLeft (10).withTrimmedRight (24), juce::Justification::centredLeft, false);
        if (i == sel)
        {
            g.setColour (accent);
            g.fillRect (t.getX() + 4, getHeight() - 2, t.getWidth() - 8, 2);
        }
    }
}

void HeaderTabs::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < names.size(); ++i)
        if (tabRect (i).contains (e.getPosition()) && i != sel)
        {
            sel = i;
            repaint();
            if (onSelect)
                onSelect (i);
            return;
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
        repaint (0, 0, getWidth(), kTitleH + 2);
    }
}

void EnvPanel::paint (juce::Graphics& g)
{
    using namespace theme;
    PanelFrame::paint (g);
    static const char* titles[] = { "Amp Envelope", "Mod Envelope 1", "Mod Envelope 2" };
    const auto header = juce::Rectangle<int> (0, 2, getWidth() - 22, kTitleH);
    badge (g, titles[tabs.selected()], header);
    if (gateVisible)
    {
        // Gate light: lit while notes passing through the plugin are held; flashes on each note-on.
        const bool flash = juce::Time::getMillisecondCounterHiRes() < flashUntil;
        const bool lit = held > 0 || flash;
        auto dot = juce::Rectangle<float> ((float) tabs.getRight() + 10, (float) header.getCentreY() - 4, 8, 8);
        if (lit)
        {
            g.setColour (led.withAlpha (flash ? 0.6f : 0.3f));
            g.fillEllipse (dot.expanded (flash ? 4.0f : 2.5f));
        }
        g.setColour (lit ? led : juce::Colour (0xff3a2a1f));
        g.fillEllipse (dot);
        caption (g, "Gate" + (held > 0 ? " " + juce::String (held) : juce::String()),
                 juce::Rectangle<int> ((int) dot.getRight() + 5, header.getY(), 52, header.getHeight()));
    }
    caption (g, "Trigger", trigCap);
    static const char* wired[] = { "the VCA (always) and filter cutoff through Env Depth when Env Src = Amp.",
                                   "oscillator Shape (Source = Env 1) and filter cutoff (Env Src = Mod 1).",
                                   "oscillator pitch through each oscillator's Env 2 knob." };
    g.setColour (edge);
    g.drawVerticalLine (noteArea.getX() - 8, (float) noteArea.getY() + 6, (float) noteArea.getBottom() - 6);
    wiredNote (g, wired[tabs.selected()], noteArea);
}

void EnvPanel::resized()
{
    PanelFrame::resized();
    static const char* keys[] = { "amp", "mod1", "mod2" };
    tabs.setBounds (0, 2, tabs.preferredWidth(), kTitleH);
    auto r = content().reduced (10, 8);
    const auto k = std::string (keys[tabs.selected()]) + "_";
    auto row2 = r.removeFromBottom (66);
    r.removeFromBottom (4);
    auto row1 = r.removeFromBottom (80);
    r.removeFromBottom (8);
    graph.setBounds (r);
    knobRow (row1, { w.get (k + "attack"), w.get (k + "decay"), w.get (k + "sustain"), w.get (k + "release") });
    knobIn (w.get (k + "velocity"), row2.removeFromLeft (56));
    row2.removeFromLeft (10);
    auto trig = row2.removeFromLeft (104).withSizeKeepingCentre (104, 38);
    trigCap = trig.removeFromTop (14);
    place (w.get (k + "trigger"), trig);
    row2.removeFromLeft (18);
    noteArea = row2;
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

void LfoPanel::paint (juce::Graphics& g)
{
    PanelFrame::paint (g);
    const int n = tabs.selected() + 1;
    badge (g, n <= 2 ? "Per voice" : "Global", juce::Rectangle<int> (0, 2, getWidth() - 22, kTitleH));
    if (n <= 2)
        caption (g, "Fade Mode", fadeCap);
    static const char* wired[] = { "oscillator Shape (Source = LFO 1) and filter cutoff (LFO 1 knob).",
                                   "oscillator pitch through each oscillator's LFO 2 knob.",
                                   "nothing directly: use it in the Mod Matrix or the FX Mod Matrix.",
                                   "nothing directly: use it in the Mod Matrix or the FX Mod Matrix." };
    g.setColour (theme::edge);
    g.drawVerticalLine (noteArea.getX() - 8, (float) noteArea.getY() + 2, (float) noteArea.getBottom() - 2);
    wiredNote (g, wired[n - 1], noteArea);
}

void LfoPanel::resized()
{
    PanelFrame::resized();
    tabs.setBounds (0, 2, tabs.preferredWidth(), kTitleH);
    const int n = tabs.selected() + 1;
    const auto k = "lfo" + std::to_string (n) + "_";
    auto r = content().reduced (10, 8);
    auto bottom = r.removeFromBottom (34);
    r.removeFromBottom (4);
    auto knobs = r.removeFromBottom (80);
    r.removeFromBottom (8);
    if (n <= 2)
    {
        auto bar = r.removeFromTop (22);
        place (w.get (k + "wave"), bar.removeFromLeft (bar.getWidth() / 2 - 4));
        bar.removeFromLeft (8);
        place (w.get (k + "range"), bar);
        r.removeFromTop (8);
        graph.setBounds (r);
        auto fm = knobs.removeFromRight (100);
        fadeCap = fm.withSizeKeepingCentre (100, 38).removeFromTop (14);
        place (w.get (k + "fade_mode"), fm.withSizeKeepingCentre (100, 38).withTrimmedTop (14));
        knobs.removeFromRight (6);
        auto rate = knobs.removeFromLeft (knobs.getWidth() / 4);
        knobIn (w.get (k + "rate"), rate);
        knobIn (w.get (k + "sync"), rate);
        knobRow (knobs, { w.get (k + "fade"), w.get (k + "phase"), w.get (k + "slew") });
        auto tg = bottom.removeFromLeft (180).withSizeKeepingCentre (180, 20);
        place (w.get (k + "oneshot"), tg.removeFromLeft (90));
        place (w.get (k + "common"), tg);
        bottom.removeFromLeft (16);
    }
    else
    {
        graph.setBounds (r);
        knobRow (knobs.withTrimmedRight (106), { w.get (k + "fade"), w.get (k + "phase"), w.get (k + "slew") });
    }
    noteArea = bottom;
}

// ---- VoicePanel / ArpPanel ----

VoicePanel::VoicePanel (UiContext& c) : PanelFrame (c, "Voice", "voice"), w (c)
{
    w.add (*this, spec ("voice").items);
    if (auto* s = dynamic_cast<ParamStepper*> (w.get ("kbd_octave")))
        s->setCaption ({});  // captioned above
}

void VoicePanel::paint (juce::Graphics& g)
{
    PanelFrame::paint (g);
    caption (g, "Mode", modeCap);
    caption (g, "Octave", octCap);
    caption (g, "Spread Mode", spreadCap);
}

void VoicePanel::resized()
{
    PanelFrame::resized();
    place (w.get ("glide_on"), juce::Rectangle<int> (getWidth() - 30 - 64, 4, 64, 18));
    auto r = content().reduced (8, 6);
    auto sel = r.removeFromTop (36);
    auto a = sel.removeFromLeft (sel.getWidth() / 2 - 3);
    sel.removeFromLeft (6);
    modeCap = a.removeFromTop (13);
    place (w.get ("voice_mode"), a.withHeight (22));
    octCap = sel.removeFromTop (13);
    place (w.get ("kbd_octave"), sel.withHeight (22));
    r.removeFromTop (4);
    const int colW = r.getWidth() / 4;
    auto row1 = r.removeFromTop (64);
    knobRow (row1, { w.get ("unison"), w.get ("unison_detune"), w.get ("spread"), w.get ("glide_time") });
    r.removeFromTop (2);
    auto row2 = r.removeFromTop (64);
    knobIn (w.get ("pre_glide"), row2.removeFromLeft (colW));
    knobIn (w.get ("pan"), row2.removeFromLeft (colW));
    auto sm = row2.reduced (4, 0).withSizeKeepingCentre (row2.getWidth() - 8, 36);
    spreadCap = sm.removeFromTop (13);
    place (w.get ("spread_mode"), sm.withHeight (22));
}

ArpPanel::ArpPanel (UiContext& c) : PanelFrame (c, "Arp", "arp"), w (c)
{
    w.add (*this, spec ("arp").items);
    if (auto* s = dynamic_cast<ParamStepper*> (w.get ("arp_rhythm")))
        s->setCaption ({});
}

void ArpPanel::paint (juce::Graphics& g)
{
    PanelFrame::paint (g);
    caption (g, "Type", typeCap);
    caption (g, "Rate", rateCap);
    caption (g, "Rhythm", rhythmCap);
}

void ArpPanel::resized()
{
    PanelFrame::resized();
    auto head = juce::Rectangle<int> (48, 4, getWidth() - 48 - 28, 18);
    place (w.get ("arp_on"), head.removeFromRight (40));
    head.removeFromRight (6);
    place (w.get ("arp_keysync"), head.removeFromRight (72));
    head.removeFromRight (6);
    place (w.get ("arp_latch"), head.removeFromRight (58));
    auto r = content().reduced (8, 6);
    auto sel = r.removeFromTop (36);
    auto a = sel.removeFromLeft (sel.getWidth() / 2 - 3);
    sel.removeFromLeft (6);
    typeCap = a.removeFromTop (13);
    place (w.get ("arp_type"), a.withHeight (22));
    rateCap = sel.removeFromTop (13);
    place (w.get ("arp_sync"), sel.withHeight (22));
    r.removeFromTop (4);
    knobRow (r.removeFromTop (64), { w.get ("arp_gate"), w.get ("arp_swing"), w.get ("arp_octaves"), w.get ("arp_chance") });
    r.removeFromTop (2);
    auto rh = r.removeFromTop (36);
    rhythmCap = rh.removeFromTop (13);
    place (w.get ("arp_rhythm"), rh.withHeight (22));
}

// ---- DrivePanel ----

DrivePanel::DrivePanel (UiContext& c) : PanelFrame (c, {}), w (c) { w.add (*this, spec ("drive").items); }

void DrivePanel::paint (juce::Graphics& g)
{
    using namespace theme;
    PanelFrame::paint (g);
    const int cw = (getWidth() - 16 - 9) / 6;
    g.setColour (ink);
    g.setFont (fontUi (12.0f).withExtraKerningFactor (0.1f));
    g.drawText ("DRIVE", 8, 2, cw * 3, kTitleH, juce::Justification::centredLeft);
    g.drawText ("ANALOG", 8 + cw * 3 + 9, 2, cw * 3, kTitleH, juce::Justification::centredLeft);
    g.setColour (edge);
    g.drawVerticalLine (8 + cw * 3 + 4, (float) kTitleH + 12, (float) getHeight() - 10);
}

void DrivePanel::resized()
{
    PanelFrame::resized();
    auto r = content().withTrimmedLeft (8).withTrimmedRight (8);
    const int cw = (r.getWidth() - 9) / 6;
    const char* ids[] = { "flt_overdrive", "flt_postdrive", "dist_level", "osc_divergence", "osc_drift", "flt_divergence" };
    for (int i = 0; i < 6; ++i)
    {
        const int x = r.getX() + i * cw + (i >= 3 ? 9 : 0);
        knobIn (w.get (ids[i]), juce::Rectangle<int> (x, r.getY(), cw, r.getHeight()));
    }
}

// ---- SourceRail ----

SourceRail::SourceRail()
{
    struct S { int src; const char* label; DragSource::Style style; const char* pad; };
    const S list[] = { { 14, "Animate 1", DragSource::Style::Pad, "1" }, { 15, "Animate 2", DragSource::Style::Pad, "2" },
                       { 1, "Mod Wheel", DragSource::Style::Meter, "" }, { 2, "Aftertouch", DragSource::Style::Meter, "" },
                       { 5, "Velocity", DragSource::Style::Meter, "" }, { 3, "Expression", DragSource::Style::Meter, "" },
                       { 6, "Keyboard", DragSource::Style::Meter, "" }, { 21, "Bend", DragSource::Style::Meter, "" } };
    for (const auto& s : list)
        addAndMakeVisible (sources.add (new DragSource (s.src, s.label, s.style, s.pad)));
}

void SourceRail::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (20);
    r.removeFromBottom (46);
    const int n = sources.size();
    const int h = (r.getHeight() - (n - 1) * 6) / n;
    for (auto* s : sources)
    {
        s->setBounds (r.removeFromTop (h));
        r.removeFromTop (6);
    }
}

void SourceRail::paint (juce::Graphics& g)
{
    using namespace theme;
    caption (g, "Sources", getLocalBounds().removeFromTop (18), juce::Justification::centred);
    auto hint = getLocalBounds().removeFromBottom (42);
    juce::AttributedString s;
    s.append ("Drag onto a knob. ", fontBody (10.0f), ink3);
    s.append ("Orange ring", fontBody (10.0f), led);
    s.append (" = modulated.", fontBody (10.0f), ink3);
    s.setJustification (juce::Justification::centred);
    s.setWordWrap (juce::AttributedString::byWord);
    s.draw (g, hint.toFloat());
}

// ---- MainPage ----

MainPage::MainPage (UiContext& c, const UiFeed& f, std::function<bool()> on)
    : oscs (c), mixer (c), filter (c), env (c, f, on), lfo (c, f, on), voice (c), arp (c), drive (c)
{
    for (juce::Component* p : { (juce::Component*) &rail, (juce::Component*) &oscs, (juce::Component*) &mixer,
                                (juce::Component*) &filter, (juce::Component*) &env, (juce::Component*) &lfo,
                                (juce::Component*) &voice, (juce::Component*) &arp, (juce::Component*) &drive })
        addAndMakeVisible (p);
}

void MainPage::refresh()
{
    oscs.refresh();
    filter.refresh();
    env.refresh();
    lfo.refresh();
}

void MainPage::resized()
{
    // The mock: rail 76 | oscillators 776 | mixer 130 | filter; envelopes 390 | LFOs 354 | voice + arp, drive.
    auto r = getLocalBounds();
    rail.setBounds (r.removeFromLeft (76));
    r.removeFromLeft (8);
    auto top = r.removeFromTop (420);
    r.removeFromTop (8);
    oscs.setBounds (top.removeFromLeft (776));
    top.removeFromLeft (8);
    mixer.setBounds (top.removeFromLeft (130));
    top.removeFromLeft (8);
    filter.setBounds (top);

    env.setBounds (r.removeFromLeft (390));
    r.removeFromLeft (8);
    lfo.setBounds (r.removeFromLeft (354));
    r.removeFromLeft (8);
    drive.setBounds (r.removeFromBottom (88));
    r.removeFromBottom (8);
    voice.setBounds (r.removeFromLeft (r.getWidth() / 2 - 4));
    r.removeFromLeft (8);
    arp.setBounds (r);
}
