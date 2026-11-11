#include "game/StaggerSides.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "core/AddressSpace.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/StaggerSideLogic.h"

namespace obvr::game {
namespace {

constexpr UInt32 kRefreshAnims = 0x004E3490;  // thiscall(ref), plain ret
constexpr UInt8 kRefreshAnimsEntry[] = {0x6A, 0xFF, 0x68, 0x5C, 0x5C, 0x9B, 0x00};  // push -1; push 009B5C5C
constexpr UInt32 kGameAlloc = 0x00401F00;                                             // cdecl(size)
constexpr UInt8 kFormTypeNpc = 0x23;
constexpr UInt32 kActorBaseAnimList = 0x98;   // TESActorBase +0x94 TESAnimation, its list after the vtable
constexpr UInt32 kActorBaseModelPath = 0xB0;  // TESActorBase +0xAC TESModel, BSStringT m_data at +4
constexpr UInt32 kVtblAnimSequenceMultiple = 0x00A3C758;
// AnimSequenceMultiple: +4 its list holder - +4 the first node, +0xC the
// count; a node {next, ?, BSAnimGroupSequence* at +8}; a sequence's file path
// at +8 (0x00470BF0, 0x00470C40).
constexpr UInt32 kMultipleHolder = 0x04;
constexpr UInt32 kHolderFirst = 0x04;
constexpr UInt32 kHolderCount = 0x0C;
constexpr UInt32 kNodeData = 0x08;
constexpr UInt32 kSequencePath = 0x08;
constexpr int kMostEntries = 8;

struct AnimNode {
	char* name;
	AnimNode* next;
};

using RefFn = void(__thiscall*)(void* ref);

using GetObjectFn = void*(__fastcall*)(void* self, void* edx, const char* name);

// A node of the actor's 3D by name (NiNode GetObjectByName through the vtable,
// as game/Dismember.cpp reads it); nullptr when absent.
const NiAVObject* NodeNamed(UInt32 root, const char* name) {
	const UInt32 vtable = mem::LooksLikeObjectAddress(root) ? *reinterpret_cast<const UInt32*>(root) : 0;
	const UInt32 fn = mem::LooksLikeObjectAddress(vtable)
	                      ? *reinterpret_cast<const UInt32*>(vtable + addr::kNiAVObjectGetObjectVtableOffset)
	                      : 0;
	if (fn < addr::kTextStart || fn >= addr::kTextEnd) {
		return nullptr;
	}
	auto* const node =
		static_cast<const NiAVObject*>(reinterpret_cast<GetObjectFn>(fn)(reinterpret_cast<void*>(root), nullptr, name));
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node)) ? node : nullptr;
}

constexpr UInt32 kRefRotZ = 0x28;  // TESObjectREFR's heading (StaggerSideLogic.h BlowLateral)
// When a stagger's heading is logged: at the pick and after it.
constexpr float kWatchAt[] = {0.0f, 0.3f, 0.6f, 1.0f, 1.5f, 2.2f};

struct SidesState {
	bool installed = false;
	bool enabled = true;
	UInt32 staggerActor = 0;  // inside the engine's stagger start for this actor
	UInt32 blowActor = 0;
	float blowLateral = 0.0f;
	UInt32 addedLines = 6;
	UInt32 pickLines = 24;
	UInt32 refusedLines = 4;
	RefFn originalRefresh = nullptr;
	// The watch on a stagger's heading (StepStaggerWatch): who, which file,
	// its heading at the pick, how long since.
	UInt32 watchActor = 0;
	bool watchMirror = false;
	float watchHeading = 0.0f;
	float watchSeconds = -1.0f;
	UInt32 watchStep = 0;
	UInt32 watchLines = 60;
};

SidesState& State() {
	static SidesState s_state;
	return s_state;
}

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

char* GameString(const char* text) {
	const UInt32 length = static_cast<UInt32>(std::strlen(text)) + 1;
	using AllocFn = void*(__cdecl*)(UInt32 size);
	char* copy = static_cast<char*>(reinterpret_cast<AllocFn>(kGameAlloc)(length));
	if (copy != nullptr) {
		std::memcpy(copy, text, length);
	}
	return copy;
}

