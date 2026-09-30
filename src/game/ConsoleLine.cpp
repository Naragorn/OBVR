#include "game/ConsoleLine.h"

#include <intrin.h>

#include <cstring>

#include "core/Log.h"
#include "obse/NativeMenuApi.h"

namespace obvr::game {
namespace {

obse::ConsoleApi* g_console = nullptr;
bool g_ready = false;

constexpr unsigned kSlots = 4;
constexpr unsigned kLineChars = 96;
char g_lines[kSlots][kLineChars];
unsigned g_count = 0;
volatile long g_lock = 0;

void Lock() {
	while (_InterlockedExchange(&g_lock, 1) != 0) {
		_mm_pause();
	}
}

void Unlock() { _InterlockedExchange(&g_lock, 0); }

// The xOBSE task: every frame, on the game's loop.
void Tick() {
	char lines[kSlots][kLineChars];
	unsigned count = 0;
	Lock();
	count = g_count;
	for (unsigned i = 0; i < count; ++i) {
		std::memcpy(lines[i], g_lines[i], kLineChars);
	}
	g_count = 0;
	Unlock();
	for (unsigned i = 0; i < count; ++i) {
		// Its answer is not whether the line took: "player.playgroup unequip 1"
		// answers false and plays the group all the same (harness 2026-09-30,
		// the player's action going to 12, ScriptAnimation). What the line did
		// is to be read from the game.
		const bool answer = g_console->RunScriptLine2(lines[i], nullptr, true);
		static unsigned s_logLeft = 24;
		if (s_logLeft > 0) {
			--s_logLeft;
			OBVR_LOG("Console line: \"%s\" run (the interface answered %d)", lines[i], answer ? 1 : 0);
		}
	}
}

}  // namespace

void InstallConsoleLine(const obse::Interface* api) {
	if (api == nullptr || api->obseVersion < 22 || api->QueryInterface == nullptr) {
		OBVR_LOG("Console line: no xOBSE 22 interfaces - script lines cannot be run");
		return;
	}
	g_console = static_cast<obse::ConsoleApi*>(api->QueryInterface(0));
	const auto tasks = static_cast<obse::TasksApi*>(api->QueryInterface(8));
	if (g_console == nullptr || g_console->version < 2 || g_console->RunScriptLine2 == nullptr || tasks == nullptr ||
	    tasks->EnqueueTask == nullptr || tasks->EnqueueTask(&Tick) == nullptr) {
		OBVR_LOG("Console line: the console or task interface is missing - script lines cannot be run");
		g_console = nullptr;
		return;
	}
	g_ready = true;
}

bool RequestConsoleLine(const char* line) {
	if (!g_ready || line == nullptr) {
		static bool s_logged = false;
		if (!s_logged) {
			s_logged = true;
			OBVR_LOG("Console line: refused \"%s\" - not installed", line != nullptr ? line : "");
		}
		return false;
	}
	const size_t length = std::strlen(line);
	if (length >= kLineChars) {
		return false;
	}
	bool queued = false;
	Lock();
	if (g_count < kSlots) {
		std::memcpy(g_lines[g_count], line, length + 1);
		++g_count;
		queued = true;
	}
	Unlock();
	return queued;
}

}  // namespace obvr::game
