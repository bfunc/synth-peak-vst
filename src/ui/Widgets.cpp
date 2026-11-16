#include "Widgets.h"

#include "Theme.h"
#include "ui/Help.h"

using namespace peak;

namespace
{
// Table value <-> the JUCE parameter's plain value (a choice is stored as its index).
int tableFromPlain (const ParamDef& p, double plain)
{
    const int v = (int) std::lround (plain);
    return p.kind == Kind::Choice ? p.min + v : v;
}
float plainFromTable (const ParamDef& p, int v) { return (float) (p.kind == Kind::Choice ? v - p.min : v); }
} // namespace

juce::var modSourceDragId (int source) { return juce::String ("modsrc:") + juce::String (source); }

int modSourceFromDragId (const juce::var& v)
{
    const auto s = v.toString();
    return s.startsWith ("modsrc:") ? s.fromFirstOccurrenceOf (":", false, false).getIntValue() : -1;
}

juce::String itemLabel (const ui::Item& it)
{
    if (! it.label.empty())
        return it.label;
    const int i = indexOfId (it.id);
    return i < 0 ? juce::String (it.id) : juce::String (paramTable()[(size_t) i].name);
}

// ---------------------------------------------------------------------------------------------------------------

ParamWidget::ParamWidget (UiContext& c, const std::string& id) : ctx (c), idx (indexOfId (id))
{
    jassert (idx >= 0);  // the layout tests catch unknown ids
    setTitle (idx >= 0 ? juce::String (def().name) : juce::String (id));
    const auto help = ui::helpFor (id);
    setTooltip (help.empty() ? getTitle() : juce::String::fromUTF8 (help.c_str()));
}

const ParamDef& ParamWidget::def() const { return paramTable()[(size_t) juce::jmax (0, idx)]; }

// ---------------------------------------------------------------------------------------------------------------

ParamKnob::ParamKnob (UiContext& c, const std::string& id, const juce::String& l, Size s)
    : ParamWidget (c, id), label (l), size (s)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setMouseDragSensitivity (200);
    slider.setVelocityBasedMode (false);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.getProperties().set ("bipolar", def().kind == Kind::Bipolar);
    slider.setTitle (def().name);
    const auto* p = &def();
    slider.textFromValueFunction = [p] (double v) { return juce::String (formatValue (*p, tableFromPlain (*p, v))); };
    addAndMakeVisible (slider);
    if (auto* prm = ctx.params.parameter ((size_t) idx))
    {
        attachment = std::make_unique<juce::SliderParameterAttachment> (*prm, slider);
        slider.setDoubleClickReturnValue (true, plainFromTable (def(), def().def));
    }
    slider.onValueChange = [this] { repaint(); };
    slider.setTooltip (getTooltip());
}

void ParamKnob::resized()
{
    const int knobH = size == Size::Big ? 70 : (size == Size::Small ? 38 : 46);
    slider.setBounds (getLocalBounds().removeFromTop (knobH).withSizeKeepingCentre (knobH, knobH));
}

void ParamKnob::paint (juce::Graphics& g)
{
    using namespace theme;
    if (dropHover)
    {
        g.setColour (led.withAlpha (0.16f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (led);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
    }
    auto r = getLocalBounds();
    r.removeFromTop (slider.getBottom());
    const bool hot = slider.isMouseOverOrDragging();
    const bool modded = std::abs ((double) slider.getProperties().getWithDefault ("mod", 0.0)) > 0.001;
    if (showValue)
    {
        g.setFont (fontMono (10.5f));
        g.setColour (hot ? accent : ink2);
        g.drawText (slider.getTextFromValue (slider.getValue()), r.removeFromTop (13), juce::Justification::centred);
    }
    g.setFont (fontUi (10.5f));
    g.setColour (modded ? led : (hot ? ink : ink3));
    g.drawFittedText (label.toUpperCase(), r.removeFromTop (14), juce::Justification::centred, 1, 0.8f);
}

void ParamKnob::setModAmount (double m)
{
    const double old = (double) slider.getProperties().getWithDefault ("mod", 0.0);
    if (std::abs (old - m) < 1e-6)
        return;
    slider.getProperties().set ("mod", m);
    slider.repaint();
    repaint();
}

bool ParamKnob::isInterestedInDragSource (const SourceDetails& d)
{
    return modSourceFromDragId (d.description) >= 0;
}

void ParamKnob::itemDragEnter (const SourceDetails&)
{
    dropHover = true;
    repaint();
}

void ParamKnob::itemDragExit (const SourceDetails&)
{
    dropHover = false;
    repaint();
}

void ParamKnob::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    repaint();
    if (ctx.onSourceDropped)
        ctx.onSourceDropped (modSourceFromDragId (d.description), def().id);
}

// ---------------------------------------------------------------------------------------------------------------

ParamSegment::ParamSegment (UiContext& c, const std::string& id, bool v, const juce::StringArray& shortNames)
    : ParamWidget (c, id), vertical (v)
{
    const auto& p = def();
    for (int val = p.min; val <= p.max; ++val)
    {
        const int k = val - p.min;
        auto* b = buttons.add (new juce::TextButton (k < shortNames.size() ? shortNames[k] : juce::String (formatValue (p, val))));
        b->getProperties().set ("segment", true);
        b->setClickingTogglesState (false);
        b->onClick = [this, val]
        {
            if (attachment)
                attachment->setValueAsCompleteGesture (plainFromTable (def(), val));
        };
        addAndMakeVisible (b);
    }
    if (auto* prm = ctx.params.parameter ((size_t) idx))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float plain) { show (plain); });
        attachment->sendInitialUpdate();
    }
}

