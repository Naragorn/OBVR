#include "game/FirstPersonHide.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/ClassNameCacheLogic.h"
#include "game/FirstPersonArms.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "game/NearShapesLogic.h"
#include "game/NodeNameList.h"

namespace obvr::game {
namespace {

constexpr UInt16 kAppCulledBit = 0x0001;
constexpr UInt32 kMaxHidden = 64;

// How deep a search for a named bone - and the hide walk - may go. The hide
// list's meshes hang a level or two under the root; a bone does not. In a
// biped the hand is the far end of a chain - Bip01, its pelvis, three spine
// bones, the neck or clavicle, upper arm, forearm, hand - well past six
// levels, which is why "Bip01 R Hand" was never found with a limit of six
// (the first Full VR headset run, 2026-09-25: both hands moved as one unit).
// The bones a sheathed weapon hangs on (SideWeapon, BackWeapon, Quiver) are
// as deep, so the hide walk shares the limit. Deep enough for any skeleton,
// still a bound against a cycle in a broken tree.
constexpr UInt32 kMaxFindDepth = 32;

bool LooksLikeObject(const void* pointer) {
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer));
}

// A short run of printable characters, the way a node's name or a type's
// mangled name reads: its length, or 0 for anything else - an empty text, one
// that runs to the limit, one with a character that is no print - which is
// not a string this code should trust.
UInt32 ReadableLength(const char* text, UInt32 limit) {
	if (!LooksLikeObject(text)) {
		return 0;
	}
	for (UInt32 at = 0; at < limit; ++at) {
		const char c = text[at];
		if (c == '\0') {
			return at;
		}
		if (c < 0x20 || c > 0x7E) {
			return 0;
		}
	}
	return 0;
}

bool ReadableText(const char* text, UInt32 limit) { return ReadableLength(text, limit) > 0; }

// The class an object was compiled as, from the compiler's own type
// records: the word before an MSVC vtable is the complete object locator,
// whose fourth word is the type descriptor, whose name sits eight bytes in
// and reads ".?AVNiNode@@". Every step is a read behind a plausibility
// check; a miss anywhere answers "?" rather than a guess. Oblivion.exe
// carries these records - the player's vtable is identified by them
// elsewhere in this project (see kAttackUpdate's comment).
// The names read so far, by vtable (game/ClassNameCacheLogic.h).
VtableNames& KeptClassNames() {
	static VtableNames s_names;
	return s_names;
}

const char* ReadClassName(const UInt8* vtable);

const char* ClassNameOf(const UInt8* object) {
	if (!LooksLikeObject(object)) {
		return "?";
	}
	const UInt8* const vtable = *reinterpret_cast<const UInt8* const*>(object);
	// The game image's type records never change: read once a class. Any
	// other vtable - a plugin's, or a freed object's stray word - is read
	// through every time, as before.
	const UInt32 key = reinterpret_cast<UInt32>(vtable);
	if (!addr::IsGameVtable(key)) {
		return ReadClassName(vtable);
	}
	VtableNames& kept = KeptClassNames();
	if (const char* const name = kept.Find(key)) {
		return name;
	}
	const char* const name = ReadClassName(vtable);
	if (name[0] != '?') {
		kept.Remember(key, name);
	}
	return name;
}

const char* ReadClassName(const UInt8* vtable) {
	if (!LooksLikeObject(vtable) || !LooksLikeObject(vtable - 4)) {
		return "?";
	}
	const UInt8* const locator = *reinterpret_cast<const UInt8* const*>(vtable - 4);
	if (!LooksLikeObject(locator)) {
		return "?";
	}
	const UInt8* const descriptor = *reinterpret_cast<const UInt8* const*>(locator + 0x0C);
	if (!LooksLikeObject(descriptor)) {
		return "?";
	}
	const char* const mangled = reinterpret_cast<const char*>(descriptor + 0x08);
	if (!ReadableText(mangled, 96)) {
		return "?";
	}
	// ".?AVNiNode@@" - the class name starts after the four-character prefix.
	if (mangled[0] == '.' && mangled[1] == '?' && mangled[2] == 'A' &&
	    (mangled[3] == 'V' || mangled[3] == 'U')) {
		return mangled + 4;
	}
	return mangled;
}

