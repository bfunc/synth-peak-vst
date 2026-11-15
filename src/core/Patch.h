// A patch as the editor holds it: the raw 510 bytes it came from (so bytes the editor does not understand yet are
// kept), plus one value per paramTable() entry. Conversions between the two, and comparing two value sets.
#pragma once

#include "Sysex.h"

#include <cstdint>
#include <string>
#include <vector>

namespace peak
{

using Values = std::vector<int>;  // one per paramTable() entry, in table order

// Init Patch defaults from the table.
Values defaultValues();

// A blank patch: Init defaults written into zeroed bytes, named "Init Patch".
PatchData initPatch();

struct ReadResult
{
    Values values;
    int read = 0;                     // parameters taken from the patch bytes
    std::vector<size_t> outOfRange;   // indices whose byte held an impossible value (kept from `fallback`)
};

// Values from patch bytes. Parameters whose byte is not mapped yet keep their value from `fallback`.
ReadResult readValues (const PatchData& data, const Values& fallback);

// `base` with every mapped parameter overwritten by `values`. Unmapped bytes stay as they were.
PatchData writeValues (const PatchData& base, const Values& values);

// Indices where the two value sets differ (sizes must match; otherwise every index of the longer one).
std::vector<size_t> differing (const Values& a, const Values& b);

// Category / genre names from the patch (provisional lists, see ParamTable.cpp).
std::string categoryName (const PatchData& data);

} // namespace peak
