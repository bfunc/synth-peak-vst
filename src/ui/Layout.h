// What each panel of the editor shows, as data (plain C++, no JUCE, tested in CoreTests).
//
// A panel lists the parameters it shows as widgets, and a rule for the parameters it owns. Every owned parameter
// that is not shown anywhere goes into that panel's "more" pop-out (the blue ▶), so a parameter added to the table
// appears in the editor without UI code. The tests check that every table parameter is reachable.
#pragma once

#include "core/ParamTable.h"

#include <functional>
#include <string>
#include <vector>

namespace peak::ui
{

enum class Widget
{
    Knob,
    BigKnob,
    SmallKnob,
    Segmented,     // a row of buttons, one per choice
    SegmentedV,    // the same, stacked
    Toggle,        // LED + label
    Choice,        // drop-down
    Stepper        // ‹ name ›
};

struct Item
{
    Widget widget;
    std::string id;     // paramTable() id
    std::string label;  // "" = the table name without its panel prefix
};

struct PanelSpec
{
    std::string key;    // stable id, e.g. "osc1", "filter"
    std::string title;
    std::vector<Item> items;
    std::function<bool (const ParamDef&)> owns;
};

const std::vector<PanelSpec>& panels();
const PanelSpec* panel (const std::string& key);

// The parameters in a panel's pop-out: owned by it, shown on no panel, not owned by an earlier panel.
std::vector<int> moreParams (const std::string& key);

// Items for one of the repeated panels (osc 1-3, the three envelopes, LFO 1-4, matrix slots).
std::vector<Item> oscItems (int n);
std::vector<Item> envItems (const std::string& key);  // "amp", "mod1", "mod2"
std::vector<Item> lfoItems (int n);                     // 1..4
std::vector<Item> modSlotItems (int slot);              // 1..16
std::vector<Item> fxSlotItems (int slot);               // 1..4

} // namespace peak::ui
