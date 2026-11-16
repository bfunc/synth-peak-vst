#include "PanelKit.h"

#include "Theme.h"

using namespace peak;

// ---- WidgetSet ----

WidgetSet::~WidgetSet()
{
    for (auto& [id, w] : widgets)
    {
        auto& reg = ctx.registry;
        reg.erase (std::remove (reg.begin(), reg.end(), w.get()), reg.end());
    }
}

void WidgetSet::add (juce::Component& parent, const std::vector<ui::Item>& items)
{
    for (const auto& it : items)
    {
        if (widgets.count (it.id))
            continue;
        auto w = makeWidget (ctx, it);
        if (w == nullptr)
            continue;
        parent.addAndMakeVisible (*w);
        ctx.registry.push_back (w.get());
        widgets[it.id] = std::move (w);
    }
}

ParamWidget* WidgetSet::get (const std::string& id) const
{
    auto it = widgets.find (id);
    return it == widgets.end() ? nullptr : it->second.get();
}

ParamWidget& WidgetSet::operator[] (const std::string& id) const
{
    auto* w = get (id);
    jassert (w != nullptr);
    return *w;
}

void WidgetSet::setVisible (const std::vector<ui::Item>& items, bool visible) const
{
    for (const auto& it : items)
        if (auto* w = get (it.id))
            w->setVisible (visible);
}

// ---- PanelFrame ----

PanelFrame::PanelFrame (UiContext& c, const juce::String& t, const std::string& key) : ctx (c), title (t), moreKey (key)
{
    if (! moreKey.empty() && ! ui::moreParams (moreKey).empty())
    {
        more.setTooltip ("More " + title + " settings");
        more.onClick = [this]
        {
            auto grid = std::make_unique<MoreGrid> (ctx, moreKey);
            juce::CallOutBox::launchAsynchronously (std::move (grid), more.getScreenBounds(), nullptr);
        };
        addAndMakeVisible (more);
    }
}

juce::Rectangle<int> PanelFrame::content() const { return getLocalBounds().withTrimmedTop (kTitleH + 2); }

void PanelFrame::resized() { more.setBounds (getWidth() - 22, 5, 16, 14); }

void PanelFrame::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1f25), 0, 0, panel, 0, 48, false));
    g.fillRect (r);
    g.setColour (accent);
    g.fillRect (r.removeFromTop (2));
    g.setColour (juce::Colour (0xff0b0d10));
    g.drawHorizontalLine (kTitleH + 1, 0.0f, (float) getWidth());
    if (title.isNotEmpty())
    {
        g.setColour (ink);
        g.setFont (fontUi (12.0f).withExtraKerningFactor (0.1f));
        g.drawText (title.toUpperCase(), juce::Rectangle<int> (8, 2, getWidth() - 40, kTitleH), juce::Justification::centredLeft);
    }
}

// ---- MoreGrid ----

MoreGrid::MoreGrid (UiContext& c, const std::string& key) : ctx (c)
{
    for (int i : ui::moreParams (key))
    {
        const auto& p = paramTable()[(size_t) i];
        ui::Item it { ui::Widget::Knob, p.id, p.name };
        if (p.kind == Kind::Bool)
            it.widget = ui::Widget::Toggle;
        else if (p.kind == Kind::Choice)
            it.widget = (p.max - p.min) < 4 ? ui::Widget::Segmented : ui::Widget::Choice;
        auto w = makeWidget (ctx, it);  // not registered: lives only while the pop-out is open
        addAndMakeVisible (*w);
        captions.push_back (it.widget == ui::Widget::Knob ? juce::String() : juce::String (p.name));
        widgets.push_back (std::move (w));
    }
    // Knobs in a grid, then one row per other widget.
    int knobs = 0, others = 0;
    for (const auto& c2 : captions)
        c2.isEmpty() ? ++knobs : ++others;
    const int cols = juce::jlimit (1, 6, knobs);
    const int rows = (knobs + cols - 1) / juce::jmax (1, cols);
    setSize (juce::jmax (300, cols * 72 + 16), 12 + rows * 84 + others * 30 + 8);
}

