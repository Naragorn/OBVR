#pragma once

// The log of every run kept, not only the last one (Nadi, 2026-10-29: "heute
// mehrere obvr runs gehabt. lass uns doch statt einfach dem letzten mehrere
// logs sichern"). At each start the previous OBVR.log is copied into
// OBVR-logs\ beside it, named by when it was last written, and the oldest
// copies beyond a count are removed. OBVR.log.prev stays as it was: the
// harness and the bug reports read it.
//
// Pure: the name and which copies go. core/Log.cpp does the files.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace obvr::log {

inline constexpr const char* kArchiveFolder = "OBVR-logs";
inline constexpr unsigned kArchiveKeep = 30;

// "OBVR-2026-10-29_12-49-03.log": sortable, so the name's order is the time's.
inline std::string ArchiveName(unsigned year, unsigned month, unsigned day, unsigned hour, unsigned minute,
                               unsigned second) {
	char name[64];
	std::snprintf(name, sizeof(name), "OBVR-%04u-%02u-%02u_%02u-%02u-%02u.log", year, month, day, hour, minute,
	              second);
	return name;
}

// A FILETIME (100 ns ticks since 1601-01-01) as a calendar date and time, in
// whatever zone the ticks are in (Log.cpp hands it local time). The civil
// date from a day count after Howard Hinnant's days_from_civil inverse.
struct CivilTime {
	unsigned year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
};

inline CivilTime CivilFromFileTime(unsigned long long ticks) {
	const unsigned long long seconds = ticks / 10000000ull;
	const long long unixSeconds = static_cast<long long>(seconds) - 11644473600ll;
	long long days = unixSeconds >= 0 ? unixSeconds / 86400 : -((-unixSeconds + 86399) / 86400);
	const long long rest = unixSeconds - days * 86400;
	CivilTime t;
	t.hour = static_cast<unsigned>(rest / 3600);
	t.minute = static_cast<unsigned>((rest % 3600) / 60);
	t.second = static_cast<unsigned>(rest % 60);
	days += 719468;
	const long long era = (days >= 0 ? days : days - 146096) / 146097;
	const long long doe = days - era * 146097;
	const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const long long mp = (5 * doy + 2) / 153;
	t.day = static_cast<unsigned>(doy - (153 * mp + 2) / 5 + 1);
	t.month = static_cast<unsigned>(mp < 10 ? mp + 3 : mp - 9);
	t.year = static_cast<unsigned>(yoe + era * 400 + (t.month <= 2 ? 1 : 0));
	return t;
}

inline std::string ArchiveNameFor(unsigned long long ticks) {
	const CivilTime t = CivilFromFileTime(ticks);
	return ArchiveName(t.year, t.month, t.day, t.hour, t.minute, t.second);
}

// Whether a file name is one of the archive's (so nothing else in the folder
// is ever removed).
inline bool IsArchiveName(const std::string& name) {
	if (name.size() != std::string("OBVR-2026-10-29_12-49-03.log").size()) return false;
	if (name.compare(0, 5, "OBVR-") != 0 || name.compare(name.size() - 4, 4, ".log") != 0) return false;
	for (size_t i = 5; i < name.size() - 4; ++i) {
		const char c = name[i];
		const bool digit = c >= '0' && c <= '9';
		const bool sep = (i == 9 || i == 12 || i == 18 || i == 21) ? c == '-' : (i == 15 ? c == '_' : false);
		if (!(digit || sep) || (digit && (i == 9 || i == 12 || i == 15 || i == 18 || i == 21))) return false;
	}
	return true;
}

// The archive's names to remove so that keep remain: the oldest first. Names
// that are not the archive's are left out of it.
inline std::vector<std::string> ArchivesToRemove(std::vector<std::string> names, unsigned keep) {
	names.erase(std::remove_if(names.begin(), names.end(), [](const std::string& n) { return !IsArchiveName(n); }),
	            names.end());
	std::sort(names.begin(), names.end());
	if (names.size() <= keep) return {};
	names.resize(names.size() - keep);
	return names;
}

}  // namespace obvr::log
