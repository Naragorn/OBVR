#include "game/QuickKeyPages.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

// xOBSE's OBSESerializationInterface (obse/PluginAPI.h, kInterface_
// Serialization = 1, version 1): the calls OBVR uses, in their order.
struct SerializationApi {
	using EventCallback = void (*)(void* reserved);
	UInt32 version;
	void (*SetSaveCallback)(UInt32 plugin, EventCallback callback);
	void (*SetLoadCallback)(UInt32 plugin, EventCallback callback);
	void (*SetNewGameCallback)(UInt32 plugin, EventCallback callback);
	bool (*WriteRecord)(UInt32 type, UInt32 version, const void* buf, UInt32 length);
	bool (*OpenRecord)(UInt32 type, UInt32 version);
	bool (*WriteRecordData)(const void* buf, UInt32 length);
	bool (*GetNextRecordInfo)(UInt32* type, UInt32* version, UInt32* length);
	UInt32 (*ReadRecordData)(void* buf, UInt32 length);
	bool (*ResolveRefID)(UInt32 refID, UInt32* outRefID);
};
constexpr UInt32 kInterfaceSerialization = 1;

SerializationApi* g_serialization = nullptr;
QuickKeyPages g_pages;
UInt32 g_turnLinesLeft = 20;

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

UInt32 ListOf(int slot) { return addr::kQuickKeyLists + static_cast<UInt32>(slot) * addr::kQuickKeyListStride; }

// The form in a slot, 0 when it is empty.
UInt32 FormInSlot(int slot) {
	const UInt32 node = Read(ListOf(slot) + addr::kQuickKeyListStartOffset);
	if (!LooksLikeObject(node)) {
		return 0;
	}
	const UInt32 form = Read(node + addr::kQuickKeyNodeDataOffset);
	return LooksLikeObject(form) ? form : 0;
}

void ReadSlotIds(UInt32 (&ids)[kQuickKeyPageSlots]) {
	for (int i = 0; i < kQuickKeyPageSlots; ++i) {
		const UInt32 form = FormInSlot(i);
		ids[i] = form != 0 ? Read(form + addr::kFormIdOffset) : 0;
	}
}

UInt32 PlayerChanges() {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return 0;
	}
	using ExtraFn = UInt32(__thiscall*)(void* ref);
	using ChangesFn = UInt32(__cdecl*)(UInt32 ref, UInt32 extra);
	const UInt32 extra = reinterpret_cast<ExtraFn>(addr::kRefExtraData)(reinterpret_cast<void*>(player));
	return reinterpret_cast<ChangesFn>(addr::kMakeContainerChanges)(player, extra);
}

bool IsSpell(UInt32 form) { return *reinterpret_cast<const UInt8*>(form + addr::kFormTypeOffset) == addr::kFormTypeSpell; }

// Whether the player's inventory changes have an entry for `form`, and its
// first extra list (0 when it has none) - the stack the inventory's click
// hands over when it is the only one.
bool FindEntry(UInt32 changes, UInt32 form, UInt32& extra) {
	extra = 0;
	for (UInt32 node = Read(changes); LooksLikeObject(node); node = Read(node + 4)) {
		const UInt32 entry = Read(node);
		if (!LooksLikeObject(entry) || Read(entry + addr::kEntryDataFormOffset) != form) {
			continue;
		}
		const UInt32 stacks = Read(entry);
		if (LooksLikeObject(stacks) && LooksLikeObject(Read(stacks))) {
			extra = Read(stacks);
		}
		return true;
	}
	return false;
}

void ClearSlot(int slot, UInt32 changes) {
	const UInt32 list = ListOf(slot);
	if (Read(list + addr::kQuickKeyListCountOffset) == 0) {
		return;
	}
	const UInt32 form = FormInSlot(slot);
	if (form != 0 && !IsSpell(form) && changes != 0) {
		using ClearFn = void(__thiscall*)(void* changes, UInt32 form, SInt32 slot);
		reinterpret_cast<ClearFn>(addr::kClearQuickKeyOfItem)(reinterpret_cast<void*>(changes), form, slot);
	}
	using RemoveAllFn = void(__thiscall*)(void* list);
	reinterpret_cast<RemoveAllFn>(addr::kListRemoveAll)(reinterpret_cast<void*>(list));
}

void SetSlot(int slot, UInt32 form, UInt32 changes) {
	using AddFn = void(__thiscall*)(void* list, UInt32* form);
	UInt32 item = form;
	reinterpret_cast<AddFn>(addr::kListAddTail)(reinterpret_cast<void*>(ListOf(slot)), &item);
	if (IsSpell(form) || changes == 0) {
		return;
	}
	UInt32 extra = 0;
	if (!FindEntry(changes, form, extra)) {
		// Not among the changes to the player's inventory: nothing to give
		// the slot to here. The engine's check after the turn takes the
		// form out of the list again when the player does not carry it.
		return;
	}
	using GetFn = signed char(__thiscall*)(void* extra);
	if (extra != 0 && reinterpret_cast<GetFn>(addr::kQuickKeyOfExtra)(reinterpret_cast<void*>(extra)) != -1) {
		using SetExtraFn = void(__thiscall*)(void* extra, SInt32 slot);
		reinterpret_cast<SetExtraFn>(addr::kSetQuickKeyOfExtra)(reinterpret_cast<void*>(extra), slot);
		return;
	}
	using SetItemFn = void(__thiscall*)(void* changes, UInt32 form, UInt32 extra, SInt32 slot);
	reinterpret_cast<SetItemFn>(addr::kSetQuickKeyOfItem)(reinterpret_cast<void*>(changes), form, extra, slot);
}

