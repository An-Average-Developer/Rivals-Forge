#include "Models/AppData.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Core/Time.h"

namespace RivalsForge::Models {

namespace {

// One property read the way System.Text.Json binds it: missing = keep the default, null = the
// default of a reference type, any other mismatch = the whole read fails (false)
bool ReadString(const Json::Value& obj, const char* name, std::wstring& out) {
    const Json::Value& v = obj[name];
    if (!obj.Has(name) || v.IsNull()) return true;
    if (!v.IsString()) return false;
    out = v.WStr();
    return true;
}

bool ReadOptionalString(const Json::Value& obj, const char* name, std::optional<std::wstring>& out) {
    const Json::Value& v = obj[name];
    if (!obj.Has(name)) return true;
    if (v.IsNull()) {
        out.reset();
        return true;
    }
    if (!v.IsString()) return false;
    out = v.WStr();
    return true;
}

bool ReadBool(const Json::Value& obj, const char* name, bool& out) {
    if (!obj.Has(name)) return true;
    const Json::Value& v = obj[name];
    if (!v.IsBool()) return false;   // bool isn't nullable: null fails too
    out = v.Bool();
    return true;
}

bool ReadStringList(const Json::Value& obj, const char* name, std::vector<std::wstring>& out) {
    const Json::Value& v = obj[name];
    if (!obj.Has(name) || v.IsNull()) return true;
    if (!v.IsArray()) return false;
    out.clear();
    for (const auto& item : v.Elements()) {
        if (!item.IsString() && !item.IsNull()) return false;
        out.push_back(item.WStr());
    }
    return true;
}

std::optional<Mod> ReadMod(const Json::Value& obj) {
    if (!obj.IsObject()) return std::nullopt;
    Mod mod;
    std::optional<std::wstring> id, addedAt;
    if (!ReadOptionalString(obj, "Id", id) || !ReadString(obj, "Name", mod.name) || !ReadStringList(obj, "Files", mod.files) ||
        !ReadBool(obj, "IsEnabled", mod.isEnabled) || !ReadOptionalString(obj, "SectionId", mod.sectionId) ||
        !ReadString(obj, "ZipFileName", mod.zipFileName) || !ReadOptionalString(obj, "Character", mod.character))
        return std::nullopt;
    if (obj.Has("NexusModId")) {
        if (!obj["NexusModId"].IsNumber()) return std::nullopt;
        mod.nexusId = obj["NexusModId"].Int();
        mod.nexusVersion = obj["NexusVersion"].WStr();
    }
    if (obj.Has("Target")) {
        if (!obj["Target"].IsString()) return std::nullopt;
        mod.target = obj["Target"].WStr();
        mod.targetStrip = obj["TargetStrip"].IsString() ? obj["TargetStrip"].WStr() : L"";
    }
    // AddedAt is a DateTime: a string that parses as a date (null or anything else fails)
    if (obj.Has("AddedAt")) {
        const Json::Value& v = obj["AddedAt"];
        if (!v.IsString() || !Time::Parse(v.WStr())) return std::nullopt;
        addedAt = v.WStr();
    }
    // The property initializers (Guid.NewGuid(), DateTime.Now) for what the file doesn't have
    mod.id = id ? *id : System::NewGuid();
    mod.addedAt = addedAt ? *addedAt : Time::JsonDate(Time::Now());
    return mod;
}

std::optional<Section> ReadSection(const Json::Value& obj) {
    if (!obj.IsObject()) return std::nullopt;
    Section section;
    std::optional<std::wstring> id;
    if (!ReadOptionalString(obj, "Id", id) || !ReadString(obj, "Name", section.name) || !ReadString(obj, "Icon", section.icon) ||
        !ReadBool(obj, "IsExpanded", section.isExpanded))
        return std::nullopt;
    section.id = id ? *id : System::NewGuid();
    return section;
}

template <typename T, typename ReadFn>
bool ReadList(const Json::Value& obj, const char* name, std::vector<T>& out, ReadFn read) {
    const Json::Value& v = obj[name];
    if (!obj.Has(name) || v.IsNull()) return true;
    if (!v.IsArray()) return false;
    for (const auto& item : v.Elements()) {
        auto parsed = read(item);
        if (!parsed) return false;
        out.push_back(std::move(*parsed));
    }
    return true;
}

Json::Value WriteMod(const Mod& mod) {
    Json::Value files = Json::Value::MakeArray();
    for (const auto& f : mod.files) files.Push(f);
    // Property order = declaration order in Mod.cs
    Json::Value v = Json::Value::MakeObject();
    v.Set("Id", mod.id);
    v.Set("Name", mod.name);
    v.Set("Files", files);
    v.Set("IsEnabled", mod.isEnabled);
    v.Set("AddedAt", Json::Value::Verbatim(Text::Narrow(mod.addedAt)));   // a DateTime: not escaped
    v.Set("SectionId", mod.sectionId ? Json::Value(*mod.sectionId) : Json::Value());
    v.Set("ZipFileName", mod.zipFileName);
    v.Set("Character", mod.character ? Json::Value(*mod.character) : Json::Value());
    if (mod.nexusId != 0) {
        v.Set("NexusModId", mod.nexusId);
        v.Set("NexusVersion", mod.nexusVersion);
    }
    if (!mod.target.empty()) {
        v.Set("Target", mod.target);
        v.Set("TargetStrip", mod.targetStrip);
    }
    return v;
}

Json::Value WriteSection(const Section& section) {
    Json::Value v = Json::Value::MakeObject();
    v.Set("Id", section.id);
    v.Set("Name", section.name);
    v.Set("Icon", section.icon);
    v.Set("IsExpanded", section.isExpanded);
    return v;
}

} // namespace

std::optional<AppData> ReadAppData(const Json::Value& root) {
    AppData data;
    if (root.IsNull()) return data;   // "null": Deserialize returns null, nothing is loaded
    if (!root.IsObject()) return std::nullopt;
    if (!ReadOptionalString(root, "GamePath", data.gamePath) || !ReadList(root, "Sections", data.sections, ReadSection) ||
        !ReadList(root, "Mods", data.mods, ReadMod))
        return std::nullopt;
    return data;
}

std::optional<std::vector<Mod>> ReadModList(const Json::Value& root) {
    std::vector<Mod> mods;
    if (root.IsNull()) return mods;
    if (!root.IsArray()) return std::nullopt;
    for (const auto& item : root.Elements()) {
        auto mod = ReadMod(item);
        if (!mod) return std::nullopt;
        mods.push_back(std::move(*mod));
    }
    return mods;
}

Json::Value WriteAppData(const AppData& data) {
    Json::Value sections = Json::Value::MakeArray(), mods = Json::Value::MakeArray();
    for (const auto& s : data.sections) sections.Push(WriteSection(s));
    for (const auto& m : data.mods) mods.Push(WriteMod(m));
    Json::Value v = Json::Value::MakeObject();
    v.Set("GamePath", data.gamePath ? Json::Value(*data.gamePath) : Json::Value());
    v.Set("Sections", sections);
    v.Set("Mods", mods);
    return v;
}

} // namespace RivalsForge::Models
