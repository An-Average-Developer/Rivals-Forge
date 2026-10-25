#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Core/Json.h"
#include "Models/Mod.h"
#include "Models/Section.h"

namespace RivalsForge::Models {

// Models\AppData.cs: what data.json holds
struct AppData {
    std::optional<std::wstring> gamePath;   // full path to the Paks directory
    std::vector<Section> sections;
    std::vector<Mod> mods;
};

// JsonSerializer.Deserialize / Serialize with the default options, so data.json stays readable by
// the C# app and the other way round: PascalCase names matched case-sensitively, unknown ones
// ignored, missing ones left at their defaults, and a value of the wrong type fails the whole read
// (nullopt), as the JsonException did.
std::optional<AppData> ReadAppData(const Json::Value& root);
// mods.json (before data.json): a bare list of mods
std::optional<std::vector<Mod>> ReadModList(const Json::Value& root);
Json::Value WriteAppData(const AppData& data);

} // namespace RivalsForge::Models
