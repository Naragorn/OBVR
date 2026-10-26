// Checks the torch kept in the other hand with a two-hander drawn
// (game/TorchKeepLogic.h): the stub's bytes for both sites in 0x005E7230,
// its short jumps landing on the hide and the keep, and its long jumps
// reaching the engine's own targets from wherever the stub lies.

#include <cstdio>
#include <cstring>

#include "game/TorchKeepLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

UInt32 Le32(const UInt8* p) {
	return static_cast<UInt32>(p[0]) | (static_cast<UInt32>(p[1]) << 8) | (static_cast<UInt32>(p[2]) << 16) |
	       (static_cast<UInt32>(p[3]) << 24);
}

// Where a jump at `at` (opcode bytes `opcodeLength` before the rel32) lands.
UInt32 Rel32Target(UInt32 stub, const UInt8* code, UInt32 at, UInt32 opcodeLength) {
	return stub + at + opcodeLength + 4 + Le32(code + at + opcodeLength);
}

constexpr UInt32 kStub = 0x12340000;
constexpr UInt32 kFlag = 0x0BADF00D;

// The body's offsets after a prefix of `prefix` bytes, as the header lays
// them out: the hide's jump at 47, the keep's at 52.
void CheckBody(const UInt8* code, UInt32 prefix, UInt32 skip, UInt32 hide, const char* site) {
	char what[160];
	const UInt8* b = code + prefix;
	std::snprintf(what, sizeof(what), "%s: cmp byte [flag], 0 with the flag's address", site);
	Check(b[0] == 0x80 && b[1] == 0x3D && Le32(b + 2) == kFlag && b[6] == 0x00, what);
	std::snprintf(what, sizeof(what), "%s: the player's pointer read from 0x00B333C4 and compared with esi", site);
	Check(b[9] == 0xA1 && Le32(b + 10) == kPlayerPointerAddress && b[14] == 0x3B && b[15] == 0xC6, what);
	std::snprintf(what, sizeof(what), "%s: the weapon entry from [esp+20h], its form at +8, the type at +90h", site);
	Check(b[18] == 0x8B && b[19] == 0x44 && b[20] == 0x24 && b[21] == 0x20 && b[26] == 0x8B && b[27] == 0x40 &&
	          b[28] == 0x08 && b[33] == 0x8A && b[34] == 0x80 && Le32(b + 35) == 0x90,
	      what);
	std::snprintf(what, sizeof(what), "%s: types 1 and 3 (the two-handed blade and blunt) keep", site);
	Check(b[39] == 0x3C && b[40] == 0x01 && b[43] == 0x3C && b[44] == 0x03, what);
	// Every short jump of the check lands on the hide (47) or the keep (52).
	const UInt32 jumps[6] = {7, 16, 24, 31, 41, 45};
	const UInt32 lands[6] = {47, 47, 47, 47, 52, 52};
	bool shortOk = true;
	for (int i = 0; i < 6; ++i) {
		const UInt32 at = jumps[i];
		const bool isJcc = b[at] == 0x74 || b[at] == 0x75;
		shortOk = shortOk && isJcc && at + 2 + b[at + 1] == lands[i];
	}
	std::snprintf(what, sizeof(what), "%s: each short jump on the hide or the keep", site);
	Check(shortOk, what);
	std::snprintf(what, sizeof(what), "%s: the hide jumps to the engine's hide", site);
	Check(b[47] == 0xE9 && Rel32Target(kStub, code, prefix + 47, 1) == hide, what);
	std::snprintf(what, sizeof(what), "%s: the keep jumps where the engine skips the hide", site);
	Check(b[52] == 0xE9 && Rel32Target(kStub, code, prefix + 52, 1) == skip, what);
}