bool ClassIsNode(const char* className) { return ClassNameIsNode(className); }

// Whether a scene object is a node - its class's name ending in Node, which
// every walk asks of every object: for a class of the game's already read,
// the answer kept beside its name.
bool ObjectIsNode(const UInt8* object) {
	if (LooksLikeObject(object)) {
		const UInt32 key = *reinterpret_cast<const UInt32*>(object);
		const VtableNames& kept = KeptClassNames();
		if (kept.Find(key) != nullptr) {
			return kept.IsNode(key);
		}
	}
	return ClassIsNode(ClassNameOf(object));
}

// The object's name and its length, "" and 0 for one that does not read as a
// name (ReadableLength).
const char* NameOf(const UInt8* object, UInt32& length) {
	const char* const name =
		*reinterpret_cast<const char* const*>(object + addr::kNiObjectNameOffset);
	length = ReadableLength(name, 64);
	return length > 0 ? name : "";
}

const char* NameOf(const UInt8* object) {
	UInt32 length = 0;
	return NameOf(object, length);
}

UInt16& FlagsOf(UInt8* object) { return *reinterpret_cast<UInt16*>(object + addr::kNiFlagsOffset); }

// The children of a node: the array pointer and the count the engine keeps
// beside it. Null children are normal - the array has holes.
UInt8* const* ChildrenOf(const UInt8* node, UInt32& count) {
	count = 0;
	UInt8* const* const children =
		*reinterpret_cast<UInt8* const* const*>(node + addr::kNiChildrenOffset);
	if (!LooksLikeObject(children)) {
		return nullptr;
	}
	count = *reinterpret_cast<const UInt16*>(node + addr::kNiChildCountOffset);
	if (count > 512) {
		count = 0;  // not a count
		return nullptr;
	}
	return children;
}

// What was hidden: the nodes, so the bit can be cleared on exactly those,
// and only where this code set it - and for each the worn cuirass's node it
// was under, if any, so the cuirass rule can be asked again.
UInt8* g_hidden[kMaxHidden];
const UInt8* g_hiddenUnder[kMaxHidden];
// The worn item's node each was under when hidden (the nearest; null for
// none): its slot decides for the parts below it (ShapeHiddenByList).
const UInt8* g_hiddenWorn[kMaxHidden];
UInt32 g_hiddenCount = 0;
UInt32 g_hiddenReportsLeft = 60;  // a scabbard hidden late still shows (2026-09-27)
const UInt8* g_probedRoot = nullptr;
UInt32 g_probedSignature = 0;
UInt32 g_probeReportsLeft = 24;
UInt32 g_probeLinesLeft = 0;

bool IsHidden(const UInt8* node) {
	for (UInt32 at = 0; at < g_hiddenCount; ++at) {
		if (g_hidden[at] == node) {
			return true;
		}
	}
	return false;
}

void ForgetHidden(UInt32 index) {
	for (UInt32 at = index + 1; at < g_hiddenCount; ++at) {
		g_hidden[at - 1] = g_hidden[at];
		g_hiddenUnder[at - 1] = g_hiddenUnder[at];
		g_hiddenWorn[at - 1] = g_hiddenWorn[at];
	}
	--g_hiddenCount;
}

// Whether `node` is still somewhere under `root`, so a node the engine has
// since freed is never written to.
bool StillUnder(const UInt8* root, const UInt8* node, UInt32 depth) {
	if (root == node) {
		return true;
	}
	if (depth >= kMaxFindDepth || !ObjectIsNode(root)) {
		return false;
	}
	UInt32 count = 0;
	UInt8* const* const children = ChildrenOf(root, count);
	for (UInt32 at = 0; at < count; ++at) {
		if (LooksLikeObject(children[at]) && StillUnder(children[at], node, depth + 1)) {
			return true;
		}
	}
	return false;
}

