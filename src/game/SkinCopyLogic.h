#pragma once

// A copied shape's own skin instance (game/DismemberCaps.cpp AddPartCopy; a
// limb's own skin, [Hands] DismemberLimbSkin). Pure, covered by dismember_test.
//
// The engine's clone of a skinned shape alone does not clone its skin:
// NiSkinInstance's clone 0x0072BFA0 first asks 0x0072BCA0 whether the root
// parent or any bone is in the cloning's map, and when none is it returns
// `this` (0x0072BFD5: mov eax,ebp) - the copy draws from the original's
// instance and bone list, and writing its bones would write the original's.
// Read in the EXE's bytes 2026-11-16 after the first run in the game refused
// every copy ("the clone has no bone list of its own", harness
// dismember-limb-skin-nostage 20261116-091701). A model cloned whole (the
// wound caps' neck plug with its own bone nodes) gets an instance of its own.
//
// So the copy is given one, built as 0x0072BFDC to 0x0072C0B4 builds it: 0x2C
// bytes from the game's allocator, NiObject's constructor, NiSkinInstance's
// vtable, the skin data and the partition shared (each counted once more), the
// root parent, a bone list of its own from the same allocator, 0xFFFFFFFF at
// +0x18 and nothing in the rest.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kNiSkinInstanceVtable = 0x00A684CC;  // set at 0x0072BFFD
inline constexpr UInt32 kSkinCopyMostBones = 128;

// A NiSkinInstance as the engine lays it out.
struct SkinInstanceImage {
	UInt32 vtable;      // +0x00
	UInt32 refCount;    // +0x04, NiObject's constructor leaves 0
	UInt32 data;        // +0x08 NiSkinData, counted
	UInt32 partition;   // +0x0C NiSkinPartition, counted; may be 0
	UInt32 rootParent;  // +0x10, a plain pointer
	UInt32 bones;       // +0x14 the bone list, one pointer a bone of the data
	UInt32 frame;       // +0x18, 0xFFFFFFFF in a new one
	UInt32 rest[4];     // +0x1C to +0x28, 0 in a new one
};
static_assert(sizeof(SkinInstanceImage) == 0x2C, "0x0072BFDC allocates 0x2C bytes");

// What to do about the skin of a shape's clone.
//   Own     the clone came with an instance and a bone list of its own
//   Build   it shares the original's instance, or has none: one is built
//   Refuse  nothing known to be safe to write
enum class CopySkin : UInt8 { Own, Build, Refuse };

inline CopySkin CopySkinFor(UInt32 copySkin, UInt32 originalSkin, UInt32 copyBones, UInt32 originalBones,
                            UInt32 boneCount) {
	if (originalSkin == 0 || originalBones == 0 || boneCount == 0 || boneCount > kSkinCopyMostBones) {
		return CopySkin::Refuse;
	}
	if (copySkin == 0 || copySkin == originalSkin) {
		return CopySkin::Build;
	}
	// Another instance: only with a list that is its own.
	return copyBones != 0 && copyBones != originalBones ? CopySkin::Own : CopySkin::Refuse;
}

// Only a plain NiSkinInstance is rebuilt: a class derived from it would be
// larger than the 0x2C bytes read.
inline bool SkinIsPlainInstance(UInt32 vtable) { return vtable == kNiSkinInstanceVtable; }

// The fields of a new instance after NiObject's constructor ran on it: all
// but the reference count, which is the smart pointer's that takes it.
inline void FillSkinInstance(SkinInstanceImage& image, UInt32 data, UInt32 partition, UInt32 rootParent,
                             UInt32 bones) {
	image.vtable = kNiSkinInstanceVtable;
	image.data = data;
	image.partition = partition;
	image.rootParent = rootParent;
	image.bones = bones;
	image.frame = 0xFFFFFFFFu;
	for (UInt32& word : image.rest) {
		word = 0;
	}
}

}  // namespace obvr::game
