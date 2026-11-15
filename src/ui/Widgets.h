// Parameter widgets. Each binds to one table parameter by id through a JUCE parameter attachment, so it follows the
// host, the synth and every other widget bound to the same parameter. makeWidget() builds the right one for a
// layout Item; panels never construct widgets by hand for table parameters.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/ParamBridge.h"
#include "ui/Layout.h"

#include <functional>
#include <memory>

class ParamWidget;

// What a widget may need from the plugin; passed down instead of the processor.
struct UiContext
{
    ParamBridge& params;
    // A mod source was dropped on the knob for `paramId` (rail / envelope / LFO drag handles).
    std::function<void (int modSource, const std::string& paramId)> onSourceDropped;
    // Long-lived widgets register here so the editor can update modulation rings.
    std::vector<ParamWidget*> registry;
};

// Base: knows its table index and draws nothing special.
class ParamWidget : public juce::Component
{
public:
    ParamWidget (UiContext& ctx, const std::string& id);
    int index() const { return idx; }
    const peak::ParamDef& def() const;
    virtual void setModAmount (double) {}  // knobs show it as an orange ring

protected:
    UiContext& ctx;
    int idx;
};

class ParamKnob : public ParamWidget, public juce::DragAndDropTarget
{
public:
    enum class Size { Small, Normal, Big };
    ParamKnob (UiContext&, const std::string& id, const juce::String& label, Size);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setModAmount (double) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    int preferredHeight() const { return preferredHeight (size); }
    static int preferredWidth (Size s) { return s == Size::Big ? 80 : (s == Size::Small ? 46 : 56); }
    static int preferredHeight (Size s) { return s == Size::Big ? 104 : (s == Size::Small ? 70 : 80); }

private:
    juce::Slider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    juce::String label;
    Size size;
    bool dropHover = false;
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

private:
    void setFromX (int x);
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

std::unique_ptr<ParamWidget> makeWidget (UiContext&, const peak::ui::Item&);

// Label text from an Item, falling back to the table name.
juce::String itemLabel (const peak::ui::Item&);

// Drag source description for mod sources: "modsrc:<n>".
juce::var modSourceDragId (int source);
int modSourceFromDragId (const juce::var&);
