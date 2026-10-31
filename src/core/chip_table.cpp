#include "newisp/chip_table.h"

#include "newisp/chip_logic.h"
#include "newisp/i18n.h"

// The English spelling of UnknownChipName(). Kept as a stable identifier for
// the tests, which compare against it by name rather than by text: the visible
// placeholder follows the build language, this constant does not.
const char* const kUnknownChipName = "unknown part";

std::string UnknownChipName()
{
    // Prose, not a part name, so it follows the UI language like any other
    // message: the chip ID belongs to no part the table knows.
    return newisp::i18n::Tr(newisp::i18n::Msg::ChipUnknownPlaceholder);
}

std::string ChipNameFromTable(uint16_t magic)
{
    auto it = kChipTable.find(magic);
    if (it == kChipTable.end()) return UnknownChipName();
    return std::string(it->second);
}

std::string ListFrequencyMenu(const std::string& chipName)
{
    newisp::FrequencyMenu menu = newisp::FrequencyMenuFor(chipName);
    std::string out;
    for (int i = 0; i <= menu.maxIdx; ++i) {
        if (!out.empty()) out += ", ";
        out += newisp::FormatDouble(menu.hz[i] / 1e6, 4);
        // No "(no trim)" marking here: the trim table is private to the
        // protocol classes, which cannot be consulted from this file without
        // pulling them into every includer of the chip table. The menu is what
        // this list is for -- the option stage stops on the entries it has no
        // triple for, and says which frequency it stopped on.
    }
    return out;
}