// `cuirass` is the worn cuirass's node this one is under, or null; `worn`
// the nearest worn item's node above it, or null.
void HideMatching(UInt8* node, const char* list, const char* cuirassKeep, const UInt8* cuirass, const UInt8* worn,
                  UInt32 depth) {
	if (!LooksLikeObject(node)) {
		return;
	}
	const char* const name = NameOf(node);
	const bool isNode = ObjectIsNode(node);
	if (isNode && cuirassKeep != nullptr && IsWornItemNode(name, "UpperBody")) {
		cuirass = node;
	}
	const bool byCuirass = !isNode && HiddenUnderCuirass(cuirass != nullptr, name, cuirassKeep);
	const char* const wornName = worn != nullptr ? NameOf(worn) : nullptr;
	if ((ShapeHiddenByList(name, wornName, list) || byCuirass) && !IsHidden(node)) {
		UInt16& flags = FlagsOf(node);
		if ((flags & kAppCulledBit) == 0 && g_hiddenCount < kMaxHidden) {
			flags = static_cast<UInt16>(flags | kAppCulledBit);
			g_hiddenUnder[g_hiddenCount] = cuirass;
			g_hiddenWorn[g_hiddenCount] = worn;
			g_hidden[g_hiddenCount++] = node;
			if (g_hiddenReportsLeft > 0) {
				--g_hiddenReportsLeft;
				OBVR_LOG("First person hide: \"%s\" (%s) at %08X is hidden%s, under \"%s\"", name, ClassNameOf(node),
				         reinterpret_cast<UInt32>(node), byCuirass ? " (a shape of the worn cuirass)" : "",
				         wornName != nullptr ? wornName : "no worn item");
			}
		}
	}
	if (depth >= kMaxFindDepth || !isNode) {
		return;
	}
	const UInt8* const below = isNode && WornSlotLength(name) > 0 ? node : worn;
	UInt32 count = 0;
	UInt8* const* const children = ChildrenOf(node, count);
	for (UInt32 at = 0; at < count; ++at) {
		HideMatching(children[at], list, cuirassKeep, cuirass, below, depth + 1);
	}
}

// Whether a node this code hid is still to be hidden: by the list, or as a
// shape still under a worn cuirass's node that is still in the tree.
bool StaysHidden(const UInt8* root, UInt32 index, const char* list, const char* cuirassKeep) {
	const UInt8* const node = g_hidden[index];
	const char* const name = NameOf(node);
	const UInt8* const worn = g_hiddenWorn[index];
	const bool wornStill = worn != nullptr && StillUnder(root, worn, 0) && StillUnder(worn, node, 0);
	if (ShapeHiddenByList(name, wornStill ? NameOf(worn) : nullptr, list)) {
		return true;
	}
	const UInt8* const cuirass = g_hiddenUnder[index];
	const bool underCuirass = cuirass != nullptr && cuirassKeep != nullptr && StillUnder(root, cuirass, 0) &&
	                          IsWornItemNode(NameOf(cuirass), "UpperBody") && StillUnder(cuirass, node, 0);
	return HiddenUnderCuirass(underCuirass, name, cuirassKeep);
}

UInt8* FindNamed(UInt8* node, const ListEntry& wanted, UInt32 depth) {
	if (!LooksLikeObject(node)) {
		return nullptr;
	}
	UInt32 length = 0;
	const char* const name = NameOf(node, length);
	if (NameIsEntry(name, length, wanted)) {
		return node;
	}
	if (depth >= kMaxFindDepth || !ObjectIsNode(node)) {
		return nullptr;
	}
	UInt32 count = 0;
	UInt8* const* const children = ChildrenOf(node, count);
	for (UInt32 at = 0; at < count; ++at) {
		UInt8* const found = FindNamed(children[at], wanted, depth + 1);
		if (found != nullptr) {
			return found;
		}
	}
	return nullptr;
}

// The tree's shape as one number: every object's address in walk order,
// with its depth, folded together - a part added, removed or moved to
// another parent changes it.
UInt32 TreeSignature(const UInt8* node, UInt32 depth, UInt32 signature) {
	if (!LooksLikeObject(node)) {
		return signature;
	}
	signature = signature * 31u + reinterpret_cast<UInt32>(node) + depth;
	if (depth >= kMaxFindDepth || !ObjectIsNode(node)) {
		return signature;
	}
	UInt32 count = 0;
	UInt8* const* const children = ChildrenOf(node, count);
	for (UInt32 at = 0; at < count; ++at) {
		signature = TreeSignature(children[at], depth + 1, signature);
	}
	return signature;
}