void ParamSegment::show (float plain)
{
    const int v = tableFromPlain (def(), plain);
    for (int k = 0; k < buttons.size(); ++k)
        buttons[k]->setToggleState (def().min + k == v, juce::dontSendNotification);
}

void ParamSegment::resized()
{
    auto r = getLocalBounds().reduced (1);
    const int n = juce::jmax (1, buttons.size());
    if (columns > 0)
    {
        const int rows = (n + columns - 1) / columns;
        const float cw = r.getWidth() / (float) columns, rh = r.getHeight() / (float) rows;
        for (int k = 0; k < buttons.size(); ++k)
            buttons[k]->setBounds (juce::Rectangle<float> (r.getX() + (k % columns) * cw, r.getY() + (k / columns) * rh, cw, rh)
                                       .toNearestInt()
                                       .reduced (1));
        return;
    }
    for (int k = 0; k < buttons.size(); ++k)
    {
        if (vertical)
            buttons[k]->setBounds (r.removeFromTop (r.getHeight() / (n - k)).reduced (0, 1));
        else
            buttons[k]->setBounds (r.removeFromLeft (r.getWidth() / (n - k)).reduced (1, 0));
    }
}

void ParamSegment::paint (juce::Graphics& g)
{
    g.setColour (theme::well);
    g.fillRect (getLocalBounds());
    g.setColour (juce::Colours::black);
    g.drawRect (getLocalBounds());
}

// ---------------------------------------------------------------------------------------------------------------

ParamToggle::ParamToggle (UiContext& c, const std::string& id, const juce::String& l) : ParamWidget (c, id)
{
    button.setButtonText (l);
    addAndMakeVisible (button);
    if (auto* prm = ctx.params.parameter ((size_t) idx))
        attachment = std::make_unique<juce::ButtonParameterAttachment> (*prm, button);
}

// ---------------------------------------------------------------------------------------------------------------

ParamChoice::ParamChoice (UiContext& c, const std::string& id) : ParamWidget (c, id)
{
    const auto& p = def();
    for (int v = p.min; v <= p.max; ++v)
        box.addItem (formatValue (p, v), v - p.min + 1);
    box.setTitle (p.name);
    addAndMakeVisible (box);
    if (auto* prm = ctx.params.parameter ((size_t) idx))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float plain)
        {
            box.setSelectedId (tableFromPlain (def(), plain) - def().min + 1, juce::dontSendNotification);
        });
        attachment->sendInitialUpdate();
    }
    box.onChange = [this]
    {
        if (attachment && box.getSelectedId() > 0)
            attachment->setValueAsCompleteGesture (plainFromTable (def(), def().min + box.getSelectedId() - 1));
    };
}

// ---------------------------------------------------------------------------------------------------------------

ParamStepper::ParamStepper (UiContext& c, const std::string& id, const juce::String& cap) : ParamWidget (c, id), caption (cap)
{
    for (auto* b : { &prev, &next })
        addAndMakeVisible (b);
    prev.onClick = [this] { stepBy (-1); };
    next.onClick = [this] { stepBy (1); };
    if (auto* prm = ctx.params.parameter ((size_t) idx))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float) { repaint(); });
        attachment->sendInitialUpdate();
    }
}