using LookupFn = UInt32(__cdecl*)(UInt32 id);
constexpr UInt32 kLookupFormById = 0x0046B250;  // xOBSE GameAPI.cpp, cdecl, as HandBones.cpp

void WriteSlots(const UInt32 (&ids)[kQuickKeyPageSlots]) {
	const UInt32 changes = PlayerChanges();
	for (int i = 0; i < kQuickKeyPageSlots; ++i) {
		ClearSlot(i, changes);
	}
	for (int i = 0; i < kQuickKeyPageSlots; ++i) {
		if (ids[i] == 0) {
			continue;
		}
		const UInt32 form = reinterpret_cast<LookupFn>(kLookupFormById)(ids[i]);
		if (LooksLikeObject(form)) {
			SetSlot(i, form, changes);
		}
	}
	*reinterpret_cast<UInt8*>(addr::kQuickKeysChanged) = 1;
	using CheckFn = void(__cdecl*)();
	reinterpret_cast<CheckFn>(addr::kCheckQuickKeys)();
}

void OnSave(void*) {
	UInt32 now[kQuickKeyPageSlots];
	ReadSlotIds(now);
	const QuickKeyPagesRecord record = RecordOfPages(g_pages, now);
	g_serialization->WriteRecord(kQuickKeyPagesRecord, kQuickKeyPagesVersion, &record, sizeof(record));
}

void OnLoad(void*) {
	g_pages = QuickKeyPages{};
	UInt32 type = 0;
	UInt32 version = 0;
	UInt32 length = 0;
	bool found = false;
	while (g_serialization->GetNextRecordInfo(&type, &version, &length)) {
		if (type != kQuickKeyPagesRecord) {
			continue;
		}
		QuickKeyPagesRecord record;
		const UInt32 read = length == sizeof(record) ? g_serialization->ReadRecordData(&record, sizeof(record)) : 0;
		found = PagesOfRecord(read == sizeof(record) ? &record : nullptr, length, version,
		                      [](UInt32 id, UInt32& out) { return g_serialization->ResolveRefID(id, &out); },
		                      g_pages);
	}
	OBVR_LOG("QuickMenu: pages %s - page %d of the hotkeys shown", found ? "read from the co-save" : "none in the co-save, one page of the game's own",
	         g_pages.current + 1);
}

void OnNewGame(void*) { g_pages = QuickKeyPages{}; }

}  // namespace

bool InstallQuickKeyPages(const obse::Interface* obse) {
	if (obse == nullptr || obse->QueryInterface == nullptr || obse->obseVersion < 15 || obse->GetPluginHandle == nullptr) {
		OBVR_LOG("QuickMenu: xOBSE offers no co-save here - the ring keeps the game's eight hotkeys only");
		return false;
	}
	auto* const api = static_cast<SerializationApi*>(obse->QueryInterface(kInterfaceSerialization));
	if (api == nullptr || api->version < 1) {
		OBVR_LOG("QuickMenu: no serialization interface - the ring keeps the game's eight hotkeys only");
		return false;
	}
	const UInt32 plugin = obse->GetPluginHandle();
	api->SetSaveCallback(plugin, OnSave);
	api->SetLoadCallback(plugin, OnLoad);
	api->SetNewGameCallback(plugin, OnNewGame);
	g_serialization = api;
	return true;
}

bool QuickKeyPagesAvailable() { return g_serialization != nullptr; }

int CurrentQuickKeyPage() { return g_pages.current; }

int TurnQuickKeyPage(int count, bool toFirst) {
	if (g_serialization == nullptr) {
		return 0;
	}
	UInt32 now[kQuickKeyPageSlots];
	ReadSlotIds(now);
	const int from = g_pages.current;
	const int to = toFirst ? 0 : NextQuickKeyPage(g_pages.current, count);
	if (to == from) {
		return from;
	}
	UInt32 write[kQuickKeyPageSlots];
	TurnQuickKeyPages(g_pages, now, to, write);
	WriteSlots(write);
	if (g_turnLinesLeft > 0) {
		--g_turnLinesLeft;
		UInt32 after[kQuickKeyPageSlots];
		ReadSlotIds(after);
		UInt32 wanted = 0;
		UInt32 held = 0;
		for (int i = 0; i < kQuickKeyPageSlots; ++i) {
			wanted += write[i] != 0 ? 1 : 0;
			held += after[i] != 0 ? 1 : 0;
		}
		OBVR_LOG("QuickMenu: page %d of %d - %u of its %u hotkeys in the game's slots (a thing the "
		         "player no longer has is taken out by the game's own check)",
		         to + 1, count, held, wanted);
	}
	return to;
}

}  // namespace obvr::game
