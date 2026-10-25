#pragma once
#include <string>
#include <vector>
#include "Graphics/Color.h"

// The playable heroes, by the id the game files use (Marvel/Content/Marvel/Characters/<id>/...):
// the name (brand, not translated), the role and the portrait in Assets\Heroes\<id>.png
// (Tools\FetchHeroPortraits.py). A mod belongs to the hero whose id its files use.
namespace RivalsForge::Data {

enum class Role { Vanguard, Duelist, Strategist };

struct Hero {
    const wchar_t* id;     // "1020"
    const wchar_t* name;   // "Mantis"
    Role role;
};

// Every hero, by name
const std::vector<Hero>& Heroes();
const Hero* FindHero(const std::wstring& id);

// The role's colour (Vanguard blue, Duelist red, Strategist green) and its translation key
Color RoleColor(Role role);
const char* RoleKey(Role role);

} // namespace RivalsForge::Data
