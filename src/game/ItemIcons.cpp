#include "game/ItemIcons.h"

#include <cstring>
#include <vector>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "render/DdsImage.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

using DynamicCastFn = void*(__cdecl*)(void* object, UInt32 offset, const void* from, const void* to,
                                      UInt32 isReference);

UInt32 Cast(UInt32 form, UInt32 toType) {
	const auto cast = reinterpret_cast<DynamicCastFn>(addr::kDynamicCast);
	return reinterpret_cast<UInt32>(cast(reinterpret_cast<void*>(form), 0,
	                                     reinterpret_cast<const void*>(addr::kRttiTESForm),
	                                     reinterpret_cast<const void*>(toType), 0));
}

// A BSStringT {char* data; UInt16 length; UInt16 capacity} into `out`.
bool ReadString(UInt32 string, char* out, UInt32 size) {
	out[0] = '\0';
	const UInt32 text = Read(string);
	const UInt16 length = *reinterpret_cast<const UInt16*>(string + 4);
	if (!LooksLikeObject(text) || length == 0) {
		return false;
	}
	UInt32 i = 0;
	for (; i + 1 < size && i < length; ++i) {
		const char c = reinterpret_cast<const char*>(text)[i];
		if (c == '\0') {
			break;
		}
		out[i] = c;
	}
	out[i] = '\0';
	return i > 0;
}

// The whole file through the engine's opener; false when it does not open.
bool ReadGameFile(const char* path, std::vector<UInt8>& out) {
	out.clear();
	using GetFileFn = void*(__cdecl*)(const char* path, UInt32 mode, UInt32 bufferBytes);
	void* const file =
		reinterpret_cast<GetFileFn>(addr::kNiFileGetFile)(path, addr::kNiFileReadMode, addr::kNiFileBufferBytes);
	const UInt32 at = reinterpret_cast<UInt32>(file);
	if (!LooksLikeObject(at) || !LooksLikeObject(Read(at))) {
		return false;
	}
	const UInt32 vtable = Read(at);
	using GoodFn = bool(__thiscall*)(void* file);
	using DestroyFn = void(__thiscall*)(void* file, UInt32 free);
	using ReadFn = UInt32(__cdecl*)(void* stream, void* buffer, UInt32 bytes, UInt32* sizes, UInt32 count);
	const bool good = reinterpret_cast<GoodFn>(Read(vtable + addr::kNiFileGoodSlot))(file);
	const UInt32 readProc = Read(at + addr::kNiFileReadProcOffset);
	if (good && LooksLikeObject(readProc)) {
		// An icon is a few kilobytes; a file past this is not one.
		constexpr UInt32 kChunk = 0x4000;
		constexpr UInt32 kMaxBytes = 0x400000;
		while (out.size() < kMaxBytes) {
			const size_t start = out.size();
			out.resize(start + kChunk);
			UInt32 componentSize = 1;
			const UInt32 got = reinterpret_cast<ReadFn>(readProc)(file, out.data() + start, kChunk,
			                                                      &componentSize, 1);
			out.resize(start + (got <= kChunk ? got : 0));
			if (got != kChunk) {
				break;
			}
		}
	}
	reinterpret_cast<DestroyFn>(Read(vtable))(file, 1);
	return good && !out.empty();
}

struct CachedIcon {
	char path[128] = "";
	bool loaded = false;
	UInt32 lastUse = 0;
	render::Pixel pixels[kItemIconSide * kItemIconSide];
};

CachedIcon g_cache[kItemIconCacheSize];
UInt32 g_useClock = 0;
UInt32 g_logLinesLeft = 24;

void Load(CachedIcon& slot, const char* iconPath) {
	strncpy_s(slot.path, iconPath, _TRUNCATE);
	slot.loaded = false;
	char file[260];
	if (!IconFilePath(iconPath, file, sizeof(file))) {
		return;
	}
	std::vector<UInt8> bytes;
	const bool read = ReadGameFile(file, bytes);
	std::vector<render::Pixel> decoded;
	UInt32 width = 0;
	UInt32 height = 0;
	const bool ok = read && render::DecodeDds(bytes.data(), static_cast<UInt32>(bytes.size()), decoded,
	                                          width, height);
	if (ok) {
		render::FitSquare(decoded.data(), width, height, slot.pixels, kItemIconSide);
		// The overlays paint in the texture's order, blue first.
		for (render::Pixel& p : slot.pixels) {
			const UInt8 r = p.r;
			p.r = p.b;
			p.b = r;
		}
		slot.loaded = true;
	}
	if (g_logLinesLeft > 0) {
		--g_logLinesLeft;
		if (ok) {
			OBVR_LOG("QuickMenu: icon %s read (%u bytes, %ux%u)", file, static_cast<UInt32>(bytes.size()),
			         width, height);
		} else {
			OBVR_LOG("QuickMenu: icon %s %s - the slot shows its name only", file,
			         read ? "is not a DDS this reads" : "did not open");
		}
	}
}

}  // namespace

void ReadIconPath(UInt32 form, char* out, UInt32 size) {
	if (out == nullptr || size == 0) {
		return;
	}
	out[0] = '\0';
	if (!LooksLikeObject(form)) {
		return;
	}
	const UInt32 icon = Cast(form, addr::kRttiTESIcon);
	if (LooksLikeObject(icon) && ReadString(icon + addr::kIconPathOffset, out, size)) {
		return;
	}
	const UInt32 biped = Cast(form, addr::kRttiTESBipedModelForm);
	if (LooksLikeObject(biped)) {
		// The male icon, the female one when a mod gave only that.
		if (ReadString(biped + addr::kBipedMaleIconPathOffset, out, size) ||
		    ReadString(biped + addr::kBipedFemaleIconPathOffset, out, size)) {
			return;
		}
	}
	// A spell: its first effect's icon.
	const UInt32 effects = Cast(form, addr::kRttiEffectItemList);
	if (!LooksLikeObject(effects)) {
		return;
	}
	const UInt32 item = Read(effects + addr::kEffectListFirstItemOffset);
	if (!LooksLikeObject(item)) {
		return;
	}
	const UInt32 setting = Read(item + addr::kEffectItemSettingOffset);
	if (!LooksLikeObject(setting)) {
		return;
	}
	const UInt32 effectIcon = Cast(setting, addr::kRttiTESIcon);
	if (LooksLikeObject(effectIcon)) {
		ReadString(effectIcon + addr::kIconPathOffset, out, size);
	}
}

const render::Pixel* ItemIcon(const char* iconPath) {
	if (iconPath == nullptr || iconPath[0] == '\0') {
		return nullptr;
	}
	++g_useClock;
	bool used[kItemIconCacheSize];
	UInt32 lastUse[kItemIconCacheSize];
	for (UInt32 i = 0; i < kItemIconCacheSize; ++i) {
		CachedIcon& slot = g_cache[i];
		if (slot.path[0] != '\0' && std::strlen(slot.path) == std::strlen(iconPath) &&
		    StartsWithNoCase(slot.path, iconPath)) {
			slot.lastUse = g_useClock;
			return slot.loaded ? slot.pixels : nullptr;
		}
		used[i] = slot.path[0] != '\0';
		lastUse[i] = slot.lastUse;
	}
	CachedIcon& victim = g_cache[IconCacheVictim(used, lastUse)];
	Load(victim, iconPath);
	victim.lastUse = g_useClock;
	return victim.loaded ? victim.pixels : nullptr;
}

}  // namespace obvr::game
