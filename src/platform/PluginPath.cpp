#include "platform/PluginPath.h"

#include "platform/Win32Min.h"

namespace obvr::platform {
namespace {

HMODULE g_pluginModule = nullptr;

// Replaces the file name at the end of a module path with fileName. The
// module path always contains a separator, so cutting at the last one is
// enough - no need for a general path library.
bool ReplaceFileName(char* buffer, UInt32 bufferLength, const char* fileName, UInt32 outSize) {
	UInt32 cut = bufferLength;
	while (cut > 0 && buffer[cut - 1] != '\\' && buffer[cut - 1] != '/') {
		--cut;
	}

	UInt32 i = 0;
	while (fileName[i] != '\0') {
		if (cut + i + 1 >= outSize) {
			return false;
		}
		buffer[cut + i] = fileName[i];
		++i;
	}
	buffer[cut + i] = '\0';
	return true;
}

bool BuildFrom(HMODULE module, const char* fileName, char* out, UInt32 outSize) {
	const DWORD length = GetModuleFileNameA(module, out, outSize);
	if (length == 0 || length >= outSize) {
		return false;
	}
	return ReplaceFileName(out, static_cast<UInt32>(length), fileName, outSize);
}

}  // namespace

void SetPluginModule(void* module) { g_pluginModule = static_cast<HMODULE>(module); }

bool BuildPluginPath(const char* fileName, char* out, UInt32 outSize) {
	if (g_pluginModule == nullptr) {
		return false;
	}
	return BuildFrom(g_pluginModule, fileName, out, outSize);
}

bool BuildGamePath(const char* fileName, char* out, UInt32 outSize) {
	// A null module means the running executable, which is Oblivion.exe.
	return BuildFrom(nullptr, fileName, out, outSize);
}


bool AppendToPath(const char* base, const char* suffix, char* out, UInt32 outSize) {
	UInt32 needed = 0;
	while (base[needed] != '\0') {
		++needed;
	}
	UInt32 suffixLength = 0;
	while (suffix[suffixLength] != '\0') {
		++suffixLength;
	}
	if (needed + suffixLength + 1 > outSize) {
		return false;
	}
	for (UInt32 i = 0; i < needed; ++i) {
		out[i] = base[i];
	}
	for (UInt32 i = 0; i < suffixLength; ++i) {
		out[needed + i] = suffix[i];
	}
	out[needed + suffixLength] = '\0';
	return true;
}

}  // namespace obvr::platform
