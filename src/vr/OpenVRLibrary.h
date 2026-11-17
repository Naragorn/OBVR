#pragma once

// Which library the backend reaches OpenVR through (vr/OpenVRBackend.cpp,
// Start): openvr_api.dll as shipped, or a stand-in named by [Debug]
// OpenVRLibrary.
//
// The stand-in is for the harness (docs/hand-script-harness.md, "Without
// SteamVR"): tools/mock-openvr builds openvr_mock.dll, which answers as a
// headset that is connected and asleep, with no SteamVR running. The runner's
// --no-steamvr puts it beside OBVR.dll and names it in the run's
// OBVR-test.ini, which OBVR reads only with the run's token (core/HarnessToken.h).
//
// A stand-in that was asked for and is not there is not replaced by
// openvr_api.dll: a run that meant to stay away from SteamVR must not take
// the headset from whoever wears it.

#include <cstring>

namespace obvr::vr {

constexpr const char* kShippedOpenVRLibrary = "openvr_api.dll";

enum class OpenVRLibrary : unsigned char {
	Shipped,  // openvr_api.dll: beside OBVR.dll, else beside Oblivion.exe
	StandIn,  // the named file beside OBVR.dll, and only that
	Refused,  // the name is not a plain DLL's: nothing is loaded
};

struct OpenVRLibraryPlan {
	OpenVRLibrary kind = OpenVRLibrary::Shipped;
	const char* name = kShippedOpenVRLibrary;
};

inline char LowerAscii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

inline bool SameNameNoCase(const char* a, const char* b) {
	for (; *a != 0 && *b != 0; ++a, ++b) {
		if (LowerAscii(*a) != LowerAscii(*b)) {
			return false;
		}
	}
	return *a == *b;
}

// A file's name and nothing more: letters, digits, '_', '-' and '.', ending
// in ".dll" with something before it. No folder, no drive, no "..": the file
// is looked for beside OBVR.dll alone.
inline bool PlainDllName(const char* name) {
	if (name == nullptr) {
		return false;
	}
	const size_t length = std::strlen(name);
	if (length < 5 || !SameNameNoCase(name + length - 4, ".dll")) {
		return false;
	}
	for (size_t i = 0; i < length; ++i) {
		const char c = name[i];
		const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
		                   c == '-' || c == '.';
		if (!plain || (c == '.' && i + 1 < length && name[i + 1] == '.')) {
			return false;
		}
	}
	return true;
}

// `configured` is [Debug] OpenVRLibrary: empty for the shipped library, as is
// its own name.
inline OpenVRLibraryPlan PlanOpenVRLibrary(const char* configured) {
	if (configured == nullptr || configured[0] == 0 || SameNameNoCase(configured, kShippedOpenVRLibrary)) {
		return OpenVRLibraryPlan{};
	}
	if (!PlainDllName(configured)) {
		return OpenVRLibraryPlan{OpenVRLibrary::Refused, configured};
	}
	return OpenVRLibraryPlan{OpenVRLibrary::StandIn, configured};
}

// Loads the library planned for `configured` (vr/OpenVRLibrary.cpp) and
// names it in `name`; null, with the reason in the log, when there is none.
void* LoadOpenVRLibrary(const char* configured, const char*& name);

}  // namespace obvr::vr
