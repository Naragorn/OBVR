#include "core/Log.h"

#include <string>
#include <vector>

#include "core/LogArchiveLogic.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"

#if defined(OBVR_NO_WINSDK)
// <cstdarg> is missing in the freestanding cross build; the compiler builtins
// exist regardless, and msvcrt provides _vsnprintf.
using va_list = __builtin_va_list;
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap) __builtin_va_end(ap)
extern "C" int __cdecl _vsnprintf(char* buffer, unsigned int count, const char* format, va_list args);
#else
#include <cstdarg>
#endif

namespace obvr::log {
namespace {

HANDLE g_file = InvalidHandle();

// Enough for one log line; longer ones are truncated rather than grown, so
// that the logger needs no heap.
constexpr UInt32 kLineBufferSize = 1024;

void WriteRaw(const char* text, UInt32 length) {
	OutputDebugStringA(text);
	if (g_file != InvalidHandle()) {
		DWORD written = 0;
		WriteFile(g_file, text, length, &written, nullptr);
	}
}

// The previous run's log kept in OBVR-logs\ beside it, named by its last
// write in local time, the oldest beyond kArchiveKeep removed
// (core/LogArchiveLogic.h). Runs before the new log exists, so it says
// nothing; a copy that cannot be made leaves the start as it was. A copy of
// that name already there is the same run (the harness puts the player's log
// back after its own), kept once.
void ArchivePrevious(const char* path) {
	UInt64 ticks = 0;
	char folder[512];
	if (!ReadLocalWriteTicks(path, ticks) || !platform::BuildGamePath(kArchiveFolder, folder, sizeof(folder))) {
		return;
	}
	CreateDirectoryA(folder, nullptr);
	const std::string prefix = std::string(folder) + "\\";
	CopyFileA(path, (prefix + ArchiveNameFor(ticks)).c_str(), 1);
	std::vector<std::string> names;
	ForEachFileName((prefix + "OBVR-*.log").c_str(), [&](const char* name) { names.emplace_back(name); });
	for (const std::string& name : ArchivesToRemove(names, kArchiveKeep)) {
		DeleteFileA((prefix + name).c_str());
	}
}

}  // namespace

void Open(const char* fileName) {
	// Deliberately next to Oblivion.exe rather than next to OBVR.dll.
	//
	// Under Mod Organizer 2 the plugin directory is virtualised, so a log
	// written there would be redirected into MO2's Overwrite folder. That is
	// correct behaviour, but awkward for a diagnostic file: the log is the
	// first thing a user gets asked to attach to a bug report, and the game
	// root is where every other script extender already writes its own.
	//
	// An absolute path also keeps the file off the working directory, which is
	// not necessarily the game folder.
	char path[512];
	if (!platform::BuildGamePath(fileName, path, sizeof(path))) {
		return;
	}


	ArchivePrevious(path);

	// One start ago is often exactly the run a question is about: the game
	// is restarted, CREATE_ALWAYS truncates, and the log of the run that
	// showed the fault is gone. So the previous log survives one generation
	// under its own name. MOVEFILE_REPLACE_EXISTING is winbase.h's 0x1.
	char previous[512];
	if (platform::AppendToPath(path, ".prev", previous, sizeof(previous))) {
		MoveFileExA(path, previous, 0x1);
	}

	g_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
	                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void Close() {
	if (g_file != InvalidHandle()) {
		CloseHandle(g_file);
		g_file = InvalidHandle();
	}
}

void Write(const char* format, ...) {
	char line[kLineBufferSize];

	va_list args;
	va_start(args, format);
	int length = _vsnprintf(line, kLineBufferSize - 2, format, args);
	va_end(args);

	if (length < 0) {
		length = kLineBufferSize - 2;
	}

	line[length] = '\r';
	line[length + 1] = '\n';
	WriteRaw(line, static_cast<UInt32>(length) + 2);
}

}  // namespace obvr::log
