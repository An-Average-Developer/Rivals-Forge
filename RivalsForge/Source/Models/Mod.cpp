#include "Models/Mod.h"
#include "Core/System.h"
#include "Core/Time.h"

namespace RivalsForge::Models {

// new Mod(): Id = Guid.NewGuid().ToString(), AddedAt = DateTime.Now
Mod Mod::New() {
    Mod mod;
    mod.id = System::NewGuid();
    mod.addedAt = Time::JsonDate(Time::Now());
    return mod;
}

} // namespace RivalsForge::Models
