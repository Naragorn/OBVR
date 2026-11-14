#include "game/SheathTrace.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/NiMath.h"

namespace obvr::game {

namespace {

constexpr UInt32 kTraced = 3;  // SideWeapon, BackWeapon, the body's Weapon
constexpr UInt32 kTracesPerSession = 8;
const char* const kWeaponName[] = {"Weapon"};

struct TraceState {
	UInt32 firstChild[kTraced] = {};
	UInt32 framesLeft = 0;
	UInt32 frame = 0;
	UInt32 tracesLeft = kTracesPerSession;
};

TraceState& State() {
	static TraceState s;
	return s;
}

bool Looks(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }

NiAVObject* FirstChild(const NiAVObject* node) {
	if (!Looks(node)) {
		return nullptr;
	}
	const auto* bytes = reinterpret_cast<const UInt8*>(node);
	auto* const* children = *reinterpret_cast<NiAVObject* const* const*>(bytes + addr::kNiChildrenOffset);
	const UInt16 count = *reinterpret_cast<const UInt16*>(bytes + addr::kNiChildCountOffset);
	if (!Looks(children) || count > 512) {
		return nullptr;
	}
	for (UInt16 i = 0; i < count; ++i) {
		if (Looks(children[i])) {
			return children[i];
		}
	}
	return nullptr;
}

// Where `p` stands in the root's frame (x the root's right, y forward, z up).
NiPoint3 InRoot(const NiAVObject* root, const NiPoint3& p) {
	return InverseMatrix(root->worldTransform.rot) * (p - root->worldTransform.pos);
}

}  // namespace

void TraceSheaths(NiAVObject* root, NiAVObject* const* sheaths, const bool* mirrored) {
	TraceState& s = State();
	if (!Looks(root)) {
		return;
	}
	NiAVObject* weapon = nullptr;
	FindNodesNamed(root, kWeaponName, 1, &weapon);
	NiAVObject* const nodes[kTraced] = {sheaths[0], sheaths[1], weapon};
	static const char* const kNames[kTraced] = {"SideWeapon", "BackWeapon", "Weapon"};
	NiAVObject* child[kTraced] = {};
	bool changed = false;
	for (UInt32 i = 0; i < kTraced; ++i) {
		child[i] = FirstChild(nodes[i]);
		const UInt32 id = reinterpret_cast<UInt32>(child[i]);
		changed = changed || id != s.firstChild[i];
		s.firstChild[i] = id;
	}
	++s.frame;
	if (changed && s.framesLeft == 0 && s.tracesLeft > 0) {
		--s.tracesLeft;
		s.framesLeft = kSheathTraceFrames;
		OBVR_LOG("Sheath trace: a child changed at frame %u - %u frames follow (%u traces left)", s.frame,
		         kSheathTraceFrames, s.tracesLeft);
	}
	if (s.framesLeft == 0) {
		return;
	}
	--s.framesLeft;
	char line[512];
	int at = std::snprintf(line, sizeof(line), "Sheath trace: f%u mirror %d%d", s.frame, mirrored[0] ? 1 : 0,
	                       mirrored[1] ? 1 : 0);
	for (UInt32 i = 0; i < kTraced && at > 0 && at < static_cast<int>(sizeof(line)); ++i) {
		if (!Looks(nodes[i])) {
			at += std::snprintf(line + at, sizeof(line) - at, " | %s none", kNames[i]);
			continue;
		}
		const NiPoint3 n = InRoot(root, nodes[i]->worldTransform.pos);
		if (!Looks(child[i])) {
			at += std::snprintf(line + at, sizeof(line) - at, " | %s %.1f %.1f %.1f empty", kNames[i],
			                    static_cast<double>(n.x), static_cast<double>(n.y), static_cast<double>(n.z));
			continue;
		}
		const NiPoint3 c = InRoot(root, child[i]->worldTransform.pos);
		const bool hidden = (child[i]->flags & 1) != 0 || (nodes[i]->flags & 1) != 0;
		at += std::snprintf(line + at, sizeof(line) - at, " | %s %.1f %.1f %.1f, %08X at %.1f %.1f %.1f%s", kNames[i],
		                    static_cast<double>(n.x), static_cast<double>(n.y), static_cast<double>(n.z),
		                    reinterpret_cast<UInt32>(child[i]), static_cast<double>(c.x), static_cast<double>(c.y),
		                    static_cast<double>(c.z), hidden ? " hidden" : "");
	}
	OBVR_LOG("%s", line);
}

}  // namespace obvr::game
