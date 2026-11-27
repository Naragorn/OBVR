#pragma once

// The INI's readers of one value each (GetPrivateProfileString with a
// fallback, a word or a number checked and said in the log when it is not
// one), split out of core/Config.cpp. Used by its key table and its steps.

#include "core/Config.h"

namespace obvr::config_read {

bool EqualsIgnoreCase(const char* a, const char* b);
float ReadFloat(const char* section, const char* key, float fallback, const char* path);
UInt32 ReadUInt(const char* section, const char* key, UInt32 fallback, const char* path);
bool ReadBool(const char* section, const char* key, bool fallback, const char* path);
void ReadText(const char* section, const char* key, char* buffer, UInt32 capacity, const char* path);
bool ReadAnchorIsWorld(const char* section, const char* key, bool fallback, const char* path);
bool ReadLegacyWordBool(const char* section, const char* key, bool fallback, const char* falseWord,
                        const char* trueWord, const char* path);
UInt32 ReadKeyCode(const char* section, const char* key, UInt32 fallback, const char* path);
vr::StereoMode ReadStereoMode(vr::StereoMode fallback, const char* path);
bool ReadMenusInWorld(const char* section, const char* key, bool fallback, const char* path);
UInt32 ReadHexColor(const char* section, const char* key, UInt32 fallback, const char* path);

// The words the log says a setting's value in (Config::Load's lines).
inline const char* StereoModeName(vr::StereoMode mode) {
	switch (mode) {
		case vr::StereoMode::None: return "none";
		case vr::StereoMode::AlternateEyes: return "aer";
		case vr::StereoMode::DualPass: return "dual";
	}
	return "?";
}

inline const char* SourceName(vr::TrackerSource source) {
	switch (source) {
		case vr::TrackerSource::None: return "none";
		case vr::TrackerSource::Fixed: return "fixed";
		case vr::TrackerSource::Simulated: return "simulated";
		case vr::TrackerSource::OpenVR: return "openvr";
		case vr::TrackerSource::OpenXR: return "openxr";
	}
	return "?";
}

}  // namespace obvr::config_read
