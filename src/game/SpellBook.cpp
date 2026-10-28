#include "game/SpellBook.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "game/ItemIcons.h"
#include "game/QuickKeys.h"
#include "game/SpellReachLogic.h"

#include <cstdio>

namespace obvr::game {
namespace {

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

// Enough for any vanilla or modded spell; a longer list is cut there.
constexpr int kMaxSpellEffects = 16;

// The player's base form (the TESNPC), 0 when it cannot be read.
UInt32 PlayerBaseForm() {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return 0;
	}
	const UInt32 base = Read(player + addr::kRefBaseFormOffset);
	return LooksLikeObject(base) ? base : 0;
}

// A list that loops, or runs on into memory that is no list, ends here.
constexpr UInt32 kMostSpellNodes = 2048;

}  // namespace

bool ReadPlayerSpells(vr::SpellBook& book, UInt32& leftOut) {
	book = vr::SpellBook{};
	leftOut = 0;
	const UInt32 base = PlayerBaseForm();
	if (base == 0) {
		return false;
	}
	UInt32 node = base + addr::kActorBaseSpellListOffset;
	for (UInt32 n = 0; n < kMostSpellNodes && LooksLikeObject(node); ++n) {
		const UInt32 form = Read(node);
		if (form == 0) {
			break;
		}
		if (LooksLikeObject(form)) {
			const UInt32 spellType = Read(form + addr::kSpellTypeOffset);
			if (vr::SpellIsCastable(spellType)) {
				char name[40];
				char icon[128];
				ReadFormFullName(form, name, sizeof(name));
				ReadIconPath(form, icon, sizeof(icon));
				if (!vr::AddSpell(book, form, spellType, name, icon)) {
					++leftOut;
				}
			}
		}
		node = Read(node + addr::kSpellListNodeNextOffset);
	}
	vr::SortSpellBook(book);
	return true;
}

void DescribeSpellList(char* out, UInt32 size) {
	if (size == 0) {
		return;
	}
	out[0] = '\0';
	const UInt32 base = PlayerBaseForm();
	UInt32 used = 0;
	int n = std::snprintf(out, size, "base %08X:", base);
	if (n < 0 || static_cast<UInt32>(n) >= size) {
		return;
	}
	used = static_cast<UInt32>(n);
	UInt32 node = base != 0 ? base + addr::kActorBaseSpellListOffset : 0;
	for (int i = 0; i < 12 && LooksLikeObject(node); ++i) {
		const UInt32 form = Read(node);
		const UInt32 type = LooksLikeObject(form) ? *reinterpret_cast<const UInt8*>(form + addr::kFormTypeOffset) : 0xFF;
		const UInt32 spellType = LooksLikeObject(form) ? Read(form + addr::kSpellTypeOffset) : 0xFFFFFFFF;
		char name[40] = "";
		UInt32 refId = 0;
		UInt32 cost = 0;
		UInt32 level = 0;
		if (LooksLikeObject(form)) {
			ReadFormFullName(form, name, sizeof(name));
			refId = Read(form + addr::kFormIdOffset);
			cost = Read(form + addr::kSpellTypeOffset + 4);
			level = Read(form + addr::kSpellTypeOffset + 8);
		}
		n = std::snprintf(out + used, size - used, " %08X:%08X(%08X \"%s\")/%02X/%X/%u/%u", node, form, refId, name,
		                  type, spellType, cost, level);
		if (n <= 0 || used + static_cast<UInt32>(n) >= size) {
			break;
		}
		used += static_cast<UInt32>(n);
		if (form == 0) {
			break;
		}
		node = Read(node + addr::kSpellListNodeNextOffset);
	}
}

UInt32 ActiveSpellForm(const vr::SpellBook& book) {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return 0;
	}
	using ActiveFn = UInt32(__thiscall*)(void* player);
	const UInt32 item = reinterpret_cast<ActiveFn>(addr::kPlayerGetActiveMagicItem)(reinterpret_cast<void*>(player));
	if (item == 0) {
		return 0;
	}
	for (int i = 0; i < book.count; ++i) {
		if (book.spells[i].form + addr::kMagicItemOffset == item) {
			return book.spells[i].form;
		}
	}
	return 0;
}

vr::SpellReach ActiveSpellReach(UInt32* itemOut) {
	if (itemOut != nullptr) {
		*itemOut = 0;
	}
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return vr::SpellReach::Unknown;
	}
	using ActiveFn = UInt32(__thiscall*)(void* player);
	const UInt32 item = reinterpret_cast<ActiveFn>(addr::kPlayerGetActiveMagicItem)(reinterpret_cast<void*>(player));
	if (itemOut != nullptr) {
		*itemOut = item;
	}
	return SpellReachOf(item, Read, LooksLikeObject, kMaxSpellEffects);
}

bool MakeActiveSpell(UInt32 form) {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player) || !LooksLikeObject(form)) {
		return false;
	}
	using SetFn = UInt8(__thiscall*)(void* player, void* magicItem);
	return reinterpret_cast<SetFn>(addr::kPlayerSetActiveSpell)(
	           reinterpret_cast<void*>(player), reinterpret_cast<void*>(form + addr::kMagicItemOffset)) != 0;
}

}  // namespace obvr::game
