#include "Patch.h"

#include <algorithm>

namespace peak
{

Values defaultValues()
{
    Values v;
    for (const auto& p : paramTable())
        v.push_back (p.def);
    return v;
}

PatchData initPatch()
{
    PatchData d {};
    setPatchName (d, "Init Patch");
    return writeValues (d, defaultValues());
}

ReadResult readValues (const PatchData& data, const Values& fallback)
{
    const auto& t = paramTable();
    ReadResult r;
    r.values = fallback;
    r.values.resize (t.size(), 0);
    for (size_t i = 0; i < t.size(); ++i)
    {
        const int v = readValue (t[i], data);
        if (v < 0)
            continue;
        if (v < t[i].min || v > t[i].max)
        {
            r.outOfRange.push_back (i);
            continue;
        }
        r.values[i] = v;
        ++r.read;
    }
    return r;
}

PatchData writeValues (const PatchData& base, const Values& values)
{
    PatchData d = base;
    const auto& t = paramTable();
    for (size_t i = 0; i < t.size() && i < values.size(); ++i)
        writeValue (t[i], d, values[i]);
    return d;
}

std::vector<size_t> differing (const Values& a, const Values& b)
{
    std::vector<size_t> out;
    const size_t n = std::max (a.size(), b.size());
    for (size_t i = 0; i < n; ++i)
        if (i >= a.size() || i >= b.size() || a[i] != b[i])
            out.push_back (i);
    return out;
}

std::string categoryName (const PatchData& data)
{
    const int i = indexOfId ("category");
    if (i < 0)
        return {};
    const auto& p = paramTable()[(size_t) i];
    const int v = readValue (p, data);
    return v < 0 ? std::string() : formatValue (p, v);
}

} // namespace peak
