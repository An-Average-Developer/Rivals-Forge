#pragma once
#include <cstdint>
#include <optional>
#include <string>

// Local date and time (System.DateTime with DateTimeKind.Local), stored as FILETIME ticks
// (100 ns since 1601) in local time so it compares and subtracts like DateTime.
namespace RivalsForge::Time {

using Ticks = int64_t;
inline constexpr Ticks kTicksPerSecond = 10'000'000;

// DateTime.Now
Ticks Now();

// A local DateTime as System.Text.Json writes it: "2025-06-26T16:32:10.1234567+02:00" with the
// local offset of that date, the fraction's trailing zeros dropped (no fraction at all when zero)
std::wstring JsonDate(Ticks local);
// DateTime.Parse of ISO 8601 text: "yyyy-MM-dd[THH:mm[:ss[.fffffff]]][Z|±hh:mm]" (a space works as
// the separator too). With an offset the result is converted to local time, as DateTime.Parse
// does. nullopt when the text isn't a date.
std::optional<Ticks> Parse(const std::wstring& text);

// DateTime.ToString(format) in the culture `locale` ("en-US", "es-ES"...): GetDateFormatEx
// pictures ("MMM d, yyyy")
std::wstring Format(Ticks local, const std::wstring& format, const std::wstring& locale);

} // namespace RivalsForge::Time