void ParamStepper::stepBy (int d)
{
    const auto& p = def();
    int v = ctx.params.value ((size_t) idx) + d;
    if (v > p.max)
        v = p.min;
    if (v < p.min)
        v = p.max;
    if (attachment)
        attachment->setValueAsCompleteGesture (plainFromTable (p, v));
}

void ParamStepper::resized()
{
    auto r = getLocalBounds();
    prev.setBounds (r.removeFromLeft (20));
    next.setBounds (r.removeFromRight (20));
}

void ParamStepper::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds().reduced (20, 0);
    g.setColour (well);
    g.fillRect (r);
    g.setColour (juce::Colours::black);
    g.drawRect (r.expanded (0, 0));
    const auto text = juce::String (formatValue (def(), ctx.params.value ((size_t) idx)));
    if (caption.isNotEmpty())
    {
        g.setFont (fontUi (9.5f, false));
        g.setColour (ink3);
        g.drawText (caption.toUpperCase(), r.reduced (5, 0), juce::Justification::centredLeft);
    }
    g.setFont (fontUi (12.0f));
    g.setColour (ink);
    g.drawFittedText (text, r.reduced (caption.isNotEmpty() ? 40 : 4, 0), juce::Justification::centred, 1, 0.8f);
}

void ParamStepper::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().reduced (20, 0).contains (e.getPosition()))
        return;
    juce::PopupMenu m;
    const auto& p = def();
    const int cur = ctx.params.value ((size_t) idx);
    for (int v = p.min; v <= p.max; ++v)
        m.addItem (v - p.min + 1, formatValue (p, v), true, v == cur);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        if (r > 0 && attachment)
            attachment->setValueAsCompleteGesture (plainFromTable (def(), def().min + r - 1));
    });
}

// ---------------------------------------------------------------------------------------------------------------

ParamBar::ParamBar (UiContext& c, const std::string& id) : ParamWidget (c, id)
{
    if (auto* prm = ctx.params.parameter ((size_t) idx))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float) { repaint(); });
        attachment->sendInitialUpdate();
    }
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void ParamBar::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto& p = def();
    const int v = ctx.params.value ((size_t) idx);
    auto r = getLocalBounds().toFloat();
    auto text = r.removeFromRight (34);
    auto bar = r.reduced (0, r.getHeight() * 0.5f - 7);
    g.setColour (well);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (bar, 2.0f, 1.0f);
    const float cx = bar.getCentreX();
    g.setColour (edgeHi);
    g.drawVerticalLine ((int) cx, bar.getY() - 2, bar.getBottom() + 2);
    const float half = bar.getWidth() * 0.5f;
    const float frac = (float) (v - p.center) / (float) juce::jmax (1, p.max - p.center);
    const float w = std::abs (frac) * half;
    g.setColour (v == p.center ? ink3 : accent);
    g.fillRect (juce::Rectangle<float> (frac < 0 ? cx - w : cx, bar.getY() + 2, w, bar.getHeight() - 4));
    g.setFont (fontMono (10.5f));
    g.setColour (ink2);
    g.drawText (formatValue (p, v), text, juce::Justification::centred);
}

void ParamBar::setFromX (int x)
{
    const auto& p = def();
    const float w = (float) juce::jmax (1, getWidth() - 34);
    const float frac = juce::jlimit (0.0f, 1.0f, (float) x / w);
    const int v = juce::jlimit (p.min, p.max, (int) std::lround (p.min + frac * (p.max - p.min)));
    if (attachment)
        attachment->setValueAsPartOfGesture (plainFromTable (p, v));
}

void ParamBar::mouseDown (const juce::MouseEvent& e)
{
    if (attachment)
        attachment->beginGesture();
    setFromX (e.x);
}

void ParamBar::mouseDrag (const juce::MouseEvent& e) { setFromX (e.x); }

void ParamBar::mouseUp (const juce::MouseEvent&)
{
    if (attachment)
        attachment->endGesture();
}

void ParamBar::mouseDoubleClick (const juce::MouseEvent&)
{
    if (attachment)
        attachment->setValueAsCompleteGesture (plainFromTable (def(), def().def));
}

