#include "platform/ImportHook.h"

#include "platform/Win32Min.h"

namespace obvr::platform {
namespace {

// The parts of the PE format needed to walk an import table, and no more.
//
// Read out of Microsoft's own winnt.h rather than recalled. Every offset below
// is a fixed part of the format - it has not changed since Windows NT - and
// the ones that matter are checked against their signatures before use, so a
// module that is not a PE file is refused rather than wandered through.

// IMAGE_DOS_HEADER: "MZ", then a great deal that is not used, then the offset
// of the real header at 0x3C.
constexpr UInt32 kDosMagicOffset = 0x00;
constexpr UInt32 kNtHeaderOffsetField = 0x3C;
constexpr UInt16 kDosMagic = 0x5A4D;  // 'MZ', little endian

// IMAGE_NT_HEADERS32: signature, then the file header, then the optional one.
constexpr UInt32 kNtSignature = 0x00004550;  // 'PE\0\0'
constexpr UInt32 kFileHeaderSize = 20;
constexpr UInt32 kOptionalHeaderOffset = 4 + kFileHeaderSize;

// IMAGE_OPTIONAL_HEADER32: the data directory array begins at 96, and the
// import directory is its second entry - address then size, four bytes each.
constexpr UInt32 kDataDirectoryOffset = 96;
constexpr UInt32 kImportDirectoryIndex = 1;

// IMAGE_IMPORT_DESCRIPTOR, twenty bytes, terminated by an all-zero one.
constexpr UInt32 kDescriptorSize = 20;
constexpr UInt32 kOriginalFirstThunk = 0;
constexpr UInt32 kDescriptorName = 12;
constexpr UInt32 kFirstThunk = 16;

// IMAGE_IMPORT_BY_NAME: a two-byte hint, then the name.
constexpr UInt32 kHintSize = 2;

// The top bit of a thunk means the import is by ordinal, and then there is no
// name to compare against.
constexpr UInt32 kOrdinalFlag = 0x80000000;

UInt32 Read32(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
UInt16 Read16(UInt32 address) { return *reinterpret_cast<const UInt16*>(address); }

char Lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

bool EqualsIgnoreCase(const char* a, const char* b) {
	while (*a != '\0' && *b != '\0') {
		if (Lower(*a) != Lower(*b)) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

bool Equals(const char* a, const char* b) {
	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

}  // namespace

bool ReplaceImport(UInt32 moduleBase, const char* importedFrom, const char* functionName,
                   void* replacement, void** previous) {
	if (moduleBase == 0 || importedFrom == nullptr || functionName == nullptr ||
	    replacement == nullptr) {
		return false;
	}

	// Refused rather than walked, if this is not a PE image. Reading an import
	// table out of something that is not one would follow offsets into
	// arbitrary memory.
	if (Read16(moduleBase + kDosMagicOffset) != kDosMagic) {
		return false;
	}

	const UInt32 ntHeader = moduleBase + Read32(moduleBase + kNtHeaderOffsetField);
	if (Read32(ntHeader) != kNtSignature) {
		return false;
	}

	const UInt32 importRva =
		Read32(ntHeader + kOptionalHeaderOffset + kDataDirectoryOffset +
		       kImportDirectoryIndex * 8);
	if (importRva == 0) {
		return false;
	}

	for (UInt32 descriptor = moduleBase + importRva;; descriptor += kDescriptorSize) {
		const UInt32 nameRva = Read32(descriptor + kDescriptorName);
		const UInt32 firstThunkRva = Read32(descriptor + kFirstThunk);

		// The table ends with a descriptor of all zeroes.
		if (nameRva == 0 && firstThunkRva == 0) {
			return false;
		}

		if (!EqualsIgnoreCase(reinterpret_cast<const char*>(moduleBase + nameRva),
		                      importedFrom)) {
			continue;
		}

		// Names come from the original thunk array and addresses from the
		// first - the loader overwrites the second with resolved addresses and
		// leaves the first alone. A module built without the original array
		// has no names to match, and guessing by position would be worse than
		// declining.
		const UInt32 nameThunkRva = Read32(descriptor + kOriginalFirstThunk);
		if (nameThunkRva == 0 || firstThunkRva == 0) {
			return false;
		}

		UInt32 nameThunk = moduleBase + nameThunkRva;
		UInt32 addressThunk = moduleBase + firstThunkRva;

		for (; Read32(nameThunk) != 0; nameThunk += 4, addressThunk += 4) {
			const UInt32 entry = Read32(nameThunk);
			if ((entry & kOrdinalFlag) != 0) {
				// Imported by number, so there is no name to compare.
				continue;
			}

			const char* name =
				reinterpret_cast<const char*>(moduleBase + entry + kHintSize);
			if (!Equals(name, functionName)) {
				continue;
			}

			void** slot = reinterpret_cast<void**>(addressThunk);

			DWORD protection = 0;
			if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) {
				return false;
			}

			if (previous != nullptr) {
				*previous = *slot;
			}
			*slot = replacement;

			DWORD ignored = 0;
			VirtualProtect(slot, sizeof(void*), protection, &ignored);
			return true;
		}

		// The module is imported but this function is not among its names.
		return false;
	}
}

}  // namespace obvr::platform
