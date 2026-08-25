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

// Liest ein rel32-Feld an der angegebenen Stelle.
UInt32 ReadRel32(const UInt8* at) {
	return static_cast<UInt32>(at[0]) | (static_cast<UInt32>(at[1]) << 8) |
	       (static_cast<UInt32>(at[2]) << 16) | (static_cast<UInt32>(at[3]) << 24);
}

// Frei gewaehlte Adressen. Sie muessen weit genug von den Zieladressen weg
// liegen, damit die Relativabstaende nicht zufaellig stimmen.
constexpr UInt32 kTrampolineAddress = 0x20000000;
constexpr UInt32 kCallbackAddress = 0x30000000;

// Adressen, die es nur in einem LargeAddressAware-Prozess geben kann. Ohne den
// 4GB-Patch gibt VirtualAlloc einem 32-Bit-Prozess nur Adressen unterhalb
// 0x80000000; mit dem Patch kann das Trampolin auch darueber landen.
constexpr UInt32 kHighTrampolineAddress = 0xC0000000;
constexpr UInt32 kHighCallbackAddress = 0xD0000000;

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
	Check(kTrampolineAddress + 11 + ReadRel32(buffer + 7) == kCallbackAddress,
	      "call zeigt auf OBVR_OnCameraUpdated");

	const UInt8 expectedCleanup[6] = {0x83, 0xC4, 0x04, 0x9D, 0x61, 0x66};
	CheckBytes(buffer + 11, expectedCleanup, 6, "add esp,4 / popfd / popad");

	// Die ueberschriebene Originalinstruktion muss wortgleich wieder
	// auftauchen, sonst geht dem Spiel ein Vergleich verloren.
	CheckBytes(buffer + 16, obvr::camera::kOriginalBytes, 8,
	           "Originalinstruktion cmp word ptr [ebx+0xB6],0");

	// ja rel32 auf den Zweig, den das Original bei nicht leerer Liste nimmt.
	Check(buffer[24] == 0x0F && buffer[25] == 0x87, "ja rel32 folgt");
	Check(kTrampolineAddress + 30 + ReadRel32(buffer + 26) ==
	          obvr::addr::kHookCameraUpdateResumeTaken,
	      "ja zeigt auf 0x0066BE7C");

	Check(buffer[30] == 0x33 && buffer[31] == 0xC9, "xor ecx,ecx");

	// jmp rel32 auf den Zweig fuer die leere Liste.
	Check(buffer[32] == 0xE9, "jmp rel32 folgt");
	Check(kTrampolineAddress + 37 + ReadRel32(buffer + 33) ==
	          obvr::addr::kHookCameraUpdateResumeEmpty,
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
	Check(obvr::addr::kHookCameraUpdate + 5 + ReadRel32(buffer + 1) == kTrampolineAddress,
	      "jmp zeigt auf das Trampolin");

	Check(buffer[5] == 0x90 && buffer[6] == 0x90 && buffer[7] == 0x90,
	      "Rest mit nop aufgefuellt");
}

// Der 4GB-Patch (LargeAddressAware) aendert nur zwei Bytes im PE-Header der
// Oblivion.exe, nicht den Code - die geprueften acht Bytes an 0x0066BE6E sind
// in beiden Fassungen identisch, der Hook selbst ist also unberuehrt.
//
// Was sich aendert, ist die Lage des Trampolins: VirtualAlloc kann es nun
// oberhalb 2 GB ablegen, und der Sprung vom Hook dorthin ueberspannt dann mehr
// als 2 GB.
//
// Auf x86-64 waere das unmoeglich: rel32 ist dort eine vorzeichenbehaftete
// Verschiebung von +-2 GB innerhalb eines 64-Bit-Adressraums. Auf x86-32 ist
// der Adressraum aber exakt 2^32 gross und die CPU rechnet
// EIP = EIP_next + rel32 modulo 2^32 - jedes Ziel ist von jeder Quelle aus
// erreichbar, der Abstand laeuft schlicht ueber.
//
// CodeWriter rechnet die Abstaende durchgehend in UInt32. Der Ueberlauf ist
// damit wohldefiniert und deckt sich exakt mit dem Verhalten der CPU. Wuerde
// jemand das spaeter auf int32_t umstellen oder eine Reichweitenpruefung
// einziehen, faellt es hier auf statt erst im Spiel - und dort nur auf
// Rechnern mit 4GB-Patch, was die Suche unangenehm machen wuerde.
void TestLargeAddressAware() {
	std::printf("4GB-Patch: Trampolin oberhalb 2 GB\n");

	UInt8 buffer[64] = {};
	const UInt32 size = obvr::camera::BuildTrampoline(
		buffer, sizeof(buffer), kHighTrampolineAddress, kHighCallbackAddress);

	Check(size == 37, "Laenge unveraendert 37 Bytes");

	Check(static_cast<UInt32>(kHighTrampolineAddress + 11 + ReadRel32(buffer + 7)) ==
	          kHighCallbackAddress,
	      "call erreicht den Callback oberhalb 2 GB");

	// Die beiden Ruecksprunge fuehren von 0xC0000000 hinunter nach 0x0066BExx,
	// eine Distanz von rund -3,2 GB. Genau hier zeigt sich der Ueberlauf.
	Check(static_cast<UInt32>(kHighTrampolineAddress + 30 + ReadRel32(buffer + 26)) ==
	          obvr::addr::kHookCameraUpdateResumeTaken,
	      "ja springt von oben zurueck nach 0x0066BE7C");

	Check(static_cast<UInt32>(kHighTrampolineAddress + 37 + ReadRel32(buffer + 33)) ==
	          obvr::addr::kHookCameraUpdateResumeEmpty,
	      "jmp springt von oben zurueck nach 0x0066BE84");

	// Der Patch an der Hookstelle muss ein hoch gelegenes Trampolin genauso
	// erreichen wie ein niedriges.
	UInt8 patch[8] = {};
	const UInt32 patchSize = obvr::camera::BuildPatch(
		patch, sizeof(patch), obvr::addr::kHookCameraUpdate, kHighTrampolineAddress);

	Check(patchSize == obvr::addr::kHookCameraUpdatePatchSize, "Patch weiterhin 8 Bytes");
	Check(patch[0] == 0xE9, "Patch beginnt mit jmp rel32");
	Check(static_cast<UInt32>(obvr::addr::kHookCameraUpdate + 5 + ReadRel32(patch + 1)) ==
	          kHighTrampolineAddress,
	      "jmp erreicht das Trampolin oberhalb 2 GB");

	// Gegenprobe: die Adresslage darf ausschliesslich die Relativfelder
	// beeinflussen. Waeren auch Opcodes betroffen, haette die Byteerzeugung
	// abhaengig von der Adresse andere Instruktionen gewaehlt - ein Fehler,
	// den die Einzelpruefungen oben nicht sehen wuerden.
	UInt8 low[64] = {};
	obvr::camera::BuildTrampoline(low, sizeof(low), kTrampolineAddress, kCallbackAddress);

	bool opcodesEqual = true;
	for (UInt32 i = 0; i < 37; ++i) {
		const bool isRel32Field =
			(i >= 7 && i <= 10) || (i >= 26 && i <= 29) || (i >= 33 && i <= 36);
		if (!isRel32Field && low[i] != buffer[i]) {
			std::printf("  FEHLT Byte %u weicht ab: %02X gegen %02X\n", i, low[i], buffer[i]);
			opcodesEqual = false;
		}
	}
	Check(opcodesEqual, "identische Opcodes, nur die rel32-Felder unterscheiden sich");
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
	TestLargeAddressAware();
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