// The four names on the NPC's list, each once. Answers how many were added.
UInt32 AddMirrors(UInt32 base) {
	AnimNode* head = reinterpret_cast<AnimNode*>(base + kActorBaseAnimList);
	UInt32 added = 0;
	for (UInt32 i = 0; i < kStaggerMirrorFileCount; ++i) {
		const char* name = kStaggerMirrorFiles[i];
		AnimNode* last = nullptr;
		bool present = false;
		for (AnimNode* n = head; n != nullptr; n = n->next) {
			if (n->name != nullptr && SameAnimName(n->name, name)) {
				present = true;
				break;
			}
			last = n;
			if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(n->next))) {
				break;  // the end, or a list not as read: added after what was walked
			}
		}
		if (present) {
			continue;
		}
		if (head->name == nullptr && head->next == nullptr) {
			head->name = GameString(name);
			added += head->name != nullptr ? 1 : 0;
			continue;
		}
		using AllocFn = void*(__cdecl*)(UInt32 size);
		AnimNode* node = static_cast<AnimNode*>(reinterpret_cast<AllocFn>(kGameAlloc)(sizeof(AnimNode)));
		if (node == nullptr) {
			continue;
		}
		node->name = GameString(name);
		node->next = nullptr;
		if (node->name == nullptr || last == nullptr) {
			continue;
		}
		last->next = node;
		++added;
	}
	return added;
}

void __fastcall HookedRefreshAnims(void* ref, void* /*edx*/) {
	const UInt32 r = reinterpret_cast<UInt32>(ref);
	if (State().enabled && mem::LooksLikeObjectAddress(r) && r != Read(addr::kPlayerPointer)) {
		const UInt32 base = Read(r + addr::kRefBaseFormOffset);
		if (mem::LooksLikeObjectAddress(base) &&
		    *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset) == kFormTypeNpc) {
			const char* skeleton = reinterpret_cast<const char*>(Read(base + kActorBaseModelPath));
			const bool looks = mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(skeleton));
			if (looks && SkeletonTakesStaggerMirrors(skeleton)) {
				const UInt32 added = AddMirrors(base);
				if (added > 0 && State().addedLines > 0) {
					--State().addedLines;
					const AnimNode* head = reinterpret_cast<const AnimNode*>(base + kActorBaseAnimList);
					OBVR_LOG("Stagger sides: %u mirror(s) added to NPC %08X (%s); its list starts with %s", added,
					         Read(base + addr::kFormIdOffset), skeleton,
					         head->name != nullptr ? head->name : "(none)");
				}
			} else if (State().refusedLines > 0) {
				--State().refusedLines;
				OBVR_LOG("Stagger sides: NPC %08X keeps its staggers - skeleton %s", Read(base + addr::kFormIdOffset),
				         looks ? skeleton : "(unreadable)");
			}
		}
	}
	State().originalRefresh(ref);
}

