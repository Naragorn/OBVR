#pragma once

#include "core/Types.h"

namespace obvr::game {

// A piece of one of the game's sound files, found and cut without the game:
// the bow's draw sound is one file that the engine plays whole at the draw's
// start, and the bow drawn by hand wants its parts one at a time
// (game/BowDrawSound.h). Pure, covered by sound_slice_test.

// A PCM wave file's format and where its samples are, from its RIFF chunks.
struct WavPcm {
	UInt16 channels = 0;
	UInt16 bitsPerSample = 0;
	UInt32 samplesPerSecond = 0;
	UInt32 blockAlign = 0;
	UInt32 dataOffset = 0;  // from the file's first byte
	UInt32 dataBytes = 0;
};

namespace sound_detail {

inline UInt32 Le32(const UInt8* p) {
	return static_cast<UInt32>(p[0]) | (static_cast<UInt32>(p[1]) << 8) | (static_cast<UInt32>(p[2]) << 16) |
	       (static_cast<UInt32>(p[3]) << 24);
}
inline UInt16 Le16(const UInt8* p) { return static_cast<UInt16>(p[0] | (p[1] << 8)); }
inline bool Tag(const UInt8* p, const char* tag) {
	return p[0] == static_cast<UInt8>(tag[0]) && p[1] == static_cast<UInt8>(tag[1]) &&
	       p[2] == static_cast<UInt8>(tag[2]) && p[3] == static_cast<UInt8>(tag[3]);
}
inline char Lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

}  // namespace sound_detail

// "RIFF" .. "WAVE", its "fmt " chunk plain PCM (format 1) of 8 or 16 bits,
// and its "data" chunk within the file. False for anything else - a
// compressed wave is not cut.
inline bool ParseWav(const UInt8* bytes, UInt32 size, WavPcm& out) {
	using namespace sound_detail;
	if (bytes == nullptr || size < 12 || !Tag(bytes, "RIFF") || !Tag(bytes + 8, "WAVE")) {
		return false;
	}
	WavPcm w;
	bool haveFormat = false;
	UInt32 at = 12;
	while (at + 8 <= size) {
		const UInt32 length = Le32(bytes + at + 4);
		const UInt32 body = at + 8;
		if (length > size - body) {
			return false;
		}
		if (Tag(bytes + at, "fmt ") && length >= 16) {
			if (Le16(bytes + body) != 1) {
				return false;
			}
			w.channels = Le16(bytes + body + 2);
			w.samplesPerSecond = Le32(bytes + body + 4);
			w.blockAlign = Le16(bytes + body + 12);
			w.bitsPerSample = Le16(bytes + body + 14);
			haveFormat = true;
		} else if (Tag(bytes + at, "data")) {
			w.dataOffset = body;
			w.dataBytes = length;
			if (!haveFormat || w.channels == 0 || w.samplesPerSecond == 0 ||
			    (w.bitsPerSample != 8 && w.bitsPerSample != 16) ||
			    w.blockAlign != static_cast<UInt32>(w.channels) * (w.bitsPerSample / 8u)) {
				return false;
			}
			out = w;
			return true;
		}
		// Chunks are padded to an even length.
		at = body + length + (length & 1u);
	}
	return false;
}

// Where a stretch of seconds lies in the samples: whole frames, clamped to
// the data. False when nothing of it is left.
inline bool SliceBytes(const WavPcm& w, float startSeconds, float endSeconds, UInt32& offset, UInt32& bytes) {
	if (w.blockAlign == 0 || w.samplesPerSecond == 0 || !(endSeconds > startSeconds)) {
		return false;
	}
	const UInt32 frames = w.dataBytes / w.blockAlign;
	const auto frameAt = [&](float seconds) {
		if (!(seconds > 0.0f)) {
			return 0u;
		}
		const float f = seconds * static_cast<float>(w.samplesPerSecond);
		return f >= static_cast<float>(frames) ? frames : static_cast<UInt32>(f);
	};
	const UInt32 first = frameAt(startSeconds);
	const UInt32 last = frameAt(endSeconds);
	if (last <= first) {
		return false;
	}
	offset = w.dataOffset + first * w.blockAlign;
	bytes = (last - first) * w.blockAlign;
	return true;
}

// 16-bit samples faded in over their first and out over their last
// `fadeFrames` frames, so a cut starts and ends at nothing - no click.
inline void FadeEdges(SInt16* samples, UInt32 frames, UInt16 channels, UInt32 fadeFrames) {
	if (samples == nullptr || channels == 0 || frames == 0) {
		return;
	}
	const UInt32 fade = fadeFrames * 2 > frames ? frames / 2 : fadeFrames;
	for (UInt32 i = 0; i < fade; ++i) {
		const float gain = static_cast<float>(i) / static_cast<float>(fade);
		for (UInt16 c = 0; c < channels; ++c) {
			SInt16& head = samples[i * channels + c];
			SInt16& tail = samples[(frames - 1 - i) * channels + c];
			head = static_cast<SInt16>(static_cast<float>(head) * gain);
			tail = static_cast<SInt16>(static_cast<float>(tail) * gain);
		}
	}
}

// DirectSound's volume for a linear gain: hundredths of a decibel, 0 at full,
// -10000 (DSBVOLUME_MIN, dsound.h) at nothing.
inline SInt32 DirectSoundVolume(float gain) {
	if (!(gain > 0.0001f)) {
		return -10000;
	}
	if (gain >= 1.0f) {
		return 0;
	}
	// 2000 * log10(gain). No log is imported (core/MathFns.h keeps that list
	// short): the gain halved into [0.5, 1), ln of that by its series, and
	// ln 2 for each halving.
	int twos = 0;
	float x = gain;
	while (x < 0.5f) {
		x *= 2.0f;
		++twos;
	}
	// ln(x) for x in [0.5, 1): ln((1+y)/(1-y)) = 2(y + y^3/3 + y^5/5 + ...).
	const float y = (x - 1.0f) / (x + 1.0f);
	const float y2 = y * y;
	const float ln = 2.0f * y * (1.0f + y2 * (1.0f / 3.0f + y2 * (1.0f / 5.0f + y2 * (1.0f / 7.0f + y2 / 9.0f)))) -
	                 static_cast<float>(twos) * 0.69314718f;
	const float hundredths = 2000.0f * ln / 2.30258509f;
	return hundredths < -10000.0f ? -10000 : static_cast<SInt32>(hundredths - 0.5f);
}

// A file in a TES4 archive (Oblivion's .bsa, version 103), from its index:
// the 36-byte header, the folder records, each folder's name and file
// records, then the file names - the layout the game's archives were read
// with (2026-09-30, "Oblivion - Sounds.bsa": version 103, flags 0x713).
struct BsaEntry {
	UInt32 offset = 0;  // of the file's bytes, from the archive's start
	UInt32 size = 0;
};

constexpr UInt32 kBsaHeaderBytes = 36;
constexpr UInt32 kBsaVersionOblivion = 103;
constexpr UInt32 kBsaFolderNames = 0x1;
constexpr UInt32 kBsaFileNames = 0x2;
constexpr UInt32 kBsaCompressed = 0x4;
constexpr UInt32 kBsaSizeToggle = 0x40000000;

// How many bytes from the archive's start its index takes, from the header;
// 0 for no TES4 archive of version 103 with both kinds of names.
inline UInt32 BsaIndexBytes(const UInt8* header, UInt32 size) {
	using namespace sound_detail;
	if (header == nullptr || size < kBsaHeaderBytes || !Tag(header, "BSA\0") ||
	    Le32(header + 4) != kBsaVersionOblivion || Le32(header + 8) != kBsaHeaderBytes) {
		return 0;
	}
	const UInt32 flags = Le32(header + 12);
	if ((flags & (kBsaFolderNames | kBsaFileNames)) != (kBsaFolderNames | kBsaFileNames)) {
		return 0;
	}
	const UInt64 folders = Le32(header + 16);
	const UInt64 files = Le32(header + 20);
	const UInt64 total = kBsaHeaderBytes + folders * 16 + Le32(header + 24) + folders + files * 16 + Le32(header + 28);
	return total > 0x10000000ull ? 0 : static_cast<UInt32>(total);
}

// The file `folder\name` (either case, backslashes) in the index; false when
// it is not there, or stored compressed - that is not cut here.
inline bool BsaFind(const UInt8* index, UInt32 size, const char* folder, const char* name, BsaEntry& out) {
	using namespace sound_detail;
	const UInt32 need = BsaIndexBytes(index, size);
	if (need == 0 || need > size || folder == nullptr || name == nullptr) {
		return false;
	}
	const UInt32 flags = Le32(index + 12);
	const UInt32 folders = Le32(index + 16);
	const UInt32 files = Le32(index + 20);
	const UInt32 fileNamesAt = need - Le32(index + 28);
	UInt32 at = kBsaHeaderBytes + folders * 16;
	UInt32 fileIndex = 0;
	UInt32 nameAt = fileNamesAt;
	for (UInt32 f = 0; f < folders; ++f) {
		const UInt32 count = Le32(index + kBsaHeaderBytes + f * 16 + 8);
		if (at >= fileNamesAt) {
			return false;
		}
		const UInt32 nameLength = index[at];
		const char* const folderName = reinterpret_cast<const char*>(index + at + 1);
		at += 1 + nameLength;
		bool folderMatches = nameLength > 0;
		for (UInt32 i = 0; folderMatches && i + 1 < nameLength; ++i) {
			folderMatches = folder[i] != '\0' && Lower(folder[i]) == Lower(folderName[i]);
		}
		folderMatches = folderMatches && folder[nameLength - 1] == '\0';
		for (UInt32 i = 0; i < count; ++i, ++fileIndex) {
			if (at + 16 > fileNamesAt || fileIndex >= files) {
				return false;
			}
			const UInt32 rawSize = Le32(index + at + 8);
			const UInt32 offset = Le32(index + at + 12);
			at += 16;
			// This file's name: the next in the names block.
			const char* const fileName = reinterpret_cast<const char*>(index + nameAt);
			UInt32 length = 0;
			while (nameAt + length < need && fileName[length] != '\0') {
				++length;
			}
			nameAt += length + 1;
			if (!folderMatches) {
				continue;
			}
			bool same = true;
			for (UInt32 c = 0; same && c < length; ++c) {
				same = name[c] != '\0' && Lower(name[c]) == Lower(fileName[c]);
			}
			if (!same || name[length] != '\0') {
				continue;
			}
			const bool compressed = ((flags & kBsaCompressed) != 0) != ((rawSize & kBsaSizeToggle) != 0);
			if (compressed) {
				return false;
			}
			out.offset = offset;
			out.size = rawSize & ~kBsaSizeToggle;
			return true;
		}
	}
	return false;
}

}  // namespace obvr::game
