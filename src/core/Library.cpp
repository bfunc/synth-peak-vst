#include "Library.h"

#include "Patch.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace peak
{

std::string slotLabel (int bank, int slot)
{
    char buf[16];
    std::snprintf (buf, sizeof (buf), "%c %03d", (char) ('A' + std::clamp (bank, 1, kBanks) - 1), std::clamp (slot, 0, 999));
    return buf;
}

Library::Library() : entries ((size_t) (kBanks * kSlots))
{
    for (int b = 1; b <= kBanks; ++b)
        for (int s = 0; s < kSlots; ++s)
        {
            auto& e = entries[(size_t) ((b - 1) * kSlots + s)];
            e.bank = b;
            e.slot = s;
        }
}

void Library::set (int bank, int slot, const PatchData& data)
{
    if (bank < 1 || bank > kBanks || slot < 0 || slot >= kSlots)
        return;
    auto& e = entries[(size_t) ((bank - 1) * kSlots + slot)];
    e.loaded = true;
    e.data = data;
    e.name = patchName (data);
    e.category = categoryName (data);
}

void Library::clear()
{
    for (auto& e : entries)
    {
        e.loaded = false;
        e.name.clear();
        e.category.clear();
    }
}

const Library::Entry& Library::at (int bank, int slot) const
{
    bank = std::clamp (bank, 1, kBanks);
    slot = std::clamp (slot, 0, kSlots - 1);
    return entries[(size_t) ((bank - 1) * kSlots + slot)];
}

int Library::loadedCount() const
{
    return (int) std::count_if (entries.begin(), entries.end(), [] (const Entry& e) { return e.loaded; });
}

std::vector<const Library::Entry*> Library::filter (const std::string& text, const std::string& category,
                                                    bool loadedOnly) const
{
    auto lower = [] (std::string s)
    {
        std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
        return s;
    };
    const auto needle = lower (text);
    std::vector<const Entry*> out;
    for (const auto& e : entries)
    {
        if (loadedOnly && ! e.loaded)
            continue;
        if (! needle.empty() && lower (e.name).find (needle) == std::string::npos)
            continue;
        if (! category.empty() && e.category != category)
            continue;
        out.push_back (&e);
    }
    return out;
}

std::vector<PatchMessage> parseSyx (const std::vector<uint8_t>& bytes)
{
    std::vector<PatchMessage> out;
    size_t i = 0;
    while (i < bytes.size())
    {
        if (bytes[i] != 0xF0)
        {
            ++i;
            continue;
        }
        size_t j = i + 1;
        while (j < bytes.size() && bytes[j] != 0xF7)
            ++j;
        if (j >= bytes.size())
            break;
        if (auto m = parsePatchMessage (bytes.data() + i, j - i + 1))
            out.push_back (*m);
        i = j + 1;
    }
    return out;
}

std::vector<uint8_t> patchFile (const PatchData& data) { return editBufferDump (data); }

std::vector<uint8_t> bankFile (const Library& lib, int bank)
{
    std::vector<uint8_t> out;
    for (int s = 0; s < kSlots; ++s)
    {
        const auto& e = lib.at (bank, s);
        if (! e.loaded)
            continue;
        const auto m = writeSlotDump (bank, s, e.data);
        out.insert (out.end(), m.begin(), m.end());
    }
    return out;
}

} // namespace peak