int __cdecl OnStaggerPick(void* entry) {
	if (State().staggerActor == 0) {
		return -1;
	}
	const UInt32 e = reinterpret_cast<UInt32>(entry);
	if (!mem::LooksLikeObjectAddress(e) || Read(e) != kVtblAnimSequenceMultiple) {
		return -1;
	}
	const UInt32 holder = Read(e + kMultipleHolder);
	if (!mem::LooksLikeObjectAddress(holder)) {
		return -1;
	}
	const char* names[kMostEntries] = {};
	int count = 0;
	UInt32 node = Read(holder + kHolderFirst);
	const UInt32 total = Read(holder + kHolderCount);
	while (count < kMostEntries && static_cast<UInt32>(count) < total && mem::LooksLikeObjectAddress(node)) {
		const UInt32 sequence = Read(node + kNodeData);
		const UInt32 path = mem::LooksLikeObjectAddress(sequence) ? Read(sequence + kSequencePath) : 0;
		names[count++] = mem::LooksLikeObjectAddress(path) ? reinterpret_cast<const char*>(path) : nullptr;
		node = Read(node);
	}
	const bool blow = State().blowActor == State().staggerActor;
	const int index = blow ? PickStaggerEntry(names, count, State().blowLateral, kStaggerBlowLateralMin) : -1;
	State().watchActor = State().staggerActor;
	State().watchMirror = index >= 0 && names[index] != nullptr && IsStaggerMirror(names[index]);
	State().watchHeading = *reinterpret_cast<const float*>(State().staggerActor + kRefRotZ);
	State().watchSeconds = 0.0f;
	State().watchStep = 0;
	if (State().pickLines > 0) {
		--State().pickLines;
		char list[320] = {};
		UInt32 at = 0;
		for (int i = 0; i < count && at < sizeof(list) - 1; ++i) {
			const int n = std::snprintf(list + at, sizeof(list) - at, "%s[%d] %s", i == 0 ? "" : ", ", i,
			                            names[i] != nullptr ? AnimFileStem(names[i]) : "(no name)");
			if (n < 0) {
				break;
			}
			at += static_cast<UInt32>(n);
		}
		if (blow) {
			OBVR_LOG("Stagger sides: %08X staggered by a blow %.2f m to its %s - %s (of %u: %s)", State().staggerActor,
			         static_cast<double>(State().blowLateral < 0.0f ? -State().blowLateral : State().blowLateral),
			         State().blowLateral < 0.0f ? "left" : "right",
			         index < 0 ? "the engine's draw" : (names[index] != nullptr ? AnimFileStem(names[index]) : "?"),
			         total, list);
		} else {
			OBVR_LOG("Stagger sides: %08X staggered, no blow of the hands - the engine's draw (of %u: %s)",
			         State().staggerActor, total, list);
		}
	}
	return index;
}

bool FilesPresent() {
	for (UInt32 i = 0; i < kStaggerMirrorFileCount; ++i) {
		char path[MAX_PATH];
		std::snprintf(path, sizeof(path), "Data\\Meshes\\Characters\\_Male\\SpecialAnims\\%s", kStaggerMirrorFiles[i]);
		if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
			OBVR_LOG("Stagger sides: %s is missing - staggers keep the game's one side", path);
			return false;
		}
	}
	return true;
}

}  // namespace

void InstallStaggerSides() {
	if (State().installed) {
		return;
	}
	State().installed = true;
	if (!FilesPresent()) {
		return;
	}
	if (!mem::Verify(kRefreshAnims, kRefreshAnimsEntry, sizeof(kRefreshAnimsEntry)) ||
	    !mem::Verify(kStaggerPickSite, kStaggerPickSiteBytes, sizeof(kStaggerPickSiteBytes))) {
		OBVR_LOG("Stagger sides: the animation refresh or the pick site differ from the bytes read - not installed");
		mem::ReportForeignCode("Stagger sides", kRefreshAnims);
		return;
	}
	constexpr UInt32 kTrampolineCapacity = 16;
	auto* code = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity + 32));
	if (code == nullptr) {
		OBVR_LOG("Stagger sides: no executable memory - not installed");
		return;
	}
	UInt8* trampoline = code;
	UInt8* stub = code + kTrampolineCapacity;
	const UInt32 stubAddress = reinterpret_cast<UInt32>(stub);
	if (mem::BuildEntryTrampoline(trampoline, kTrampolineCapacity, reinterpret_cast<UInt32>(trampoline), kRefreshAnims,
	                              kRefreshAnimsEntry, sizeof(kRefreshAnimsEntry)) == 0 ||
	    BuildStaggerPickStub(stub, 32, stubAddress, reinterpret_cast<UInt32>(&OnStaggerPick)) == 0) {
		OBVR_LOG("Stagger sides: the trampoline or the stub does not fit - not installed");
		return;
	}
	UInt8 entryPatch[16];
	UInt8 sitePatch[8];
	if (mem::BuildEntryPatch(entryPatch, sizeof(entryPatch), kRefreshAnims,
	                         reinterpret_cast<UInt32>(&HookedRefreshAnims),
	                         sizeof(kRefreshAnimsEntry)) != sizeof(kRefreshAnimsEntry) ||
	    BuildStaggerPickSitePatch(sitePatch, sizeof(sitePatch), stubAddress) != sizeof(kStaggerPickSiteBytes)) {
		OBVR_LOG("Stagger sides: the patches have an unexpected length - not installed");
		return;
	}
	State().originalRefresh = reinterpret_cast<RefFn>(trampoline);
	if (!mem::SafeWrite(kStaggerPickSite, sitePatch, sizeof(kStaggerPickSiteBytes))) {
		OBVR_LOG("Stagger sides: could not write the pick site - not installed");
		return;
	}
	if (!mem::SafeWrite(kRefreshAnims, entryPatch, sizeof(kRefreshAnimsEntry))) {
		OBVR_LOG("Stagger sides: could not write the animation refresh's entry - the mirrors are not added");
		return;
	}
	OBVR_LOG("Stagger sides: installed - the refresh %08X detoured, the pick at %08X", kRefreshAnims, kStaggerPickSite);
}

