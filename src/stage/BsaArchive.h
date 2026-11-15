#pragma once

// A file looked up in one of Oblivion's archives (BSA, version 103) by its
// path, from the archive's directory held in memory. The layout as UESP's
// "Oblivion Mod:BSA File Format" gives it and as tools/bsa-extract.py reads
// it: a 36-byte header, one 16-byte record a folder, then a folder at a time
// its name and one 16-byte record a file, then every file's name in the same
// order. Pure over bytes; the reading is the caller's (stage/DomeBuild.cpp).

#include <cstddef>
#include <cstdint>

#include "stage/DdsImage.h"

namespace obvr::stage {

inline constexpr std::size_t kBsaHeaderBytes = 36;
inline constexpr std::uint32_t kBsaFolderNames = 0x1;   // archive flag: the folders' names are there
inline constexpr std::uint32_t kBsaFileNames = 0x2;     // and the files'
inline constexpr std::uint32_t kBsaCompressed = 0x4;    // files are compressed unless a file says otherwise
inline constexpr std::uint32_t kBsaSizeToggle = 0x40000000u;  // a file's size bit: the other way round for this one

struct BsaHeader {
	bool valid = false;
	std::uint32_t folderOffset = 0;
	std::uint32_t flags = 0;
	std::uint32_t folderCount = 0;
	std::uint32_t fileCount = 0;
	std::uint32_t folderNamesBytes = 0;
	std::uint32_t fileNamesBytes = 0;
};

// valid only for a version 103 archive that names its folders and files.
inline BsaHeader ReadBsaHeader(const std::uint8_t* data, std::size_t size) {
	BsaHeader h;
	if (data == nullptr || size < kBsaHeaderBytes || ReadU32(data) != 0x00415342u || ReadU32(data + 4) != 103u) {
		return h;
	}
	h.folderOffset = ReadU32(data + 8);
	h.flags = ReadU32(data + 12);
	h.folderCount = ReadU32(data + 16);
	h.fileCount = ReadU32(data + 20);
	h.folderNamesBytes = ReadU32(data + 24);
	h.fileNamesBytes = ReadU32(data + 28);
	h.valid = h.folderOffset == kBsaHeaderBytes && (h.flags & kBsaFolderNames) != 0 && (h.flags & kBsaFileNames) != 0;
	return h;
}

// How many bytes from the archive's start hold the whole directory.
inline std::uint64_t BsaDirectoryBytes(const BsaHeader& h) {
	return static_cast<std::uint64_t>(kBsaHeaderBytes) + static_cast<std::uint64_t>(h.folderCount) * 16 +
	       h.folderNamesBytes + h.folderCount + static_cast<std::uint64_t>(h.fileCount) * 16 + h.fileNamesBytes;
}

struct BsaEntry {
	std::uint32_t offset = 0;  // from the archive's start
	std::uint32_t size = 0;    // as stored
	bool compressed = false;   // stored as four bytes of the full size and a zlib stream
};

// A path's letters without their case, either slash the same.
inline char BsaPathChar(char c) {
	if (c == '/') {
		return '\\';
	}
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// name[0..nameLength) against the path's part [from, to).
inline bool BsaNameIs(const std::uint8_t* name, std::size_t nameLength, const char* from, const char* to) {
	if (nameLength != static_cast<std::size_t>(to - from)) {
		return false;
	}
	for (std::size_t i = 0; i < nameLength; ++i) {
		if (BsaPathChar(static_cast<char>(name[i])) != BsaPathChar(from[i])) {
			return false;
		}
	}
	return true;
}

// The start of the index-th name in the files' names, null past the block.
inline const std::uint8_t* BsaFileName(const std::uint8_t* names, const std::uint8_t* end, std::uint32_t index) {
	const std::uint8_t* at = names;
	for (std::uint32_t i = 0; i < index; ++i) {
		while (at < end && *at != 0) {
			++at;
		}
		if (at >= end) {
			return nullptr;
		}
		++at;
	}
	return at < end ? at : nullptr;
}

// The file among one folder's records; firstIndex is the folder's first file
// in the names. False when it is not there or the bytes end early.
inline bool FindInBsaFolder(const std::uint8_t* records, std::uint32_t count, const std::uint8_t* names,
                            const std::uint8_t* end, std::uint32_t firstIndex, const char* file, const char* fileEnd,
                            bool compressedDefault, BsaEntry& out) {
	const std::uint8_t* name = BsaFileName(names, end, firstIndex);
	for (std::uint32_t j = 0; j < count && name != nullptr; ++j) {
		const std::uint8_t* nameEnd = name;
		while (nameEnd < end && *nameEnd != 0) {
			++nameEnd;
		}
		if (nameEnd >= end) {
			return false;
		}
		if (BsaNameIs(name, static_cast<std::size_t>(nameEnd - name), file, fileEnd)) {
			const std::uint32_t size = ReadU32(records + static_cast<std::size_t>(j) * 16 + 8);
			out.size = size & ~kBsaSizeToggle;
			out.offset = ReadU32(records + static_cast<std::size_t>(j) * 16 + 12);
			out.compressed = compressedDefault != ((size & kBsaSizeToggle) != 0);
			return true;
		}
		name = nameEnd + 1 < end ? nameEnd + 1 : nullptr;
	}
	return false;
}

// path ("textures\sky\skystars.dds", either slash, any case) in the
// directory's bytes: dir is the archive's first BsaDirectoryBytes(h) bytes.
inline bool FindInBsa(const std::uint8_t* dir, std::size_t dirSize, const BsaHeader& h, const char* path, BsaEntry& out) {
	if (!h.valid || dir == nullptr || path == nullptr || dirSize < BsaDirectoryBytes(h)) {
		return false;
	}
	const char* pathEnd = path;
	const char* slash = nullptr;
	for (; *pathEnd != 0; ++pathEnd) {
		if (*pathEnd == '\\' || *pathEnd == '/') {
			slash = pathEnd;
		}
	}
	if (slash == nullptr) {
		return false;
	}
	const std::uint8_t* end = dir + BsaDirectoryBytes(h);
	const std::uint8_t* names = end - h.fileNamesBytes;
	std::uint32_t firstIndex = 0;
	for (std::uint32_t i = 0; i < h.folderCount; ++i) {
		const std::uint8_t* record = dir + kBsaHeaderBytes + static_cast<std::size_t>(i) * 16;
		const std::uint32_t count = ReadU32(record + 8);
		const std::uint64_t at = static_cast<std::uint64_t>(ReadU32(record + 12)) - h.fileNamesBytes;
		if (ReadU32(record + 12) < h.fileNamesBytes || at + 1 > static_cast<std::uint64_t>(names - dir)) {
			return false;
		}
		const std::size_t nameBytes = dir[at];  // the name's length with its closing zero
		const std::uint64_t recordsAt = at + 1 + nameBytes;
		if (nameBytes == 0 || recordsAt + static_cast<std::uint64_t>(count) * 16 > static_cast<std::uint64_t>(names - dir)) {
			return false;
		}
		if (BsaNameIs(dir + at + 1, nameBytes - 1, path, slash)) {
			return FindInBsaFolder(dir + recordsAt, count, names, end, firstIndex, slash + 1, pathEnd,
			                       (h.flags & kBsaCompressed) != 0, out);
		}
		firstIndex += count;
	}
	return false;
}

}  // namespace obvr::stage
