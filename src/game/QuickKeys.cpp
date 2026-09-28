#include "game/QuickKeys.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "game/ItemIcons.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

using DynamicCastFn = void*(__cdecl*)(void* object, UInt32 offset, const void* from, const void* to,
                                      UInt32 isReference);

// The form's TESFullName name into `out`, cut to fit; empty when it has none.
void ReadFullName(UInt32 form, char* out, UInt32 size) {
	out[0] = '\0';
	const auto cast = reinterpret_cast<DynamicCastFn>(addr::kDynamicCast);
	const UInt32 fullName = reinterpret_cast<UInt32>(
		cast(reinterpret_cast<void*>(form), 0, reinterpret_cast<const void*>(addr::kRttiTESForm),
		     reinterpret_cast<const void*>(addr::kRttiTESFullName), 0));
	if (!LooksLikeObject(fullName)) {
		return;
	}
	const UInt32 text = Read(fullName + addr::kFullNameStringOffset);
	const UInt16 length = *reinterpret_cast<const UInt16*>(fullName + addr::kFullNameStringOffset + 4);
	if (!LooksLikeObject(text) || length == 0) {
		return;
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
}

}  // namespace

void ReadQuickKeys(QuickKeySlot (&slots)[kQuickKeyCount]) {
	for (int i = 0; i < kQuickKeyCount; ++i) {
		QuickKeySlot& slot = slots[i];
		slot = QuickKeySlot{};
		const UInt32 list = addr::kQuickKeyLists + static_cast<UInt32>(i) * addr::kQuickKeyListStride;
		const UInt32 node = Read(list + addr::kQuickKeyListStartOffset);
		if (!LooksLikeObject(node)) {
			continue;
		}
		const UInt32 form = Read(node + addr::kQuickKeyNodeDataOffset);
		if (!LooksLikeObject(form)) {
			continue;
		}
		slot.filled = true;
		slot.formType = *reinterpret_cast<const UInt8*>(form + addr::kFormTypeOffset);
		ReadFullName(form, slot.name, sizeof(slot.name));
		ReadIconPath(form, slot.iconPath, sizeof(slot.iconPath));
	}
}

void DescribeQuickKeyLists(char* out, UInt32 size) {
	if (size == 0) {
		return;
	}
	out[0] = '\0';
	UInt32 used = 0;
	for (int i = 0; i < kQuickKeyCount; ++i) {
		const UInt32 list = addr::kQuickKeyLists + static_cast<UInt32>(i) * addr::kQuickKeyListStride;
		const UInt32 start = Read(list + addr::kQuickKeyListStartOffset);
		const UInt32 count = Read(list + addr::kQuickKeyListCountOffset);
		const UInt32 form = LooksLikeObject(start) ? Read(start + addr::kQuickKeyNodeDataOffset) : 0;
		const int n = std::snprintf(out + used, size - used, "%s%d:%08X/%u->%08X", i == 0 ? "" : " ",
		                            i + 1, start, count, form);
		if (n <= 0 || used + static_cast<UInt32>(n) >= size) {
			break;
		}
		used += static_cast<UInt32>(n);
	}
}

}  // namespace obvr::game