// ---------------------------------------------------------------------------------------------------------------

ParamSlim::ParamSlim (UiContext& c, const std::string& id, const juce::String& l) : ParamWidget (c, id), label (l)
{
    if (auto* prm = ctx.params.parameter ((size_t) idx))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float) { repaint(); });
        attachment->sendInitialUpdate();
    }
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

juce::Rectangle<int> ParamSlim::track() const
{
    auto r = getLocalBounds();
    r.removeFromLeft (40);
    r.removeFromRight (26);
    return r.withSizeKeepingCentre (r.getWidth(), 5);
}

void ParamSlim::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto& p = def();
    const int v = ctx.params.value ((size_t) idx);
    auto r = getLocalBounds();
    const bool hot = isMouseOverOrDragging();
    g.setFont (fontUi (9.0f));
    g.setColour (dim ? ink3.withAlpha (0.6f) : (hot ? ink : ink3));
    g.drawText (label.toUpperCase(), r.removeFromLeft (40), juce::Justification::centredLeft);
    g.setFont (fontMono (9.5f));
    g.setColour (dim ? ink3.withAlpha (0.6f) : ink2);
    g.drawText (juce::String (formatValue (p, v)), r.removeFromRight (24), juce::Justification::centredRight);
    auto t = track().toFloat();
    g.setColour (well);
    g.fillRoundedRectangle (t, 2.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (t, 2.0f, 1.0f);
    const float frac = (float) (v - p.min) / (float) juce::jmax (1, p.max - p.min);
    g.setColour (dim ? ink3 : accent);
    g.fillRoundedRectangle (t.withWidth (t.getWidth() * frac), 2.0f);
}

void ParamSlim::setFromX (int x)
{
    const auto& p = def();
    const auto t = track();
    const float frac = juce::jlimit (0.0f, 1.0f, (float) (x - t.getX()) / (float) juce::jmax (1, t.getWidth()));
    if (attachment)
        attachment->setValueAsPartOfGesture (plainFromTable (p, (int) std::lround (p.min + frac * (p.max - p.min))));
}

void ParamSlim::mouseDown (const juce::MouseEvent& e)
{
    if (attachment)
        attachment->beginGesture();
    setFromX (e.x);
}

void ParamSlim::mouseDrag (const juce::MouseEvent& e) { setFromX (e.x); }

void ParamSlim::mouseUp (const juce::MouseEvent&)
{
    if (attachment)
        attachment->endGesture();
}

void ParamSlim::mouseDoubleClick (const juce::MouseEvent&)
{
    if (attachment)
        attachment->setValueAsCompleteGesture (plainFromTable (def(), def().def));
}

void ParamSlim::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    wheelAcc += w.deltaY;
    const int steps = (int) (wheelAcc / 0.1f);
    if (steps == 0)
        return;
    wheelAcc -= steps * 0.1f;
    const auto& p = def();
    const int v = juce::jlimit (p.min, p.max, ctx.params.value ((size_t) idx) + steps * (e.mods.isShiftDown() ? 1 : 2));
    if (attachment)
        attachment->setValueAsCompleteGesture (plainFromTable (p, v));
}

// ---------------------------------------------------------------------------------------------------------------

std::unique_ptr<ParamWidget> makeWidget (UiContext& ctx, const ui::Item& it)
{
    using W = ui::Widget;
    const auto label = itemLabel (it);
    switch (it.widget)
    {
        case W::Knob: return std::make_unique<ParamKnob> (ctx, it.id, label, ParamKnob::Size::Normal);
        case W::BigKnob: return std::make_unique<ParamKnob> (ctx, it.id, label, ParamKnob::Size::Big);
        case W::SmallKnob: return std::make_unique<ParamKnob> (ctx, it.id, label, ParamKnob::Size::Small);
        case W::Segmented: return std::make_unique<ParamSegment> (ctx, it.id, false);
        case W::SegmentedV: return std::make_unique<ParamSegment> (ctx, it.id, true);
        case W::Toggle: return std::make_unique<ParamToggle> (ctx, it.id, label);
        case W::Choice: return std::make_unique<ParamChoice> (ctx, it.id);
        case W::Stepper: return std::make_unique<ParamStepper> (ctx, it.id, label);
        case W::Slim: return std::make_unique<ParamSlim> (ctx, it.id, label);
    }
    return nullptr;
}
