#include "Data/Heroes.h"
#include <algorithm>

namespace RivalsForge::Data {

namespace {

// Ids from the game files (FModel) and the Marvel Rivals wiki; roles as released
const Hero kHeroes[] = {
    { L"1011", L"Hulk", Role::Vanguard },
    { L"1014", L"The Punisher", Role::Duelist },
    { L"1015", L"Storm", Role::Duelist },
    { L"1016", L"Loki", Role::Strategist },
    { L"1017", L"Human Torch", Role::Duelist },
    { L"1018", L"Doctor Strange", Role::Vanguard },
    { L"1020", L"Mantis", Role::Strategist },
    { L"1021", L"Hawkeye", Role::Duelist },
    { L"1022", L"Captain America", Role::Vanguard },
    { L"1023", L"Rocket Raccoon", Role::Strategist },
    { L"1024", L"Hela", Role::Duelist },
    { L"1025", L"Cloak & Dagger", Role::Strategist },
    { L"1026", L"Black Panther", Role::Duelist },
    { L"1027", L"Groot", Role::Vanguard },
    { L"1028", L"Ultron", Role::Strategist },
    { L"1029", L"Magik", Role::Duelist },
    { L"1030", L"Moon Knight", Role::Duelist },
    { L"1031", L"Luna Snow", Role::Strategist },
    { L"1032", L"Squirrel Girl", Role::Duelist },
    { L"1033", L"Black Widow", Role::Duelist },
    { L"1034", L"Iron Man", Role::Duelist },
    { L"1035", L"Venom", Role::Vanguard },
    { L"1036", L"Spider-Man", Role::Duelist },
    { L"1037", L"Magneto", Role::Vanguard },
    { L"1038", L"Scarlet Witch", Role::Duelist },
    { L"1039", L"Thor", Role::Vanguard },
    { L"1040", L"Mister Fantastic", Role::Duelist },
    { L"1041", L"Winter Soldier", Role::Duelist },
    { L"1042", L"Peni Parker", Role::Vanguard },
    { L"1043", L"Star-Lord", Role::Duelist },
    { L"1044", L"Blade", Role::Duelist },
    { L"1045", L"Namor", Role::Duelist },
    { L"1046", L"Adam Warlock", Role::Strategist },
    { L"1047", L"Jeff the Land Shark", Role::Strategist },
    { L"1048", L"Psylocke", Role::Duelist },
    { L"1049", L"Wolverine", Role::Duelist },
    { L"1050", L"Invisible Woman", Role::Strategist },
    { L"1051", L"The Thing", Role::Vanguard },
    { L"1052", L"Iron Fist", Role::Duelist },
    { L"1053", L"Emma Frost", Role::Vanguard },
    { L"1054", L"Phoenix", Role::Duelist },
    { L"1055", L"Angela", Role::Vanguard },
    { L"1056", L"Daredevil", Role::Duelist },
    { L"1057", L"Deadpool", Role::Duelist },   // plays all three roles; listed as a Duelist
    { L"1058", L"Gambit", Role::Strategist },
    { L"1059", L"Elsa Bloodstone", Role::Duelist },
    { L"1060", L"White Fox", Role::Strategist },
    { L"1061", L"Black Cat", Role::Duelist },
    { L"1062", L"Devil Dinosaur", Role::Vanguard },
    { L"1063", L"Cyclops", Role::Duelist },
    { L"1064", L"Jubilee", Role::Strategist },
    { L"1065", L"Rogue", Role::Vanguard },
    { L"1066", L"The Hood", Role::Vanguard },
    { L"1067", L"Gorr the God Butcher", Role::Duelist },
};

} // namespace

const std::vector<Hero>& Heroes() {
    static const std::vector<Hero> sorted = [] {
        std::vector<Hero> list(std::begin(kHeroes), std::end(kHeroes));
        std::sort(list.begin(), list.end(), [](const Hero& a, const Hero& b) { return std::wstring(a.name) < b.name; });
        return list;
    }();
    return sorted;
}

const Hero* FindHero(const std::wstring& id) {
    for (const auto& hero : kHeroes)
        if (id == hero.id) return &hero;
    return nullptr;
}

Color RoleColor(Role role) {
    switch (role) {
        case Role::Vanguard: return Hex(0x4A90E2);
        case Role::Strategist: return Hex(0x3FBF7F);
        default: return Hex(0xE5484D);
    }
}

const char* RoleKey(Role role) {
    switch (role) {
        case Role::Vanguard: return "role_vanguard";
        case Role::Strategist: return "role_strategist";
        default: return "role_duelist";
    }
}

} // namespace RivalsForge::Data
