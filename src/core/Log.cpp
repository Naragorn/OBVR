#include "core/Log.h"

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

UInt32 Length(const char* text) {
	UInt32 length = 0;
	while (text[length] != '\0') {
		++length;
	}
	return length;
}

}  // namespace

void Open(const char* fileName) {
	// The path is built relative to the process so that the log file ends up
	// next to Oblivion.exe rather than in whatever the working directory
	// happens to be.
	char path[512];
	const DWORD moduleLength = GetModuleFileNameA(nullptr, path, sizeof(path));
	if (moduleLength == 0 || moduleLength >= sizeof(path)) {
		return;
	}

	UInt32 cut = moduleLength;
	while (cut > 0 && path[cut - 1] != '\\' && path[cut - 1] != '/') {
		--cut;
	}

	const UInt32 nameLength = Length(fileName);
	if (cut + nameLength + 1 >= sizeof(path)) {
		return;
	}
	for (UInt32 i = 0; i <= nameLength; ++i) {
		path[cut + i] = fileName[i];
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
