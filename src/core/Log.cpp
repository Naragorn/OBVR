#include "core/Log.h"

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
