// The patches stored on the synth (4 banks x 128) as far as they were read, plus .syx files with patches.
#pragma once

#include "Sysex.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace peak
{

constexpr int kBanks = 4;     // A-D
constexpr int kSlots = 128;

std::string slotLabel (int bank, int slot);  // "A 000" .. "D 127"

class Library
{
public:
    struct Entry
    {
        int bank = 1, slot = 0;
        bool loaded = false;
        PatchData data {};
        std::string name, category;
    };

    Library();

    void set (int bank, int slot, const PatchData& data);
    void clear();
    const Entry& at (int bank, int slot) const;
    int loadedCount() const;

    // Entries whose name contains `text` (any case; "" = all) and whose category is `category` ("" = any).
    std::vector<const Entry*> filter (const std::string& text, const std::string& category, bool loadedOnly) const;

private:
    std::vector<Entry> entries;  // bank-major
};

// All patch messages in a .syx file's bytes (one or many messages back to back). Other messages are skipped.
std::vector<PatchMessage> parseSyx (const std::vector<uint8_t>& bytes);

// A .syx file for one patch: an edit-buffer message (loading it never writes a slot).
std::vector<uint8_t> patchFile (const PatchData& data);

// A .syx file for a whole bank as slot messages (cmd 01). Loading such a file into the synth writes 128 slots.
std::vector<uint8_t> bankFile (const Library& lib, int bank);

} // namespace peak