void ProbeNode(const UInt8* node, UInt32 depth, const char* label) {
	if (g_probeLinesLeft == 0 || !LooksLikeObject(node)) {
		return;
	}
	--g_probeLinesLeft;
	const char* const className = ClassNameOf(node);
	const bool isNode = ClassIsNode(className);
	UInt32 count = 0;
	if (isNode) {
		ChildrenOf(node, count);
	}
	static const char* const kIndent[] = {"", "  ", "    ", "      ", "        ", "          ",
	                                      "            "};
	const float* const world =
		reinterpret_cast<const float*>(node + addr::kNodeWorldTranslateOffset);
	OBVR_LOG("%s: %2u %s%s \"%s\" flags=%04X children=%u at %08X world %.1f %.1f %.1f", label,
	         depth, kIndent[depth < 6 ? depth : 6], className, NameOf(node),
	         *reinterpret_cast<const UInt16*>(node + addr::kNiFlagsOffset), count,
	         reinterpret_cast<UInt32>(node), static_cast<double>(world[0]),
	         static_cast<double>(world[1]), static_cast<double>(world[2]));
	if (depth >= kMaxFindDepth || !isNode) {
		return;
	}
	UInt8* const* const children = ChildrenOf(node, count);
	for (UInt32 at = 0; at < count; ++at) {
		if (LooksLikeObject(children[at])) {
			ProbeNode(children[at], depth + 1, label);
		}
	}
}

}  // namespace

void ProbeNodeTree(const NiAVObject* root, const char* label, UInt32 maxLines) {
	if (!LooksLikeObject(root) || label == nullptr) {
		return;
	}
	g_probeLinesLeft = maxLines;
	ProbeNode(reinterpret_cast<const UInt8*>(root), 0, label);
}

namespace {

void NearShapes(const UInt8* node, const char* parentName, bool parentShown, UInt32 depth, const NiPoint3& eye,
                float within, const char* label, UInt32& linesLeft, UInt32& found) {
	if (!LooksLikeObject(node) || depth > kMaxFindDepth) {
		return;
	}
	const UInt16 flags = *reinterpret_cast<const UInt16*>(node + addr::kNiFlagsOffset);
	const bool shown = ShownUnder(parentShown, flags);
	if (!shown) {
		return;
	}
	const char* const className = ClassNameOf(node);
	if (ClassIsNode(className)) {
		UInt32 count = 0;
		UInt8* const* const children = ChildrenOf(node, count);
		for (UInt32 at = 0; at < count; ++at) {
			NearShapes(children[at], NameOf(node), true, depth + 1, eye, within, label, linesLeft, found);
		}
		return;
	}
	const float* const bound = reinterpret_cast<const float*>(node + addr::kNiWorldBoundOffset);
	float gap = 0.0f;
	if (!BoundNearEye(NiPoint3{bound[0], bound[1], bound[2]}, bound[3], eye, within, gap)) {
		return;
	}
	++found;
	if (linesLeft == 0) {
		return;
	}
	--linesLeft;
	OBVR_LOG("Menu near shapes: %s - \"%s\" (%s) under \"%s\", its bound %.1f units from the eyes, %.1f round, at "
	         "%.0f %.0f %.0f",
	         label, NameOf(node), className, parentName, static_cast<double>(gap), static_cast<double>(bound[3]),
	         static_cast<double>(bound[0]), static_cast<double>(bound[1]), static_cast<double>(bound[2]));
}

}  // namespace

UInt32 LogShapesNearEye(const NiAVObject* root, const NiPoint3& eye, float withinUnits, const char* label,
                        UInt32 maxLines) {
	UInt32 found = 0;
	if (!LooksLikeObject(root) || label == nullptr || !(withinUnits > 0.0f)) {
		return found;
	}
	UInt32 linesLeft = maxLines;
	NearShapes(reinterpret_cast<const UInt8*>(root), "", true, 0, eye, withinUnits, label, linesLeft, found);
	return found;
}

void ReleaseHiddenFirstPersonNodes() {
	NiAVObject* const root = FirstPersonArmsNode();
	for (UInt32 at = 0; at < g_hiddenCount; ++at) {
		UInt8* const node = g_hidden[at];
		if (root != nullptr && StillUnder(reinterpret_cast<const UInt8*>(root), node, 0)) {
			FlagsOf(node) = static_cast<UInt16>(FlagsOf(node) & ~kAppCulledBit);
		}
	}
	if (g_hiddenCount > 0) {
		OBVR_LOG("First person hide: %u node(s) shown again", g_hiddenCount);
	}
	g_hiddenCount = 0;
}

