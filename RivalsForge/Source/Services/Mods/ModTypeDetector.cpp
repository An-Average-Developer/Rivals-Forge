#include "Services/Mods/ModTypeDetector.h"
#include <memory>
#include "Core/Text.h"
#include "Core/Zip.h"

namespace RivalsForge::Services::Mods::ModTypeDetector {

namespace {

// A .pak of an old-style mod is scanned up to this size (its index, with the paths, is at the end)
constexpr uint64_t kMaxPakScan = 256ull << 20;

bool Starts(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }
bool Has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }
bool Ends(const std::string& s, const char* suffix) {
    const size_t n = std::char_traits<char>::length(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

// What one name says: a folder ("Textures", "UI", "WwiseAudio") or a file ("SK_1050_1050001.uasset")
unsigned Classify(std::string name) {
    for (char& c : name) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
    // Paths come whole too ("../../../Marvel/Content/Marvel/UI/..."): each part on its own
    if (Has(name, "/")) {
        unsigned types = 0;
        for (const auto& part : Text::Split(name, '/', true)) types |= Classify(part);
        return types;
    }
    const size_t dot = name.rfind('.');
    const std::string ext = dot == std::string::npos ? "" : name.substr(dot);
    std::string base = dot == std::string::npos ? name : name.substr(0, dot);
    while (!base.empty() && base[0] >= '0' && base[0] <= '9') base.erase(0, 1);   // "001MI_..." ordering prefixes
    if (base.empty()) return 0;

    if (ext.empty()) {
        // A folder
        if (base == "TEXTURES" || base == "TEXTURE") return Texture;
        if (base == "MATERIALS" || base == "MATERIAL") return Material;
        if (base == "ANIMATION" || base == "ANIMATIONS" || base == "ANIMS" || base == "ANIM") return Animation;
        if (base == "VFX" || base == "FX" || base == "EFFECT" || base == "EFFECTS" || base == "NIAGARA" || base == "PARTICLES") return VFX;
        if (base == "WWISEAUDIO" || base == "AUDIO" || base == "SOUND" || base == "SOUNDS" || base == "VOICE" || base == "VO") return Audio;
        if (base == "UI" || base == "UMG" || base == "HUD") return UI;
        return 0;
    }
    if (ext == ".BNK" || ext == ".WEM") return Audio;
    if (ext == ".BK2" || ext == ".BIK") return Video;
    if (ext != ".UASSET" && ext != ".UBULK" && ext != ".UEXP" && ext != ".UPTNL") return 0;
    if (Has(base, "PHYSICSASSET") || Starts(base, "PHYS_")) return 0;   // goes with a mesh, says nothing alone
    if (Starts(base, "SK_") || Starts(base, "SM_") || Starts(base, "SKM_")) return Mesh;
    if (Starts(base, "MI_") || Starts(base, "M_") || Starts(base, "MAT_") || Ends(base, "_INST")) return Material;
    if (Starts(base, "NS_") || Starts(base, "NE_") || Starts(base, "FX_") || Starts(base, "P_") || Starts(base, "VFX_")) return VFX;
    if (Starts(base, "AS_") || Starts(base, "AM_") || Starts(base, "ABP_") || Starts(base, "BS_") || Starts(base, "ANIM_")) return Animation;
    if (Starts(base, "WBP_") || Starts(base, "UI_") || Starts(base, "IMG_") || Starts(base, "ICON_")) return UI;
    if (Starts(base, "T_") || Starts(base, "TX_") || Has(base, "TEX") || Has(base, "NORMAL") || Has(base, "SPECULAR") || Has(base, "EMISSIVE") ||
        base == "ORM" || Ends(base, "_ORM") || Ends(base, "_D") || Ends(base, "_N"))
        return Texture;
    return 0;
}

// Collects the names in a stream of bytes (a .utoc / .pak index) and classifies each. A name is
// an FString: its length (with the terminating zero) as 4 bytes, the characters, then a zero; only
// readable runs framed like that count, so a stray "UI" in binary data isn't taken for a folder.
class Scanner {
public:
    void Feed(const uint8_t* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            const uint8_t c = data[i];
            if (c >= 0x20 && c < 0x7F) {
                if (run.empty()) prefix = last;
                if (run.size() < 512) run += static_cast<char>(c);
            } else {
                End(c);
            }
            last = last >> 8 | static_cast<uint32_t>(c) << 24;   // the 4 bytes before the next one
        }
    }
    void End(int terminator = -1) {
        if (run.size() >= 2 && terminator == 0 && prefix == run.size() + 1) types |= Classify(run);
        run.clear();
    }
    unsigned types = 0;

private:
    std::string run;
    uint32_t last = 0, prefix = 0;
};

} // namespace

unsigned Detect(const std::wstring& zipPath) {
    const std::unique_ptr<Zip::Reader> archive = Zip::Reader::Open(zipPath);
    if (!archive) return 0;
    Scanner scanner;
    const auto scan = [&](const Zip::Entry& entry) {
        // A damaged entry still gives whatever it had read before failing
        archive->ReadEntry(entry, [&](const uint8_t* data, size_t size) {
            scanner.Feed(data, size);
            return true;
        });
        scanner.End();
    };
    bool sawToc = false;
    for (const auto& entry : archive->Entries()) {
        // Loose game files in the archive count too: audio, movies
        const std::wstring ext = Text::Lower(entry.name.substr(entry.name.rfind(L'.') == std::wstring::npos ? entry.name.size() : entry.name.rfind(L'.')));
        if (ext == L".bnk" || ext == L".wem") scanner.types |= Audio;
        if (ext == L".bk2" || ext == L".bik" || ext == L".mp4") scanner.types |= Video;
        if (ext != L".utoc") continue;
        sawToc = true;
        scan(entry);
    }
    if (!sawToc)
        for (const auto& entry : archive->Entries())
            if (Text::EndsWith(entry.name, L".pak", true) && entry.length <= kMaxPakScan) scan(entry);
    return scanner.types;
}

} // namespace RivalsForge::Services::Mods::ModTypeDetector
