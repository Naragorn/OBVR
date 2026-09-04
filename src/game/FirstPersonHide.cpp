#include "game/FirstPersonHide.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/FirstPersonArms.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/NodeNameList.h"

namespace obvr::game {
namespace {

constexpr UInt16 kAppCulledBit = 0x0001;
constexpr UInt32 kMaxDepth = 6;
constexpr UInt32 kMaxHidden = 32;

bool LooksLikeObject(const void* pointer) {
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer));
}

// A short run of printable characters, the way a node's name or a type's
// mangled name reads. Anything else is not a string this code should trust.
bool ReadableText(const char* text, UInt32 limit) {
	if (!LooksLikeObject(text)) {
		return false;
	}
	for (UInt32 at = 0; at < limit; ++at) {
		const char c = text[at];
		if (c == '\0') {
			return at > 0;
		}
		if (c < 0x20 || c > 0x7E) {
			return false;
		}
	}
	return false;
}

// The class an object was compiled as, from the compiler's own type
// records: the word before an MSVC vtable is the complete object locator,
// whose fourth word is the type descriptor, whose name sits eight bytes in
// and reads ".?AVNiNode@@". Every step is a read behind a plausibility
// check; a miss anywhere answers "?" rather than a guess. Oblivion.exe
// carries these records - the player's vtable is identified by them
// elsewhere in this project (see kAttackUpdate's comment).
const char* ClassNameOf(const UInt8* object) {
	if (!LooksLikeObject(object)) {
		return "?";
	}
	const UInt8* const vtable = *reinterpret_cast<const UInt8* const*>(object);
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

// Whether a class name (still carrying its "@@" tail) is a node - one that
// keeps children. Every NetImmerse node class ends in "Node": NiNode,
// BSFadeNode, BSFaceGenNiNode, NiBillboardNode, NiSwitchNode; geometry -
// NiTriShape, NiTriStrips - does not. Reading a children array off a
// geometry would read its own fields as pointers, so the question is asked
// first.
bool ClassIsNode(const char* className) {
	UInt32 length = 0;
	while (className[length] != '\0' && length < 96) {
		++length;
	}
	// Strip the "@@" tail.
	while (length > 0 && className[length - 1] == '@') {
		--length;
	}
	if (length < 4) {
		return false;
	}
	return className[length - 4] == 'N' && className[length - 3] == 'o' &&
	       className[length - 2] == 'd' && className[length - 1] == 'e';
}

const char* NameOf(const UInt8* object) {
	const char* const name =
		*reinterpret_cast<const char* const*>(object + addr::kNiObjectNameOffset);
	return ReadableText(name, 64) ? name : "";
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
// and only where this code set it.
UInt8* g_hidden[kMaxHidden];
UInt32 g_hiddenCount = 0;
UInt32 g_hiddenReportsLeft = 12;
const UInt8* g_probedRoot = nullptr;
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
	}
	--g_hiddenCount;
}

// Whether `node` is still somewhere under `root`, so a node the engine has
// since freed is never written to.
bool StillUnder(const UInt8* root, const UInt8* node, UInt32 depth) {
	if (root == node) {
		return true;
	}
	if (depth >= kMaxDepth || !ClassIsNode(ClassNameOf(root))) {
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

void HideMatching(UInt8* node, const char* list, UInt32 depth) {
	if (!LooksLikeObject(node)) {
		return;
	}
	const char* const name = NameOf(node);
	if (NameInList(name, list) && !IsHidden(node)) {
		UInt16& flags = FlagsOf(node);
		if ((flags & kAppCulledBit) == 0 && g_hiddenCount < kMaxHidden) {
			flags = static_cast<UInt16>(flags | kAppCulledBit);
			g_hidden[g_hiddenCount++] = node;
			if (g_hiddenReportsLeft > 0) {
				--g_hiddenReportsLeft;
				OBVR_LOG("First person hide: \"%s\" (%s) at %08X is hidden", name,
				         ClassNameOf(node), reinterpret_cast<UInt32>(node));
			}
		}
	}
	if (depth >= kMaxDepth || !ClassIsNode(ClassNameOf(node))) {
		return;
	}
	UInt32 count = 0;
	UInt8* const* const children = ChildrenOf(node, count);
	for (UInt32 at = 0; at < count; ++at) {
		HideMatching(children[at], list, depth + 1);
	}
}

UInt8* FindNamed(UInt8* node, const char* name, UInt32 depth) {
	if (!LooksLikeObject(node)) {
		return nullptr;
	}
	if (NameInList(NameOf(node), name)) {
		return node;
	}
	if (depth >= kMaxDepth || !ClassIsNode(ClassNameOf(node))) {
		return nullptr;
	}
	UInt32 count = 0;
	UInt8* const* const children = ChildrenOf(node, count);
	for (UInt32 at = 0; at < count; ++at) {
		UInt8* const found = FindNamed(children[at], name, depth + 1);
		if (found != nullptr) {
			return found;
		}
	}
	return nullptr;
}

void ProbeNode(const UInt8* node, UInt32 depth) {
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
	OBVR_LOG("First person tree: %s%s \"%s\" flags=%04X children=%u at %08X",
	         kIndent[depth < 6 ? depth : 6], className, NameOf(node),
	         *reinterpret_cast<const UInt16*>(node + addr::kNiFlagsOffset), count,
	         reinterpret_cast<UInt32>(node));
	if (depth >= kMaxDepth || !isNode) {
		return;
	}
	UInt8* const* const children = ChildrenOf(node, count);
	for (UInt32 at = 0; at < count; ++at) {
		if (LooksLikeObject(children[at])) {
			ProbeNode(children[at], depth + 1);
		}
	}
}

}  // namespace

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

void HideFirstPersonNodes(bool enabled, const char* list) {
	if (!enabled || ListIsEmpty(list)) {
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
		if (!NameInList(NameOf(node), list)) {
			FlagsOf(node) = static_cast<UInt16>(FlagsOf(node) & ~kAppCulledBit);
			ForgetHidden(at);
			continue;
		}
		++at;
	}

	HideMatching(rootBytes, list, 0);
}

NiAVObject* FindFirstPersonNode(const char* name) {
	if (name == nullptr || name[0] == '\0') {
		return nullptr;
	}
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr) {
		return nullptr;
	}
	return reinterpret_cast<NiAVObject*>(FindNamed(reinterpret_cast<UInt8*>(root), name, 0));
}

void ProbeFirstPersonTree() {
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr || reinterpret_cast<const UInt8*>(root) == g_probedRoot) {
		return;
	}
	g_probedRoot = reinterpret_cast<const UInt8*>(root);
	g_probeLinesLeft = 200;
	OBVR_LOG("First person tree: root %08X - class, name, flags (bit 0 hidden), children",
	         reinterpret_cast<UInt32>(root));
	ProbeNode(g_probedRoot, 0);
}

}  // namespace obvr::game