void HideFirstPersonNodes(bool enabled, const char* list, const char* cuirassKeep) {
	if (!enabled || (ListIsEmpty(list) && cuirassKeep == nullptr)) {
		if (g_hiddenCount > 0) {
			ReleaseHiddenFirstPersonNodes();
		}
		return;
	}
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr) {
		g_hiddenCount = 0;  // no tree to write to; whatever was hidden is gone with it
		return;
	}
	UInt8* const rootBytes = reinterpret_cast<UInt8*>(root);

	// Nodes hidden earlier that are no longer in the list, or no longer in
	// the tree, are let go - the first by clearing the bit, the second by
	// forgetting them without a write.
	for (UInt32 at = 0; at < g_hiddenCount;) {
		UInt8* const node = g_hidden[at];
		if (!StillUnder(rootBytes, node, 0)) {
			ForgetHidden(at);
			continue;
		}
		if (!StaysHidden(rootBytes, at, list, cuirassKeep)) {
			FlagsOf(node) = static_cast<UInt16>(FlagsOf(node) & ~kAppCulledBit);
			ForgetHidden(at);
			continue;
		}
		// Hidden again every pass, not only when first taken. The engine
		// frees a shape and makes a new one at the same address when a mesh
		// is swapped - Vanilla BreakArmor swaps a cuirass's damage stages -
		// and this list then still holds the address: the walk below takes
		// it for one already hidden and leaves it, and the new shape was
		// never culled. Nadi's "Foot:Body" under the cuirass 0001C6D1 hung
		// before the eyes and moved with the right hand (2026-10-28; in the
		// harness the same addresses came back for other shapes, 21907F84
		// "Foot:upperHGEC_C" then "Cuirass"). StaysHidden judged the shape
		// at the address by its name and place now, so it is the right one.
		FlagsOf(node) = static_cast<UInt16>(FlagsOf(node) | kAppCulledBit);
		++at;
	}

	HideMatching(rootBytes, list, cuirassKeep, nullptr, nullptr, 0);
}

namespace {

bool ContainsIgnoringCase(const char* text, const char* part) {
	for (; *text != '\0'; ++text) {
		UInt32 at = 0;
		while (part[at] != '\0' && text[at] != '\0') {
			char a = text[at];
			char b = part[at];
			if (a >= 'A' && a <= 'Z') {
				a = static_cast<char>(a - 'A' + 'a');
			}
			if (b >= 'A' && b <= 'Z') {
				b = static_cast<char>(b - 'A' + 'a');
			}
			if (a != b) {
				break;
			}
			++at;
		}
		if (part[at] == '\0') {
			return true;
		}
	}
	return false;
}

void CollectContaining(UInt8* node, const char* part, NiAVObject** out, UInt32 capacity,
                       UInt32& found, UInt32 depth) {
	if (depth >= kMaxFindDepth || found >= capacity || !ObjectIsNode(node)) {
		return;
	}
	UInt32 count = 0;
	UInt8* const* const children = ChildrenOf(node, count);
	for (UInt32 at = 0; at < count && found < capacity; ++at) {
		UInt8* const child = children[at];
		if (!LooksLikeObject(child)) {
			continue;
		}
		if (ContainsIgnoringCase(NameOf(child), part)) {
			out[found++] = reinterpret_cast<NiAVObject*>(child);
		}
		CollectContaining(child, part, out, capacity, found, depth + 1);
	}
}

}  // namespace

UInt32 CollectNodesContaining(NiAVObject* under, const char* part, NiAVObject** out,
                              UInt32 capacity) {
	if (!LooksLikeObject(under) || part == nullptr || part[0] == '\0' || out == nullptr) {
		return 0;
	}
	UInt32 found = 0;
	CollectContaining(reinterpret_cast<UInt8*>(under), part, out, capacity, found, 0);
	return found;
}

