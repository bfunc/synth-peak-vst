// Parameter widgets. Each binds to one table parameter by id through a JUCE parameter attachment, so it follows the
// host, the synth and every other widget bound to the same parameter. makeWidget() builds the right one for a
// layout Item; panels never construct widgets by hand for table parameters.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/ParamBridge.h"
#include "ui/Layout.h"
#include "ui/ModMap.h"

#include <functional>
#include <memory>

class ParamWidget;

// What a widget may need from the plugin; passed down instead of the processor.
struct UiContext
{
    ParamBridge& params;
    // A mod source was dropped on the knob for `paramId` (rail / envelope / LFO drag handles).
    std::function<void (int modSource, const std::string& paramId)> onSourceDropped;
    // Long-lived widgets register here so the editor can update modulation rings and saved marks.
    std::vector<ParamWidget*> registry;
    // The saved patch's value of a parameter (-1: no saved patch yet).
    std::function<int (int index)> savedValue;
    // The mod slot picked last (dropped, or chosen in a knob's slot list): Alt + wheel sets its depth.
    peak::ui::SlotRef focus;
    // A plain click on a modulated knob: show its slots. Alt + wheel on a knob: change a slot's depth.
    std::function<void (const std::string& paramId, juce::Component& knob)> onModClick;
    std::function<void (const std::string& paramId, int steps)> onModWheel;
};

// What a knob's orange ring shows, in knob ranges (1 = the whole knob): the reach of all slots aimed at it, and the
// selected slot alone when it is one of them.
struct ModInfo
{
    double lo = 0, hi = 0;
    bool focus = false;
    double flo = 0, fhi = 0;
    bool any() const { return lo != 0 || hi != 0; }
    bool operator== (const ModInfo& o) const
    {
        return lo == o.lo && hi == o.hi && focus == o.focus && flo == o.flo && fhi == o.fhi;
    }
};

// Base: knows its table index and draws nothing special.
class ParamWidget : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParamWidget (UiContext& ctx, const std::string& id);
    int index() const { return idx; }
    const peak::ParamDef& def() const;
    virtual void setModInfo (const ModInfo&) {}  // knobs show it as an orange ring
    virtual void setSaved (int) {}                // knobs mark the saved value (-1: none)

protected:
    UiContext& ctx;
    int idx;
};

// The rotary slider of a knob: Alt + wheel goes to the editor (mod depth) instead of turning the knob.
class KnobSlider : public juce::Slider
{
public:
    std::function<bool (int steps)> altWheel;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isAltDown() && altWheel)
        {
            acc += w.deltaY != 0 ? w.deltaY : w.deltaX;
            const int steps = (int) (acc / 0.08f);
            if (steps != 0)
            {
                acc -= (float) steps * 0.08f;
                altWheel (steps);
            }
            return;
        }
        juce::Slider::mouseWheelMove (e, w);
    }

private:
    float acc = 0;
};

class ParamKnob : public ParamWidget, public juce::DragAndDropTarget
{
public:
    enum class Size { Small, Normal, Big };
    ParamKnob (UiContext&, const std::string& id, const juce::String& label, Size);
    ~ParamKnob() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void setModInfo (const ModInfo&) override;
    void setSaved (int) override;
    void mouseUp (const juce::MouseEvent&) override;            // from the slider: a plain click shows the slots
    void mouseDoubleClick (const juce::MouseEvent&) override;   // from the slider

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    int preferredHeight() const { return preferredHeight (size); }
    void setShowValue (bool s) { showValue = s; repaint(); }  // false: knob + label only (top bar)
    static int preferredWidth (Size s) { return s == Size::Big ? 80 : (s == Size::Small ? 46 : 56); }
    static int preferredHeight (Size s) { return s == Size::Big ? 104 : (s == Size::Small ? 70 : 80); }

private:
    KnobSlider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    ModInfo mod;
    int saved = -1;
    int clickToken = 0;
    juce::String label;
    Size size;
    bool dropHover = false;
    bool showValue = true;
};

class ParamSegment : public ParamWidget
{
public:
    ParamSegment (UiContext&, const std::string& id, bool vertical, const juce::StringArray& shortNames = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    void setColumns (int c) { columns = c; resized(); }  // > 0: a grid with this many columns

private:
    void show (float plain);
    bool vertical;
    int columns = 0;
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

class ParamToggle : public ParamWidget
{
public:
    ParamToggle (UiContext&, const std::string& id, const juce::String& label);
    void resized() override { button.setBounds (getLocalBounds()); }

private:
    juce::ToggleButton button;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

class ParamChoice : public ParamWidget
{
public:
    ParamChoice (UiContext&, const std::string& id);
    void resized() override { box.setBounds (getLocalBounds()); }

private:
    juce::ComboBox box;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ‹ value ›, for long lists (wavetables, routing) and plain numbers.
class ParamStepper : public ParamWidget
{
public:
    ParamStepper (UiContext&, const std::string& id, const juce::String& caption = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;  // click on the name: a menu of all values
    void setCaption (const juce::String& c) { caption = c; repaint(); }

private:
    void stepBy (int);
    juce::TextButton prev { juce::CharPointer_UTF8 ("\xe2\x80\xb9") }, next { juce::CharPointer_UTF8 ("\xe2\x80\xba") };
    juce::String caption;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// A push button that acts while held: onPress on mouse down, onRelease on mouse up (also if the mouse leaves the
// window or the button goes away while held, so a release is never lost). Lit while held.
class HoldButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
    ~HoldButton() override { release(); }
    std::function<void()> onPress, onRelease;
    void mouseDown (const juce::MouseEvent&) override
    {
        if (! isEnabled() || held)
            return;
        held = true;
        setToggleState (true, juce::dontSendNotification);
        if (onPress)
            onPress();
    }
    void mouseUp (const juce::MouseEvent&) override { release(); }
    void focusLost (FocusChangeType) override { release(); }
    void enablementChanged() override
    {
        juce::TextButton::enablementChanged();
        if (! isEnabled())
            release();
    }
    void release()
    {
        if (! held)
            return;
        held = false;
        setToggleState (false, juce::dontSendNotification);
        if (onRelease)
            onRelease();
    }
    bool isHeld() const { return held; }

private:
    bool held = false;
};

// A horizontal bipolar bar (mod matrix depth).
class ParamBar : public ParamWidget
{
public:
    ParamBar (UiContext&, const std::string& id);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void setFromX (int x);
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float wheelAcc = 0;
};

// A thin horizontal slider: LABEL ━━━━━ value. Click or drag along it, wheel to step; double-click resets.
class ParamSlim : public ParamWidget
{
public:
    ParamSlim (UiContext&, const std::string& id, const juce::String& label);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setDim (bool d) { if (dim != d) { dim = d; repaint(); } }  // greyed (e.g. detune while density is 0)

private:
    juce::Rectangle<int> track() const;
    void setFromX (int x);
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::String label;
    bool dim = false;
    float wheelAcc = 0;
};

std::unique_ptr<ParamWidget> makeWidget (UiContext&, const peak::ui::Item&);

// Label text from an Item, falling back to the table name.
juce::String itemLabel (const peak::ui::Item&);

// Drag source description for mod sources: "modsrc:<n>".
juce::var modSourceDragId (int source);
int modSourceFromDragId (const juce::var&);