void MoreGrid::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    int knobs = 0;
    for (const auto& c : captions)
        knobs += c.isEmpty();
    const int cols = juce::jlimit (1, 6, knobs);
    int k = 0;
    juce::Rectangle<int> row;
    for (size_t i = 0; i < widgets.size(); ++i)
    {
        if (! captions[i].isEmpty())
            continue;
        if (k % cols == 0)
            row = r.removeFromTop (84);
        widgets[i]->setBounds (row.removeFromLeft (72).withSizeKeepingCentre (60, 80));
        ++k;
    }
    for (size_t i = 0; i < widgets.size(); ++i)
    {
        if (captions[i].isEmpty())
            continue;
        auto line = r.removeFromTop (30).reduced (0, 4);
        line.removeFromLeft (120);
        widgets[i]->setBounds (line.removeFromLeft (juce::jmin (220, line.getWidth())));
    }
}

void MoreGrid::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (panel);
    auto r = getLocalBounds().reduced (8, 6);
    int knobs = 0;
    for (const auto& c : captions)
        knobs += c.isEmpty();
    const int cols = juce::jlimit (1, 6, knobs);
    r.removeFromTop (((knobs + cols - 1) / juce::jmax (1, cols)) * 84);
    g.setFont (fontUi (11.0f));
    g.setColour (ink3);
    for (const auto& c : captions)
        if (! c.isEmpty())
            g.drawText (c.toUpperCase(), r.removeFromTop (30).withWidth (116), juce::Justification::centredLeft);
}

// ---- DragSource ----

DragSource::DragSource (int s, const juce::String& l, Style st, juce::String pad)
    : source (s), label (l), padText (std::move (pad)), style (st)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag " + label + " onto a knob to add a mod slot");
}

void DragSource::paint (juce::Graphics& g)
{
    using namespace theme;
    const bool over = isMouseOver();
    auto r = getLocalBounds();
    if (style == Style::Grip)
    {
        g.setColour (over ? led : ink3);
        g.setFont (fontUi (12.0f));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x9c\xa5"), r, juce::Justification::centred);
        return;
    }
    g.setColour (panel);
    g.fillRect (r);
    g.setColour (over ? led : edgeHi);
    g.fillRect (r.removeFromTop (2));
    r.reduce (4, 6);
    auto top = r.removeFromTop (r.getHeight() - 24);
    if (style == Style::Pad)
    {
        auto pad = top.withSizeKeepingCentre (38, 38).toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9edf0), 0, pad.getY(), juce::Colour (0xffb9c0c6), 0,
                                                 pad.getBottom(), false));
        g.fillRoundedRectangle (pad, 3.0f);
        g.setColour (juce::Colour (0xff2a3036));
        g.setFont (fontUi (15.0f));
        g.drawText (padText, pad, juce::Justification::centred);
    }
    else
    {
        auto m = top.withSizeKeepingCentre (42, 6).toFloat();
        g.setColour (well);
        g.fillRect (m);
        g.setColour (juce::Colours::black);
        g.drawRect (m);
    }
    g.setColour (over ? ink : ink2);
    g.setFont (fontUi (10.5f));
    g.drawFittedText (label.toUpperCase(), r, juce::Justification::centred, 2, 0.8f);
}

void DragSource::mouseDrag (const juce::MouseEvent&)
{
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! c->isDragAndDropActive())
            c->startDragging (modSourceDragId (source), this);
}

void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items)
{
    const int n = (int) items.size();
    for (int i = 0; i < n; ++i)
        if (items[(size_t) i] != nullptr)
            items[(size_t) i]->setBounds (area.removeFromLeft (area.getWidth() / (n - i)));
}