void UpdateChildTransforms(NiAVObject* node) {
	if (!LooksLikeObject(node) || !ObjectIsNode(reinterpret_cast<UInt8*>(node))) {
		return;
	}
	UInt32 count = 0;
	UInt8* const* const children = ChildrenOf(reinterpret_cast<UInt8*>(node), count);
	for (UInt32 at = 0; at < count; ++at) {
		if (LooksLikeObject(children[at])) {
			UpdateNodeTransforms(reinterpret_cast<NiAVObject*>(children[at]));
		}
	}
}

NiAVObject* FindFirstPersonNode(const char* name) {
	if (name == nullptr || name[0] == '\0') {
		return nullptr;
	}
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr) {
		return nullptr;
	}
	return reinterpret_cast<NiAVObject*>(FindNamed(reinterpret_cast<UInt8*>(root), MeasureEntry(name), 0));
}

namespace {

// Sets the world bound of every node named in the list, and of the nodes
// above it up to the root, to a sphere around the camera. Answers how many
// named nodes it found.
// `worn` is the nearest worn item's node above, or null: under it the item's
// slot decides as in hiding (ShapeHiddenByList) - every part of the hand, the
// male hand's "Foot:handRM" too, not only parts named "Hand" (2026-11-11:
// on Windows, with OBNG's occlusion culling, that shape's own bound was
// culled and the male hands showed only for a few frames of a cast).
UInt32 KeepMatchingInView(UInt8* node, const char* list, const float* centre, float radius,
                          UInt8** chain, UInt32 depth, const UInt8* worn = nullptr) {
	if (!LooksLikeObject(node) || depth > kMaxFindDepth) {
		return 0;
	}
	chain[depth] = node;
	UInt32 found = 0;
	const char* const name = NameOf(node);
	if (ShapeHiddenByList(name, worn != nullptr ? NameOf(worn) : nullptr, list)) {
		++found;
		for (UInt32 up = 0; up <= depth; ++up) {
			float* const bound = reinterpret_cast<float*>(chain[up] + addr::kNiWorldBoundOffset);
			bound[0] = centre[0];
			bound[1] = centre[1];
			bound[2] = centre[2];
			if (bound[3] < radius) {
				bound[3] = radius;
			}
		}
	}
	if (!ObjectIsNode(node)) {
		return found;
	}
	const UInt8* const below = WornSlotLength(name) > 0 ? node : worn;
	UInt32 count = 0;
	UInt8* const* const children = ChildrenOf(node, count);
	for (UInt32 at = 0; at < count; ++at) {
		found += KeepMatchingInView(children[at], list, centre, radius, chain, depth + 1, below);
	}
	return found;
}

UInt32 g_keepReportsLeft = 2;

UInt32 KeepSubtreeInView(UInt8* node, const float* centre, float radius, UInt32 depth) {
	if (!LooksLikeObject(node) || depth > kMaxFindDepth) {
		return 0;
	}
	float* const bound = reinterpret_cast<float*>(node + addr::kNiWorldBoundOffset);
	bound[0] = centre[0];
	bound[1] = centre[1];
	bound[2] = centre[2];
	if (!(bound[3] >= radius)) {
		bound[3] = radius;
	}
	UInt32 set = 1;
	if (!ObjectIsNode(node)) {
		return set;
	}
	UInt32 count = 0;
	UInt8* const* const children = ChildrenOf(node, count);
	for (UInt32 at = 0; at < count; ++at) {
		set += KeepSubtreeInView(children[at], centre, radius, depth + 1);
	}
	return set;
}

}  // namespace

UInt32 KeepTreeInView(NiAVObject* root, const NiPoint3& centre, float radius) {
	const float point[3] = {centre.x, centre.y, centre.z};
	return KeepSubtreeInView(reinterpret_cast<UInt8*>(root), point, radius, 0);
}

UInt32 KeepNodesInView(NiAVObject* root, const char* list, const NiPoint3& centre, float radius) {
	if (!LooksLikeObject(root) || list == nullptr || ListIsEmpty(list)) {
		return 0;
	}
	UInt8* chain[kMaxFindDepth + 2]{};
	const float point[3] = {centre.x, centre.y, centre.z};
	return KeepMatchingInView(reinterpret_cast<UInt8*>(root), list, point, radius, chain, 0);
}

