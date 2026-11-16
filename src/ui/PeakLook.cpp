#include "PeakLook.h"

#include "Theme.h"

#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
 #define PEAK_HAS_FONTS 1
#endif

namespace theme
{
namespace
{
juce::Typeface::Ptr face (const void* data, size_t size)
{
    return data != nullptr ? juce::Typeface::createSystemTypefaceFor (data, size) : nullptr;
}

struct Faces
{
    juce::Typeface::Ptr uiBold, uiMedium, bodyRegular, monoRegular;
    Faces()
    {
#if PEAK_HAS_FONTS
        uiBold = face (BinaryData::BarlowSemiCondensedSemiBold_ttf, BinaryData::BarlowSemiCondensedSemiBold_ttfSize);
        uiMedium = face (BinaryData::BarlowSemiCondensedMedium_ttf, BinaryData::BarlowSemiCondensedMedium_ttfSize);
        bodyRegular = face (BinaryData::BarlowRegular_ttf, BinaryData::BarlowRegular_ttfSize);
        monoRegular = face (BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);
#endif
    }
};

const Faces& faces()
{
    static const Faces f;
    return f;
}

juce::Font make (juce::Typeface::Ptr tf, float h, const juce::String& fallback, bool bold)
{
    if (tf != nullptr)
        return juce::Font (juce::FontOptions (tf).withHeight (h));
    return juce::Font (juce::FontOptions (fallback, h, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace

juce::Font fontUi (float h, bool bold) { return make (bold ? faces().uiBold : faces().uiMedium, h, "Arial Narrow", bold); }
juce::Font fontBody (float h) { return make (faces().bodyRegular, h, juce::Font::getDefaultSansSerifFontName(), false); }
juce::Font fontMono (float h) { return make (faces().monoRegular, h, juce::Font::getDefaultMonospacedFontName(), false); }
} // namespace theme

PeakLook::PeakLook()
{
    using namespace theme;
    setColour (juce::ResizableWindow::backgroundColourId, chassis);
    setColour (juce::Label::textColourId, ink2);
    setColour (juce::TextButton::buttonColourId, panelHi);
    setColour (juce::TextButton::buttonOnColourId, led);
    setColour (juce::TextButton::textColourOffId, ink2);
    setColour (juce::TextButton::textColourOnId, voidBg);
    setColour (juce::ComboBox::backgroundColourId, well);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::black);
    setColour (juce::ComboBox::arrowColourId, ink3);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff14181c));
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, knobHi);
    setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setColour (juce::TextEditor::backgroundColourId, well);
    setColour (juce::TextEditor::textColourId, ink);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::ScrollBar::thumbColourId, edgeHi);
    setColour (juce::ToggleButton::textColourId, ink2);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, ink);
    setColour (juce::AlertWindow::outlineColourId, edgeHi);
    setColour (juce::ProgressBar::backgroundColourId, well);
    setColour (juce::ProgressBar::foregroundColourId, accent);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1b2026));
    setColour (juce::TooltipWindow::textColourId, ink);
    setColour (juce::TooltipWindow::outlineColourId, edgeHi);
}

void PeakLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                 juce::Slider& s)
{
    using namespace theme;
    const auto box = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float size = juce::jmin (box.getWidth(), box.getHeight());
    const auto c = box.getCentre();
    const float rTrack = size * 0.5f - 3.0f;
    const float a = start + pos * (end - start);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float a0 = bipolar ? (start + end) * 0.5f : start;

    auto arc = [&] (float r, float from, float to, juce::Colour col, float thick)
    {
        if (std::abs (to - from) < 0.01f)
            return;
        juce::Path p;
        p.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (from, to), juce::jmax (from, to), true);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    const float thick = juce::jmax (2.5f, size * 0.07f);
    arc (rTrack, start, end, juce::Colour (0xff0a0c0f), thick);
    arc (rTrack, a0, a, s.isEnabled() ? accent : ink3, thick);

    // Modulation: the reach of all slots (depth 64 = the whole knob), the selected slot bright on top.
    const auto& pr = s.getProperties();
    const double lo = pr.getWithDefault ("modLo", 0.0), hi = pr.getWithDefault ("modHi", 0.0);
    const bool focus = pr.getWithDefault ("modFocus", false);
    auto at = [&] (double m) { return juce::jlimit (start, end, a + (float) m * (end - start)); };
    const float rMod = rTrack + thick * 0.95f, modThick = juce::jmax (1.5f, thick * 0.6f);
    if (lo != 0 || hi != 0)
        arc (rMod, at (lo), at (hi), led.withAlpha (focus ? 0.72f : 1.0f), modThick);
    if (focus)
    {
        const double flo = pr.getWithDefault ("modFLo", 0.0), fhi = pr.getWithDefault ("modFHi", 0.0);
        arc (rMod, at (flo), at (fhi), ledSoft.withAlpha (0.35f), modThick * 2.6f);
        arc (rMod, at (flo), at (fhi), ledSoft, modThick * 1.3f);
    }
    // Saved value: a short notch across the track (only when the knob is away from it).
    const double sv = pr.getWithDefault ("saved", -1.0);
    if (sv >= 0 && std::abs (sv - pos) > 0.004)
    {
        const float sa = start + (float) sv * (end - start);
        const auto n1 = c.getPointOnCircumference (rTrack - thick * 0.75f, sa), n2 = c.getPointOnCircumference (rTrack + thick * 0.75f, sa);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawLine ({ n1, n2 }, 3.4f);
        g.setColour (juce::Colour (0xffeef2f6));
        g.drawLine ({ n1, n2 }, 1.9f);
    }

    const float rBody = rTrack - thick * 1.2f;
    g.setColour (knob);
    g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2, 1.0f);
    const auto p1 = c.getPointOnCircumference (rBody * 0.3f, a);
    const auto p2 = c.getPointOnCircumference (rBody * 0.85f, a);
    g.setColour (s.isMouseOverOrDragging() ? juce::Colours::white : ink);
    g.drawLine ({ p1, p2 }, juce::jmax (1.8f, size * 0.045f));
}

void PeakLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace theme;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool seg = (bool) b.getProperties().getWithDefault ("segment", false);
    if (seg)
    {
        g.setColour (on ? knobHi : well);
        g.fillRect (r);
        if (on)
        {
            g.setColour (led);
            g.fillRect (r.removeFromBottom (2.0f));
        }
        return;
    }
    juce::ColourGradient grad (on ? led : juce::Colour (0xff262c33), 0, r.getY(), on ? led.darker (0.1f) : juce::Colour (0xff1c2127), 0,
                               r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    if ((over || down) && ! on)
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.08f : 0.04f));
        g.fillRoundedRectangle (r, 2.0f);
    }
}

void PeakLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    using namespace theme;
    const bool on = b.getToggleState();
    const bool seg = (bool) b.getProperties().getWithDefault ("segment", false);
    g.setFont (theme::fontUi (seg ? 11.0f : 12.0f));
    juce::Colour col = seg ? (on ? ink : (over ? ink : ink3)) : (on ? voidBg : (over ? ink : ink2));
    if (! b.isEnabled())
        col = col.withAlpha (0.4f);
    g.setColour (col);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 0), juce::Justification::centred, 1);
}

void PeakLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    using namespace theme;
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat();
    const float d = 8.0f;
    const auto dot = juce::Rectangle<float> (r.getX() + 1, r.getCentreY() - d / 2, d, d);
    if (on)
    {
        g.setColour (led.withAlpha (0.35f));
        g.fillEllipse (dot.expanded (2.5f));
    }
    g.setColour (on ? led : juce::Colour (0xff3a2a1f));
    g.fillEllipse (dot);
    g.setColour (over ? ink : ink2);
    g.setFont (theme::fontUi (11.0f));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().withTrimmedLeft ((int) d + 6),
                      juce::Justification::centredLeft, 1);
}

void PeakLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace theme;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (well);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (box.hasKeyboardFocus (false) ? accent : juce::Colours::black);
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    juce::Path tri;
    const float cx = (float) w - 9.0f, cy = (float) h * 0.5f;
    tri.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
    g.setColour (ink3);
    g.fillPath (tri);
}

juce::Font PeakLook::getComboBoxFont (juce::ComboBox&) { return theme::fontUi (12.5f, false); }

void PeakLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

juce::Font PeakLook::getPopupMenuFont() { return theme::fontUi (14.0f, false); }
juce::Font PeakLook::getTextButtonFont (juce::TextButton&, int) { return theme::fontUi (12.0f); }
juce::Font PeakLook::getLabelFont (juce::Label& l) { return l.getFont(); }

void PeakLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (theme::edgeHi);
    g.drawRect (0, 0, w, h);
}

void PeakLook::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                              int thumbStart, int thumbSize, bool over, bool down)
{
    g.setColour (theme::well);
    g.fillRect (x, y, w, h);
    auto thumb = vertical ? juce::Rectangle<int> (x + 2, thumbStart, w - 4, thumbSize)
                          : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, h - 4);
    g.setColour (over || down ? theme::ink3 : theme::edgeHi);
    g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
}