void TestAfterTest() {
	std::printf("The stub at 0x005E73E6 (a weapon or a torch equipped)\n");
	UInt8 code[kTorchKeepStubMax] = {};
	const UInt32 n = BuildTorchKeepStub(code, sizeof(code), kStub, TorchKeepSite::Equip, kFlag);
	Check(n == 6 + kTorchKeepBodySize, "6 bytes of prefix and the body");
	Check(code[0] == 0x0F && code[1] == 0x84 && Rel32Target(kStub, code, 0, 2) == kTorchEquipSkip,
	      "first the engine's own je: not worn, or not drawn, skips the hide as before");
	CheckBody(code, 6, kTorchEquipSkip, kTorchEquipHide, "equip");
}

void TestRefresh() {
	std::printf("The stub at 0x005E76C6 (the refresh, the draw)\n");
	UInt8 code[kTorchKeepStubMax] = {};
	const UInt32 n = BuildTorchKeepStub(code, sizeof(code), kStub, TorchKeepSite::Refresh, kFlag);
	Check(n == 11 + kTorchKeepBodySize, "11 bytes of prefix and the body");
	Check(code[0] == 0x80 && code[1] == 0x7C && code[2] == 0x24 && code[3] == 0x10 && code[4] == 0x00,
	      "the engine's compare of the weapon out, as it was");
	Check(code[5] == 0x0F && code[6] == 0x84 && Rel32Target(kStub, code, 5, 2) == kTorchRefreshShow,
	      "sheathed: on to the engine's own show of the torch");
	CheckBody(code, 11, kTorchRefreshShow, kTorchRefreshHide, "refresh");
}

void TestPatches() {
	std::printf("The jumps written at the sites\n");
	UInt8 site[8] = {};
	Check(BuildTorchKeepSitePatch(site, sizeof(site), TorchKeepSite::Equip, kStub) == 6,
	      "the equip site: 6 bytes");
	Check(site[0] == 0xE9 && kTorchEquipSite + 5 + Le32(site + 1) == kStub && site[5] == 0x90,
	      "a jump to the stub and a nop over the je's last byte");
	Check(BuildTorchKeepSitePatch(site, sizeof(site), TorchKeepSite::Refresh, kStub) == 7,
	      "the refresh site: 7 bytes");
	Check(site[0] == 0xE9 && kTorchRefreshSite + 5 + Le32(site + 1) == kStub && site[5] == 0x90 && site[6] == 0x90,
	      "a jump to the stub and two nops over the cmp and je");
	Check(std::memcmp(kTorchEquipSiteBytes, "\x0F\x84\x16\x06\x00\x00", 6) == 0,
	      "the equip site's bytes as read: je 005E7A02");
	Check(std::memcmp(kTorchRefreshSiteBytes, "\x80\x7C\x24\x10\x00\x74\x24", 7) == 0,
	      "the refresh site's bytes as read: cmp byte [esp+10h], 0; je 005E76F1");
}

void TestRefusals() {
	std::printf("No room, no bytes\n");
	UInt8 small[10] = {};
	Check(BuildTorchKeepStub(small, sizeof(small), kStub, TorchKeepSite::Equip, kFlag) == 0,
	      "a stub buffer too small: nothing");
	Check(BuildTorchKeepStub(nullptr, 64, kStub, TorchKeepSite::Equip, kFlag) == 0, "no buffer: nothing");
	UInt8 tiny[4] = {};
	Check(BuildTorchKeepSitePatch(tiny, sizeof(tiny), TorchKeepSite::Refresh, kStub) == 0,
	      "a site buffer too small: nothing");
}

void TestKeeps() {
	std::printf("Which weapons keep the torch\n");
	Check(!TorchKeptWith(0), "a one-handed blade: vanilla keeps it anyway, the stub does not decide");
	Check(TorchKeptWith(1), "a two-handed blade: kept");
	Check(!TorchKeptWith(2), "a one-handed blunt: not the stub's");
	Check(TorchKeptWith(3), "a two-handed blunt: kept");
	Check(!TorchKeptWith(4), "a staff: put away as vanilla does");
	Check(!TorchKeptWith(5), "a bow: put away, the bow is in that hand");
}

}  // namespace

int main() {
	TestAfterTest();
	TestRefresh();
	TestPatches();
	TestRefusals();
	TestKeeps();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
