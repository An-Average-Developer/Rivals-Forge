#pragma once
#include <string>

// Presentation mode (Config::PresentationMode): what's drawn on screen without anything personal.
// The Windows user name in paths and the names remembered with Remember (the Nexus account) become
// Config::PresentationName. Only for display: nothing is changed on disk or in what the app uses.
namespace RivalsForge::Services::Privacy {

bool Active();
// `text` as it may be shown (unchanged when presentation mode is off)
std::wstring Mask(const std::wstring& text);
// A personal name the app learnt while running (the Nexus account), masked from then on
void Remember(const std::wstring& name);

} // namespace RivalsForge::Services::Privacy
