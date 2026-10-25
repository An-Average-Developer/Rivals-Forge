#include "Services/Mods/CharacterDetector.h"
#include <map>
#include <memory>
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Data/Heroes.h"

namespace RivalsForge::Services::Mods::CharacterDetector {

namespace {

// A .pak of an old-style mod is scanned up to this size (its index, with the paths, is at the end)
constexpr uint64_t kMaxPakScan = 256ull << 20;

// Finds the hero ids in a stream of bytes: a run of 4 digits ("1020", a path component) or 7
// ("1020001", a skin) starting with "10", inside readable text, with no digit right before or after
class IdCounter {
public:
    void Feed(const uint8_t* data, size_t size) {
        for (size_t i = 0; i < size; ++i) Byte(data[i]);
    }
    void Finish() { EndDigits(); }

    std::optional<std::wstring> Best() const {
        const std::map<std::wstring, int>::value_type* best = nullptr;
        for (const auto& entry : counts)
            if (!best || entry.second > best->second) best = &entry;
        if (!best) return std::nullopt;
        return best->first;
    }

private:
    void Byte(uint8_t c) {
        const bool printable = c >= 0x20 && c < 0x7F;
        if (c >= '0' && c <= '9') {
            if (digits.size() < 8) digits += static_cast<wchar_t>(c);
            else tooLong = true;
        } else {
            EndDigits();
        }
        printableRun = printable ? printableRun + 1 : 0;
    }

    void EndDigits() {
        // The run has to sit in text at least 4 readable characters long (itself, or more)
        if (!tooLong && (digits.size() == 4 || digits.size() == 7) && digits[0] == L'1' && digits[1] == L'0' && printableRun >= 4) {
            const std::wstring id = digits.substr(0, 4);
            if (Data::FindHero(id)) ++counts[id];
        }
        digits.clear();
        tooLong = false;
    }

    std::wstring digits;
    bool tooLong = false;
    size_t printableRun = 0;
    std::map<std::wstring, int> counts;
};

} // namespace

std::optional<std::wstring> Detect(const std::wstring& zipPath) {
    const std::unique_ptr<Zip::Reader> archive = Zip::Reader::Open(zipPath);
    if (!archive) return std::nullopt;

    IdCounter counter;
    bool sawToc = false;
    const auto scan = [&](const Zip::Entry& entry) {
        // A damaged entry still gives whatever it had read before failing
        archive->ReadEntry(entry, [&](const uint8_t* data, size_t size) {
            counter.Feed(data, size);
            return true;
        });
        counter.Finish();
    };
    for (const auto& entry : archive->Entries()) {
        if (!Text::EndsWith(entry.name, L".utoc", true)) continue;
        sawToc = true;
        scan(entry);
    }
    if (!sawToc) {
        for (const auto& entry : archive->Entries())
            if (Text::EndsWith(entry.name, L".pak", true) && entry.length <= kMaxPakScan) scan(entry);
    }
    return counter.Best();
}

} // namespace RivalsForge::Services::Mods::CharacterDetector
