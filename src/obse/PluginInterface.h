#pragma once

#include "platform/Win32Min.h"

// Minimal replica of the xOBSE plugin interface.
//
// OBVR does not include the xOBSE headers: they drag in the entire game
// object tree and tie the project to MSVC. All that is needed is the layout
// of these two structs, and it has to be binary compatible with
// xOBSE/obse/PluginAPI.h.
//
// Oblivion Reloaded does the same thing.

namespace obvr::obse {

struct Interface {
	UInt32 obseVersion;
	UInt32 oblivionVersion;
	UInt32 editorVersion;
	UInt32 isEditor;

	bool (*RegisterCommand)(void* info);
	void (*SetOpcodeBase)(UInt32 opcode);
	void* (*QueryInterface)(UInt32 id);

	// From OBSE 15 onwards.
	UInt32 (*GetPluginHandle)();

	// From OBSE 18 onwards.
	bool (*RegisterTypedCommand)(void* info, UInt32 returnType);
	const char* (*GetOblivionDirectory)();

	// From OBSE 21 onwards.
	bool (*GetPluginLoaded)(const char* pluginName);
	UInt32 (*GetPluginVersion)(const char* pluginName);
};

struct Info {
	enum {
		// xOBSE rejects plugins with infoVersion < 2; 3 is the current value
		// from obse/PluginAPI.h.
		kInfoVersion = 3,
	};

	UInt32 infoVersion;
	const char* name;
	UInt32 version;
};

}  // namespace obvr::obse