void StepStaggerWatch(float dt) {
	SidesState& st = State();
	if (st.watchSeconds < 0.0f || !mem::LooksLikeObjectAddress(st.watchActor)) {
		st.watchSeconds = -1.0f;
		return;
	}
	if (st.watchStep >= sizeof(kWatchAt) / sizeof(kWatchAt[0])) {
		st.watchSeconds = -1.0f;
		return;
	}
	if (st.watchSeconds >= kWatchAt[st.watchStep] && st.watchLines > 0) {
		--st.watchLines;
		const UInt32 player = Read(addr::kPlayerPointer);
		const float heading = *reinterpret_cast<const float*>(st.watchActor + kRefRotZ);
		const float* at = reinterpret_cast<const float*>(st.watchActor + addr::kRefPositionOffset);
		const float* p = mem::LooksLikeObjectAddress(player)
		                     ? reinterpret_cast<const float*>(player + addr::kRefPositionOffset)
		                     : at;
		const UInt32 root = Read(st.watchActor + addr::kRefNiNodeOffset);
		const NiAVObject* const left = NodeNamed(root, "Bip01 L Thigh");
		const NiAVObject* const right = NodeNamed(root, "Bip01 R Thigh");
		const float body = left != nullptr && right != nullptr
		                       ? BodyFacingOffDegrees(heading, left->worldTransform.pos.x, left->worldTransform.pos.y,
		                                              right->worldTransform.pos.x, right->worldTransform.pos.y)
		                       : 999.0f;
		OBVR_LOG("Stagger watch: %08X (%s) +%.1f s - turned %.0f degrees since the pick, %.0f degrees off facing the "
		         "player %.0f units away; the body's hips face %.0f degrees off its heading",
		         st.watchActor, st.watchMirror ? "the mirror" : "the game's file", static_cast<double>(st.watchSeconds),
		         static_cast<double>(HeadingTurnDegrees(st.watchHeading, heading)),
		         static_cast<double>(FacingOffDegrees(heading, p[0] - at[0], p[1] - at[1])),
		         static_cast<double>(math::Sqrt((p[0] - at[0]) * (p[0] - at[0]) + (p[1] - at[1]) * (p[1] - at[1]))),
		         static_cast<double>(body));
		++st.watchStep;
	}
	st.watchSeconds += dt == dt && dt > 0.0f && dt < 0.5f ? dt : 0.0f;
}

void SetStaggerSides(bool enabled) { State().enabled = enabled; }

void SetStaggerBlow(UInt32 actor, float lateral) {
	State().blowActor = State().enabled ? actor : 0;
	State().blowLateral = lateral;
}

void ClearStaggerBlow() {
	State().blowActor = 0;
	State().blowLateral = 0.0f;
}

void BeginStagger(UInt32 actor) { State().staggerActor = actor; }

void EndStagger() { State().staggerActor = 0; }

}  // namespace obvr::game
