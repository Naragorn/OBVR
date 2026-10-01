// Checks the cutting of a game sound into parts: the wave file's chunks, the
// stretch of seconds as whole frames, the fades at the cut, DirectSound's
// volume for a gain, and a file found in a TES4 archive's index.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "game/SoundSlice.h"

namespace {

using namespace obvr;
using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void Put32(std::vector<UInt8>& b, UInt32 v) {
	for (int i = 0; i < 4; ++i) {
		b.push_back(static_cast<UInt8>(v >> (8 * i)));
	}
}
void Put16(std::vector<UInt8>& b, UInt32 v) {
	b.push_back(static_cast<UInt8>(v));
	b.push_back(static_cast<UInt8>(v >> 8));
}
void PutTag(std::vector<UInt8>& b, const char* t) { b.insert(b.end(), t, t + 4); }

// A mono 16-bit wave at 1000 frames a second, `frames` long, with an odd
// "LIST" chunk before its data (padded, as RIFF pads).
std::vector<UInt8> Wave(UInt32 frames, UInt16 format = 1, UInt16 bits = 16) {
	std::vector<UInt8> b;
	PutTag(b, "RIFF");
	Put32(b, 0);
	PutTag(b, "WAVE");
	PutTag(b, "fmt ");
	Put32(b, 16);
	Put16(b, format);
	Put16(b, 1);
	Put32(b, 1000);
	Put32(b, 1000 * bits / 8);
	Put16(b, bits / 8);
	Put16(b, bits);
	PutTag(b, "LIST");
	Put32(b, 3);
	b.push_back(1);
	b.push_back(2);
	b.push_back(3);
	b.push_back(0);  // the pad byte
	PutTag(b, "data");
	Put32(b, frames * bits / 8);
	for (UInt32 i = 0; i < frames * bits / 8; ++i) {
		b.push_back(static_cast<UInt8>(i));
	}
	return b;
}

void TestWave() {
	std::printf("The wave file\n");
	std::vector<UInt8> w = Wave(500);
	WavPcm pcm;
	Check(ParseWav(w.data(), static_cast<UInt32>(w.size()), pcm) && pcm.channels == 1 && pcm.bitsPerSample == 16 &&
	          pcm.samplesPerSecond == 1000 && pcm.blockAlign == 2 && pcm.dataBytes == 1000,
	      "its format read, past a padded chunk of another kind");
	Check(pcm.dataOffset == 12 + 24 + 12 + 8, "the samples where they start");
	std::vector<UInt8> adpcm = Wave(500, 2);
	Check(!ParseWav(adpcm.data(), static_cast<UInt32>(adpcm.size()), pcm), "a compressed wave: refused");
	std::vector<UInt8> odd = Wave(500, 1, 24);
	Check(!ParseWav(odd.data(), static_cast<UInt32>(odd.size()), pcm), "24 bits: refused");
	std::vector<UInt8> cut(w.begin(), w.end() - 10);
	Check(!ParseWav(cut.data(), static_cast<UInt32>(cut.size()), pcm), "the data running past the file: refused");
	std::vector<UInt8> noFormat(w);
	std::memcpy(noFormat.data() + 12, "junk", 4);
	Check(!ParseWav(noFormat.data(), static_cast<UInt32>(noFormat.size()), pcm), "data before any format: refused");
	Check(!ParseWav(w.data(), 8, pcm) && !ParseWav(nullptr, 100, pcm), "too short, or nothing: refused");
	std::vector<UInt8> notWave(w);
	std::memcpy(notWave.data() + 8, "AVI ", 4);
	Check(!ParseWav(notWave.data(), static_cast<UInt32>(notWave.size()), pcm), "another RIFF kind: refused");
}

void TestSlice() {
	std::printf("A stretch of seconds as frames\n");
	std::vector<UInt8> w = Wave(1000);
	WavPcm pcm;
	ParseWav(w.data(), static_cast<UInt32>(w.size()), pcm);
	UInt32 offset = 0;
	UInt32 bytes = 0;
	Check(SliceBytes(pcm, 0.3f, 0.98f, offset, bytes) && offset == pcm.dataOffset + 600 && bytes == 680 * 2,
	      "0.30 to 0.98 s: frames 300 to 980");
	Check(SliceBytes(pcm, -1.0f, 5.0f, offset, bytes) && offset == pcm.dataOffset && bytes == pcm.dataBytes,
	      "past both ends: the whole of it");
	Check(!SliceBytes(pcm, 2.0f, 3.0f, offset, bytes), "wholly after the end: nothing");
	Check(!SliceBytes(pcm, 0.5f, 0.5f, offset, bytes) && !SliceBytes(pcm, 0.6f, 0.5f, offset, bytes),
	      "no length, or backwards: nothing");
	Check(!SliceBytes(WavPcm{}, 0.0f, 1.0f, offset, bytes), "no format: nothing");
}

void TestFade() {
	std::printf("The fades at the cut\n");
	std::vector<SInt16> s(10, 1000);
	FadeEdges(s.data(), 10, 1, 4);
	Check(s[0] == 0 && s[1] == 250 && s[3] == 750 && s[4] == 1000 && s[5] == 1000, "faded in over the first four");
	Check(s[9] == 0 && s[8] == 250 && s[6] == 750, "and out over the last four");
	std::vector<SInt16> st(8, 1000);
	FadeEdges(st.data(), 4, 2, 10);
	Check(st[0] == 0 && st[1] == 0 && st[2] == 500 && st[3] == 500 && st[6] == 0 && st[7] == 0,
	      "a fade longer than half: half each way, both channels");
	FadeEdges(nullptr, 4, 1, 2);
	Check(true, "nothing given: nothing done");
}

void TestVolume() {
	std::printf("DirectSound's volume\n");
	Check(DirectSoundVolume(1.0f) == 0 && DirectSoundVolume(2.0f) == 0, "full or more: 0");
	Check(DirectSoundVolume(0.0f) == -10000 && DirectSoundVolume(-1.0f) == -10000, "nothing: the minimum");
	Check(std::abs(DirectSoundVolume(0.5f) - (-602)) <= 1, "half: -6.02 dB");
	Check(std::abs(DirectSoundVolume(0.8f) - (-194)) <= 1, "0.8: -1.94 dB");
	Check(std::abs(DirectSoundVolume(0.01f) - (-4000)) <= 1, "a hundredth: -40 dB");
	Check(DirectSoundVolume(0.0002f) >= -10000, "near nothing: not below the minimum");
}

// A version 103 archive's index with two folders of two files each.
std::vector<UInt8> Archive(UInt32 flags, UInt32 sizeOfBowDraw) {
	struct File {
		const char* name;
		UInt32 size;
		UInt32 offset;
	};
	struct Folder {
		const char* name;
		File files[2];
	};
	const Folder folders[2] = {
		{"sound\\fx\\equip", {{"tes4-combat-bow_ready.wav", 10, 1000}, {"tes4-combat-bow_sheath.wav", 20, 2000}}},
		{"sound\\fx\\wpn", {{"wpn_bowdraw.wav", sizeOfBowDraw, 3000}, {"wpn_bowshoot.wav", 40, 4000}}},
	};
	UInt32 folderNames = 0;
	UInt32 fileNames = 0;
	for (const Folder& f : folders) {
		folderNames += static_cast<UInt32>(std::strlen(f.name)) + 1;
		for (const File& file : f.files) {
			fileNames += static_cast<UInt32>(std::strlen(file.name)) + 1;
		}
	}
	std::vector<UInt8> b;
	PutTag(b, "BSA\0");
	Put32(b, 103);
	Put32(b, 36);
	Put32(b, flags);
	Put32(b, 2);
	Put32(b, 4);
	Put32(b, folderNames);
	Put32(b, fileNames);
	Put32(b, 0);
	for (int i = 0; i < 2; ++i) {
		Put32(b, 0);
		Put32(b, 0);  // the hash, not read
		Put32(b, 2);
		Put32(b, 0);
	}
	for (const Folder& f : folders) {
		const UInt32 length = static_cast<UInt32>(std::strlen(f.name)) + 1;
		b.push_back(static_cast<UInt8>(length));
		b.insert(b.end(), f.name, f.name + length);
		for (const File& file : f.files) {
			Put32(b, 0);
			Put32(b, 0);
			Put32(b, file.size);
			Put32(b, file.offset);
		}
	}
	for (const Folder& f : folders) {
		for (const File& file : f.files) {
			b.insert(b.end(), file.name, file.name + std::strlen(file.name) + 1);
		}
	}
	return b;
}

void TestArchive() {
	std::printf("A file in the game's archive\n");
	std::vector<UInt8> a = Archive(0x713, 139528);
	const UInt32 size = static_cast<UInt32>(a.size());
	Check(BsaIndexBytes(a.data(), size) == size, "the index's length from its header");
	BsaEntry e;
	Check(BsaFind(a.data(), size, "sound\\fx\\wpn", "wpn_bowdraw.wav", e) && e.offset == 3000 && e.size == 139528,
	      "found in the second folder");
	Check(BsaFind(a.data(), size, "Sound\\FX\\Wpn", "WPN_BowShoot.wav", e) && e.offset == 4000 && e.size == 40,
	      "in either case");
	Check(BsaFind(a.data(), size, "sound\\fx\\equip", "tes4-combat-bow_sheath.wav", e) && e.offset == 2000,
	      "and in the first");
	Check(!BsaFind(a.data(), size, "sound\\fx\\wpn", "wpn_bowdraw", e), "a name's start alone: not it");
	Check(!BsaFind(a.data(), size, "sound\\fx\\wp", "wpn_bowdraw.wav", e), "a folder's start alone: not it");
	Check(!BsaFind(a.data(), size, "sound\\fx\\equip", "wpn_bowdraw.wav", e), "in another folder: not it");
	std::vector<UInt8> toggled = Archive(0x713, 139528 | kBsaSizeToggle);
	Check(!BsaFind(toggled.data(), size, "sound\\fx\\wpn", "wpn_bowdraw.wav", e), "stored compressed: refused");
	std::vector<UInt8> compressed = Archive(0x717, 139528 | kBsaSizeToggle);
	Check(BsaFind(compressed.data(), size, "sound\\fx\\wpn", "wpn_bowdraw.wav", e) && e.size == 139528,
	      "a compressed archive's file stored plain: found");
	std::vector<UInt8> nameless = Archive(0x711, 1);
	Check(BsaIndexBytes(nameless.data(), size) == 0, "no file names kept: no index");
	std::vector<UInt8> other(a);
	other[4] = 104;
	Check(BsaIndexBytes(other.data(), size) == 0 && !BsaFind(other.data(), size, "sound\\fx\\wpn", "wpn_bowdraw.wav", e),
	      "another version: refused");
	Check(!BsaFind(a.data(), size - 5, "sound\\fx\\wpn", "wpn_bowdraw.wav", e), "an index cut short: refused");
}

}  // namespace

int main() {
	TestWave();
	TestSlice();
	TestFade();
	TestVolume();
	TestArchive();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
