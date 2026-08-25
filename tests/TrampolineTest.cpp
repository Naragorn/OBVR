// Prueft die erzeugte Bytefolge des Kamera-Hooks.
//
// Der Test laeuft nativ auf dem Entwicklungsrechner, nicht in Oblivion. Das
// geht, weil BuildTrampoline und BuildPatch reine Byteerzeugung sind: sie
// haengen nur von den uebergebenen Adressen ab, nicht von der Architektur des
// Hostsystems.
//
// Geprueft wird gegen von Hand nachgerechnete Sollwerte. Ein Vergleich gegen
// eine zweite Implementierung derselben Rechnung waere wertlos - der Punkt ist
// gerade, dass die Sollwerte unabhaengig ermittelt sind.

#include <cstdio>

#include "camera/CameraTrampoline.h"
#include "game/GameAddresses.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	if (condition) {
		std::printf("  ok    %s\n", what);
	} else {
		std::printf("  FEHLT %s\n", what);
		++g_failures;
	}
}

void CheckBytes(const UInt8* actual, const UInt8* expected, UInt32 size, const char* what) {
	for (UInt32 i = 0; i < size; ++i) {
		if (actual[i] != expected[i]) {
			std::printf("  FEHLT %s: Byte %u ist %02X, erwartet %02X\n",
			            what, i, actual[i], expected[i]);
			++g_failures;
			return;
		}
	}
	std::printf("  ok    %s\n", what);
}

void DumpHex(const UInt8* data, UInt32 size) {
	std::printf("       ");
	for (UInt32 i = 0; i < size; ++i) {
		std::printf("%02X ", data[i]);
		if ((i % 16) == 15) {
			std::printf("\n       ");
		}
	}
	std::printf("\n");
}

// Frei gewaehlte Adressen. Sie muessen weit genug von den Zieladressen weg
// liegen, damit die Relativabstaende nicht zufaellig stimmen.
constexpr UInt32 kTrampolineAddress = 0x20000000;
constexpr UInt32 kCallbackAddress = 0x30000000;

void TestTrampoline() {
	std::printf("Trampolin\n");

	UInt8 buffer[64] = {};
	const UInt32 size = obvr::camera::BuildTrampoline(
		buffer, sizeof(buffer), kTrampolineAddress, kCallbackAddress);

	DumpHex(buffer, size);

	// pushad(1) + pushfd(1) + push[esp+0x20](4) + call(5) + add esp(3)
	// + popfd(1) + popad(1) + cmp(8) + ja(6) + xor(2) + jmp(5) = 37
	Check(size == 37, "Laenge betraegt 37 Bytes");

	Check(buffer[0] == 0x60, "beginnt mit pushad");
	Check(buffer[1] == 0x9C, "danach pushfd");

	// push dword ptr [esp+0x20] - holt das von pushad gesicherte EAX, das
	// nach dem zusaetzlichen pushfd um vier Bytes tiefer liegt.
	const UInt8 expectedPush[4] = {0xFF, 0x74, 0x24, 0x20};
	CheckBytes(buffer + 2, expectedPush, 4, "push dword ptr [esp+0x20]");

	// call rel32 auf den Callback. Ziel = Adresse nach der Instruktion + rel.
	Check(buffer[6] == 0xE8, "call rel32 folgt");
	const UInt32 callRel = static_cast<UInt32>(buffer[7]) |
	                       (static_cast<UInt32>(buffer[8]) << 8) |
	                       (static_cast<UInt32>(buffer[9]) << 16) |
	                       (static_cast<UInt32>(buffer[10]) << 24);
	Check(kTrampolineAddress + 11 + callRel == kCallbackAddress,
	      "call zeigt auf OBVR_OnCameraUpdated");

	const UInt8 expectedCleanup[6] = {0x83, 0xC4, 0x04, 0x9D, 0x61, 0x66};
	CheckBytes(buffer + 11, expectedCleanup, 6, "add esp,4 / popfd / popad");

	// Die ueberschriebene Originalinstruktion muss wortgleich wieder
	// auftauchen, sonst geht dem Spiel ein Vergleich verloren.
	CheckBytes(buffer + 16, obvr::camera::kOriginalBytes, 8,
	           "Originalinstruktion cmp word ptr [ebx+0xB6],0");

	// ja rel32 auf den Zweig, den das Original bei nicht leerer Liste nimmt.
	Check(buffer[24] == 0x0F && buffer[25] == 0x87, "ja rel32 folgt");
	const UInt32 jaRel = static_cast<UInt32>(buffer[26]) |
	                     (static_cast<UInt32>(buffer[27]) << 8) |
	                     (static_cast<UInt32>(buffer[28]) << 16) |
	                     (static_cast<UInt32>(buffer[29]) << 24);
	Check(kTrampolineAddress + 30 + jaRel == obvr::addr::kHookCameraUpdateResumeTaken,
	      "ja zeigt auf 0x0066BE7C");

	Check(buffer[30] == 0x33 && buffer[31] == 0xC9, "xor ecx,ecx");

	// jmp rel32 auf den Zweig fuer die leere Liste.
	Check(buffer[32] == 0xE9, "jmp rel32 folgt");
	const UInt32 jmpRel = static_cast<UInt32>(buffer[33]) |
	                      (static_cast<UInt32>(buffer[34]) << 8) |
	                      (static_cast<UInt32>(buffer[35]) << 16) |
	                      (static_cast<UInt32>(buffer[36]) << 24);
	Check(kTrampolineAddress + 37 + jmpRel == obvr::addr::kHookCameraUpdateResumeEmpty,
	      "jmp zeigt auf 0x0066BE84");
}

