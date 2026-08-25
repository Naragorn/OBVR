#pragma once

#include "platform/Win32Min.h"

// Minimale Nachbildung der xOBSE-Plugin-Schnittstelle.
//
// OBVR bindet die xOBSE-Header nicht ein: die ziehen den kompletten
// Spielobjektbaum nach sich und binden das Projekt an MSVC. Gebraucht wird
// nur das Layout dieser beiden Strukturen, und das muss binaerkompatibel zu
// xOBSE/obse/PluginAPI.h sein.
//
// Oblivion Reloaded macht es genauso.

namespace obvr::obse {

struct Interface {
	UInt32 obseVersion;
	UInt32 oblivionVersion;
	UInt32 editorVersion;
	UInt32 isEditor;

	bool (*RegisterCommand)(void* info);
	void (*SetOpcodeBase)(UInt32 opcode);
	void* (*QueryInterface)(UInt32 id);

	// Ab OBSE 15.
	UInt32 (*GetPluginHandle)();

	// Ab OBSE 18.
	bool (*RegisterTypedCommand)(void* info, UInt32 returnType);
	const char* (*GetOblivionDirectory)();

	// Ab OBSE 21.
	bool (*GetPluginLoaded)(const char* pluginName);
	UInt32 (*GetPluginVersion)(const char* pluginName);
};

struct Info {
	enum {
		// xOBSE lehnt Plugins mit infoVersion < 2 ab; 3 ist der aktuelle Wert
		// aus obse/PluginAPI.h.
		kInfoVersion = 3,
	};

	UInt32 infoVersion;
	const char* name;
	UInt32 version;
};

}  // namespace obvr::obse