NiAVObject* const* NodeChildren(const NiAVObject* node, UInt32& count) {
	count = 0;
	const UInt8* const bytes = reinterpret_cast<const UInt8*>(node);
	if (!LooksLikeObject(bytes) || !ObjectIsNode(bytes)) {
		return nullptr;
	}
	return reinterpret_cast<NiAVObject* const*>(ChildrenOf(bytes, count));
}

namespace {

// The names of one walk, each measured before it (NodeNameList.h
// MeasureEntry); a name past the measured ones is measured where it is asked.
constexpr UInt32 kMeasuredNames = 48;

struct WantedNames {
	const char* const* names;
	UInt32 count;
	ListEntry measured[kMeasuredNames];
};

void FindNamedInto(UInt8* node, const WantedNames& wanted, NiAVObject** out, UInt32& found, UInt32 depth) {
	const UInt32 count = wanted.count;
	if (!LooksLikeObject(node) || found >= count) {
		return;
	}
	UInt32 length = 0;
	const char* const name = NameOf(node, length);
	for (UInt32 i = 0; i < count; ++i) {
		if (out[i] == nullptr &&
		    NameIsEntry(name, length, i < kMeasuredNames ? wanted.measured[i] : MeasureEntry(wanted.names[i]))) {
			out[i] = reinterpret_cast<NiAVObject*>(node);
			++found;
			break;
		}
	}
	if (depth >= kMaxFindDepth || !ObjectIsNode(node)) {
		return;
	}
	UInt32 children = 0;
	UInt8* const* const list = ChildrenOf(node, children);
	for (UInt32 at = 0; at < children && found < count; ++at) {
		FindNamedInto(list[at], wanted, out, found, depth + 1);
	}
}

}  // namespace

UInt32 FindNodesNamed(NiAVObject* under, const char* const* names, UInt32 count, NiAVObject** out) {
	if (out == nullptr || names == nullptr) {
		return 0;
	}
	for (UInt32 i = 0; i < count; ++i) {
		out[i] = nullptr;
	}
	WantedNames wanted;
	wanted.names = names;
	wanted.count = count;
	for (UInt32 i = 0; i < count && i < kMeasuredNames; ++i) {
		wanted.measured[i] = MeasureEntry(names[i]);
	}
	UInt32 found = 0;
	FindNamedInto(reinterpret_cast<UInt8*>(under), wanted, out, found, 0);
	return found;
}

UInt32 KeepFirstPersonNodesInView(const char* list, const NiPoint3& centre, float radius) {
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr || list == nullptr || ListIsEmpty(list)) {
		return 0;
	}
	UInt8* chain[kMaxFindDepth + 2]{};
	const float point[3] = {centre.x, centre.y, centre.z};
	const UInt32 found =
		KeepMatchingInView(reinterpret_cast<UInt8*>(root), list, point, radius, chain, 0);
	if (g_keepReportsLeft > 0) {
		--g_keepReportsLeft;
		OBVR_LOG("First person hide: %u node(s) named \"%s\" kept in view - their bound centred "
		         "on the camera, %.0f units wide", found, list, static_cast<double>(radius));
	}
	return found;
}

void ProbeFirstPersonTree() {
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr) {
		return;
	}
	// Again whenever the tree changes shape - a new model, but also armour
	// put on or a weapon sheathed, whose parts arrive as new nodes or move to
	// another bone. Bounded: each report is long.
	const UInt32 signature = TreeSignature(reinterpret_cast<const UInt8*>(root), 0, 17u);
	if (reinterpret_cast<const UInt8*>(root) == g_probedRoot && signature == g_probedSignature) {
		return;
	}
	g_probedRoot = reinterpret_cast<const UInt8*>(root);
	g_probedSignature = signature;
	if (g_probeReportsLeft == 0) {
		return;
	}
	--g_probeReportsLeft;
	g_probeLinesLeft = 400;
	OBVR_LOG("First person tree: root %08X, shape %08X - class, name, flags (bit 0 hidden), "
	         "children, world position", reinterpret_cast<UInt32>(root), signature);
	ProbeNode(g_probedRoot, 0, "First person tree");
}

const char* NiClassNameOf(const void* object) {
	return ClassNameOf(reinterpret_cast<const UInt8*>(object));
}

bool NiClassIsNode(const char* className) { return ClassIsNode(className); }

}  // namespace obvr::game