void TestPatch() {
	std::printf("Patch\n");

	UInt8 buffer[8] = {};
	const UInt32 size = obvr::camera::BuildPatch(
		buffer, sizeof(buffer), obvr::addr::kHookCameraUpdate, kTrampolineAddress);

	DumpHex(buffer, size);

	// Muss die Originalinstruktion vollstaendig ueberdecken; bliebe ein Rest
	// stehen, wuerde das Spiel Bruchstuecke ausfuehren.
	Check(size == obvr::addr::kHookCameraUpdatePatchSize,
	      "Laenge deckt die 8 Byte lange Originalinstruktion ab");

	Check(buffer[0] == 0xE9, "beginnt mit jmp rel32");
	const UInt32 rel = static_cast<UInt32>(buffer[1]) |
	                   (static_cast<UInt32>(buffer[2]) << 8) |
	                   (static_cast<UInt32>(buffer[3]) << 16) |
	                   (static_cast<UInt32>(buffer[4]) << 24);
	Check(obvr::addr::kHookCameraUpdate + 5 + rel == kTrampolineAddress,
	      "jmp zeigt auf das Trampolin");

	Check(buffer[5] == 0x90 && buffer[6] == 0x90 && buffer[7] == 0x90,
	      "Rest mit nop aufgefuellt");
}

void TestOverflowIsReported() {
	std::printf("Kapazitaet\n");

	UInt8 tooSmall[16] = {};
	const UInt32 size = obvr::camera::BuildTrampoline(
		tooSmall, sizeof(tooSmall), kTrampolineAddress, kCallbackAddress);

	// Ein stillschweigend abgeschnittenes Trampolin waere der schlimmste Fall:
	// der Hook wuerde gesetzt und mitten im Nichts enden.
	Check(size == 0, "zu kleiner Puffer meldet 0 statt abzuschneiden");
}

}  // namespace

int main() {
	std::printf("OBVR Trampolin-Test\n\n");

	TestTrampoline();
	std::printf("\n");
	TestPatch();
	std::printf("\n");
	TestOverflowIsReported();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("Alle Pruefungen bestanden.\n");
		return 0;
	}

	std::printf("%d Pruefung(en) fehlgeschlagen.\n", g_failures);
	return 1;
}
