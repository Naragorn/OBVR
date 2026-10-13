#include "game/EnemyHealthModel.h"

#include <cstdio>
#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/EnemyHealthLogic.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"
#include "render/GameDevice.h"
#include "ui/DxtDecode.h"

namespace obvr::game {
namespace {

// The classes by their vtables (Oblivion.exe 1.2.0.416, from the RTTI
// locators, read 2026-10-13).
constexpr UInt32 kNiTexturingPropertyVtable = 0x00A7DB5C;
constexpr UInt32 kNiFlipControllerVtable = 0x00A7A33C;
constexpr UInt32 kNiSourceTextureVtable = 0x00A7D8CC;
// NiAVObject's property list (NiTListBase at +0x98: the head at +0x9C; a
// node is {next, prev, data}); NiObjectNET's controller at +0x0C.
constexpr UInt32 kPropertyListHeadOffset = 0x9C;
constexpr UInt32 kListNodeNextOffset = 0x00;
constexpr UInt32 kListNodeDataOffset = 0x08;
constexpr UInt32 kControllerOffset = 0x0C;
// NiFlipController: the textures' NiTArray at +0x40 (its data at +0x44, the
// used count at +0x4A) - NiFlipController::Update 0x006D1960, read
// 2026-10-13.
constexpr UInt32 kFlipTexturesOffset = 0x44;
constexpr UInt32 kFlipTextureCountOffset = 0x4A;
// NiTexture: its renderer data at +0x24 (the shader property's set-up asks
// it for its mipmap count there, 0x007DA8A9) and, a NiSourceTexture, its
// file name at +0x34 ("textures\menus\misc\sequence_dds\000001.dds"); the
// Direct3D texture is one of the renderer data's first dwords (+0x50 in the
// probe of 2026-10-13, the format's four characters at +0x18, the width and
// the height at +0x54 and +0x58), told by its vtable - the one every
// IDirect3DTexture9 of the game's device shares, read off a texture OBVR
// makes itself.
constexpr UInt32 kTextureRendererDataOffset = 0x24;
constexpr UInt32 kTextureFileNameOffset = 0x34;
constexpr UInt32 kRendererDataScanFrom = 0x04;
constexpr UInt32 kRendererDataScanTo = 0x60;
constexpr UInt32 kFramesMost = 64;
constexpr UInt32 kFrameSideMost = 1024;
constexpr UInt32 kWalkMost = 64;
constexpr UInt32 kWalkDepthMost = 8;
constexpr UInt32 kPropertiesMost = 12;
constexpr UInt32 kPoolSystemMem = 2;

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
bool LooksLikeObject(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }

template <typename T>
T& At(void* object, UInt32 offset) {
	return *reinterpret_cast<T*>(reinterpret_cast<UInt8*>(object) + offset);
}

UInt32 g_lines = 16;
EnemyHealthFramesInfo g_info;
UInt32 g_textureVtable = 0;  // IDirect3DTexture9's, once read
bool g_textureVtableTried = false;

struct Frame {
	render::Pixel* pixels = nullptr;
	UInt32 width = 0;
	UInt32 height = 0;
	UInt32 texture = 0;  // the game's NiSourceTexture it was read off
	bool tried = false;
	const char* why = "";
};
Frame g_frames[kFramesMost];

// The first property of this class on the object, walking its list.
UInt32 PropertyOf(UInt32 object, UInt32 vtable) {
	UInt32 node = Read(object + kPropertyListHeadOffset);
	for (UInt32 i = 0; i < kPropertiesMost && LooksLikeObject(node); ++i) {
		const UInt32 data = Read(node + kListNodeDataOffset);
		if (LooksLikeObject(data) && Read(data) == vtable) {
			return data;
		}
		node = Read(node + kListNodeNextOffset);
	}
	return 0;
}

// The texturing property with a flip controller, on this object; 0 when
// it has none.
UInt32 FlippingTexturingOf(UInt32 object, UInt32* flipOut) {
	const UInt32 texturing = PropertyOf(object, kNiTexturingPropertyVtable);
	if (texturing == 0) {
		return 0;
	}
	const UInt32 flip = Read(texturing + kControllerOffset);
	if (!LooksLikeObject(flip) || Read(flip) != kNiFlipControllerVtable) {
		return 0;
	}
	*flipOut = flip;
	return texturing;
}

struct Search {
	UInt32 walked = 0;
	UInt32 found = 0;
	UInt32 flip = 0;
};

void FindFlipping(NiAVObject* object, UInt32 depth, Search& s) {
	if (s.found != 0 || !LooksLikeObject(object) || s.walked >= kWalkMost || depth > kWalkDepthMost) {
		return;
	}
	++s.walked;
	UInt32 flip = 0;
	if (FlippingTexturingOf(reinterpret_cast<UInt32>(object), &flip) != 0) {
		s.found = reinterpret_cast<UInt32>(object);
		s.flip = flip;
		return;
	}
	UInt32 count = 0;
	NiAVObject* const* const children = NodeChildren(object, count);
	for (UInt32 i = 0; children != nullptr && i < count && s.found == 0; ++i) {
		if (children[i] != nullptr) {
			FindFlipping(children[i], depth + 1, s);
		}
	}
}

UInt32 TextureCountOf(UInt32 flip) {
	const UInt32 textures = Read(flip + kFlipTexturesOffset);
	if (!LooksLikeObject(textures)) {
		return 0;
	}
	return At<UInt16>(reinterpret_cast<void*>(flip), kFlipTextureCountOffset);
}

UInt32 TextureAt(UInt32 flip, UInt32 index) {
	const UInt32 textures = Read(flip + kFlipTexturesOffset);
	if (!LooksLikeObject(textures) || index >= TextureCountOf(flip)) {
		return 0;
	}
	const UInt32 texture = Read(textures + index * 4);
	return LooksLikeObject(texture) && Read(texture) == kNiSourceTextureVtable ? texture : 0;
}

void ReleaseCom(void* object) {
	if (object != nullptr) {
		auto* const unknown = static_cast<render::d3d11::Unknown*>(object);
		if (unknown->vtbl != nullptr && unknown->vtbl->Release != nullptr) {
			unknown->vtbl->Release(unknown);
		}
	}
}

// IDirect3DTexture9's vtable on this device: that of a 1 by 1 texture made
// and let go for it.
UInt32 TextureVtable() {
	if (g_textureVtableTried) {
		return g_textureVtable;
	}
	g_textureVtableTried = true;
	void* const device = render::GetGameDevice();
	if (device == nullptr) {
		return 0;
	}
	auto createTexture = render::d3d9::Method<render::d3d9::CreateTextureFn>(device, render::d3d9::kDeviceCreateTexture);
	void* texture = nullptr;
	if (createTexture == nullptr ||
	    render::d3d11::Failed(createTexture(device, 1, 1, 1, 0, render::d3d9::kFormatA8R8G8B8, kPoolSystemMem, &texture, nullptr)) ||
	    texture == nullptr) {
		return 0;
	}
	g_textureVtable = Read(reinterpret_cast<UInt32>(texture));
	ReleaseCom(texture);
	return g_textureVtable;
}

// The Direct3D texture behind the game's NiSourceTexture: the dword of its
// renderer data that points at an object with IDirect3DTexture9's vtable.
UInt32 D3DTextureOf(UInt32 texture, const char** why) {
	const UInt32 vtable = TextureVtable();
	if (vtable == 0) {
		*why = "IDirect3DTexture9's vtable could not be read (no texture to be made)";
		return 0;
	}
	const UInt32 data = Read(texture + kTextureRendererDataOffset);
	if (!LooksLikeObject(data)) {
		*why = "the texture has no renderer data";
		return 0;
	}
	for (UInt32 offset = kRendererDataScanFrom; offset <= kRendererDataScanTo; offset += 4) {
		const UInt32 p = Read(data + offset);
		if (LooksLikeObject(p) && Read(p) == vtable) {
			return p;
		}
	}
	// The probe, once: the texture's and its renderer data's first dwords,
	// each pointer's own first dword beside it, against the vtable wanted.
	static bool s_probed = false;
	if (!s_probed && g_lines > 0) {
		s_probed = true;
		--g_lines;
		char line[640];
		UInt32 at = 0;
		at += static_cast<UInt32>(std::snprintf(line + at, sizeof(line) - at, "texture %08X:", texture));
		for (UInt32 offset = 0; offset <= 0x3C && at < sizeof(line) - 40; offset += 4) {
			const UInt32 v = Read(texture + offset);
			at += static_cast<UInt32>(std::snprintf(line + at, sizeof(line) - at, " +%02X=%08X%s%08X", offset, v,
			                                        LooksLikeObject(v) ? ">" : "", LooksLikeObject(v) ? Read(v) : 0u));
		}
		OBVR_LOG("Enemy health: the probe (IDirect3DTexture9's vtable %08X) - %s", vtable, line);
		at = 0;
		at += static_cast<UInt32>(std::snprintf(line + at, sizeof(line) - at, "renderer data %08X:", data));
		for (UInt32 offset = 0; offset <= 0x5C && at < sizeof(line) - 40; offset += 4) {
			const UInt32 v = Read(data + offset);
			at += static_cast<UInt32>(std::snprintf(line + at, sizeof(line) - at, " +%02X=%08X%s%08X", offset, v,
			                                        LooksLikeObject(v) ? ">" : "", LooksLikeObject(v) ? Read(v) : 0u));
		}
		OBVR_LOG("Enemy health: the probe - %s", line);
	}
	*why = "no Direct3D texture among the renderer data's first dwords";
	return 0;
}

// The frame read off the game's texture: its level 0 locked read-only, the
// DXT5 blocks decoded (or an A8R8G8B8 picture copied).
bool ReadFrame(Frame& f, UInt32 texture) {
	const char* why = "";
	const UInt32 d3d = D3DTextureOf(texture, &why);
	if (d3d == 0) {
		f.why = why;
		return false;
	}
	void* const d3dTexture = reinterpret_cast<void*>(d3d);
	auto getSurfaceLevel = render::d3d9::Method<render::d3d9::GetSurfaceLevelFn>(d3dTexture, render::d3d9::kTextureGetSurfaceLevel);
	void* surface = nullptr;
	if (getSurfaceLevel == nullptr || render::d3d11::Failed(getSurfaceLevel(d3dTexture, 0, &surface)) || surface == nullptr) {
		f.why = "the texture's first level has no surface";
		return false;
	}
	auto getDesc = render::d3d9::Method<render::d3d9::GetDescFn>(surface, render::d3d9::kSurfaceGetDesc);
	auto lockRect = render::d3d9::Method<render::d3d9::LockRectFn>(surface, render::d3d9::kSurfaceLockRect);
	auto unlockRect = render::d3d9::Method<render::d3d9::UnlockRectFn>(surface, render::d3d9::kSurfaceUnlockRect);
	render::d3d9::SurfaceDesc desc{};
	bool ok = false;
	if (getDesc == nullptr || lockRect == nullptr || unlockRect == nullptr || render::d3d11::Failed(getDesc(surface, &desc))) {
		f.why = "the surface's description could not be read";
	} else if (desc.width == 0 || desc.height == 0 || desc.width > kFrameSideMost || desc.height > kFrameSideMost) {
		f.why = "the texture's size is not one OBVR reads";
	} else if (desc.format != ui::kFourCcDxt5 && desc.format != ui::kFormatA8R8G8B8) {
		f.why = "the texture's format is neither DXT5 nor A8R8G8B8";
	} else {
		render::d3d9::LockedRect locked{};
		if (render::d3d11::Failed(lockRect(surface, &locked, nullptr, render::d3d9::kLockReadOnly)) || locked.bits == nullptr) {
			f.why = "the texture cannot be locked (not in a lockable pool)";
		} else {
			render::Pixel* const pixels = new render::Pixel[desc.width * desc.height];
			const UInt8* const bits = static_cast<const UInt8*>(locked.bits);
			const UInt32 pitch = static_cast<UInt32>(locked.pitch);
			ok = desc.format == ui::kFourCcDxt5 ? ui::DecodeDxt5(bits, desc.width, desc.height, pitch, pixels)
			                                    : ui::CopyA8R8G8B8(bits, desc.width, desc.height, pitch, pixels);
			unlockRect(surface);
			if (ok) {
				f.pixels = pixels;
				f.width = desc.width;
				f.height = desc.height;
				f.texture = texture;
			} else {
				delete[] pixels;
				f.why = "the picture could not be decoded";
			}
		}
	}
	ReleaseCom(surface);
	return ok;
}

void ForgetFrames() {
	for (Frame& f : g_frames) {
		delete[] f.pixels;
		f = Frame{};
	}
}

}  // namespace

bool EnemyHealthFramesAvailable(EnemyHealthFramesInfo* info, const char** why) {
	*why = "";
	// Still the geometry found: its tile, its node and its flip controller
	// unchanged.
	if (g_info.source != 0 && Read(addr::kHudAuxRootPointer) == g_info.tile && LooksLikeObject(g_info.tile) &&
	    Read(g_info.tile + addr::kTileRenderNodeOffset) == g_info.tileNode && LooksLikeObject(g_info.source)) {
		UInt32 flip = 0;
		if (FlippingTexturingOf(g_info.source, &flip) != 0 && flip == g_info.flip &&
		    TextureCountOf(flip) == g_info.frames) {
			if (info != nullptr) {
				*info = g_info;
			}
			return true;
		}
	}
	if (g_info.source != 0) {
		// The HUD built anew: its textures may be new too.
		ForgetFrames();
	}
	g_info = EnemyHealthFramesInfo{};
	const UInt32 tile = Read(addr::kHudAuxRootPointer);
	if (!LooksLikeObject(tile)) {
		*why = "HUDReticle's enemy health tile is not there";
		return false;
	}
	const UInt32 node = Read(tile + addr::kTileRenderNodeOffset);
	if (!LooksLikeObject(node)) {
		*why = "the enemy health tile has no render node";
		return false;
	}
	Search s;
	FindFlipping(reinterpret_cast<NiAVObject*>(node), 0, s);
	if (s.found == 0) {
		*why = "no geometry with a flip controller under the enemy health tile's node";
		if (g_lines > 0) {
			--g_lines;
			OBVR_LOG("Enemy health: the game's own bar model was not found - %s (%u objects walked from node %08X, "
			         "class %s)",
			         *why, s.walked, node, NiClassNameOf(reinterpret_cast<const void*>(node)));
		}
		return false;
	}
	const UInt32 frames = TextureCountOf(s.flip);
	if (frames < 2 || frames > kFramesMost) {
		*why = "the flip controller's texture count is not one OBVR takes";
		return false;
	}
	g_info.tile = tile;
	g_info.tileNode = node;
	g_info.source = s.found;
	g_info.flip = s.flip;
	g_info.frames = frames;
	g_info.walked = s.walked;
	if (g_lines > 0) {
		--g_lines;
		const NiAVObject* const geometry = reinterpret_cast<const NiAVObject*>(s.found);
		OBVR_LOG("Enemy health: the game's own bar model found - geometry %08X \"%s\" (%s) under the enemy health "
		         "tile's node %08X, %u objects walked; its flip controller %08X carries %u textures, the frames",
		         s.found, LooksLikeObject(geometry->name) ? geometry->name : "?",
		         NiClassNameOf(reinterpret_cast<const void*>(s.found)), node, s.walked, s.flip, frames);
	}
	if (info != nullptr) {
		*info = g_info;
	}
	return true;
}

const render::Pixel* EnemyHealthFrame(UInt32 index, UInt32* width, UInt32* height, const char** why) {
	*why = "";
	*width = 0;
	*height = 0;
	if (!EnemyHealthFramesAvailable(nullptr, why)) {
		return nullptr;
	}
	if (index >= g_info.frames || index >= kFramesMost) {
		*why = "no such frame";
		return nullptr;
	}
	const UInt32 texture = TextureAt(g_info.flip, index);
	if (texture == 0) {
		*why = "the frame's texture is no NiSourceTexture";
		return nullptr;
	}
	Frame& f = g_frames[index];
	if (f.pixels != nullptr && f.texture != texture) {
		// A new texture in the slot: read anew.
		delete[] f.pixels;
		f = Frame{};
	}
	if (f.pixels == nullptr && !f.tried) {
		f.tried = true;
		const bool read = ReadFrame(f, texture);
		if (g_lines > 0 && (!read || index == 0 || index + 1 == g_info.frames)) {
			--g_lines;
			if (read) {
				// How wide the picture's opaque part is, for the log.
				UInt32 first = f.width;
				UInt32 last = 0;
				for (UInt32 y = 0; y < f.height; ++y) {
					for (UInt32 x = 0; x < f.width; ++x) {
						if (f.pixels[y * f.width + x].a > 100) {
							first = x < first ? x : first;
							last = x > last ? x : last;
						}
					}
				}
				const char* const name = reinterpret_cast<const char*>(Read(texture + kTextureFileNameOffset));
				OBVR_LOG("Enemy health: frame %u of %u read off the game's texture %08X (\"%s\") - %ux%u pixels, the "
				         "picture opaque from column %u to %u",
				         index + 1, g_info.frames, texture, LooksLikeObject(name) ? name : "?", f.width, f.height,
				         first, last);
			} else {
				OBVR_LOG("Enemy health: frame %u of %u could not be read off the game's texture %08X - %s; OBVR's own "
				         "bar instead",
				         index + 1, g_info.frames, texture, f.why);
			}
		}
	}
	if (f.pixels == nullptr) {
		*why = f.why[0] != '\0' ? f.why : "the frame was not read";
		return nullptr;
	}
	*width = f.width;
	*height = f.height;
	return f.pixels;
}

}  // namespace obvr::game
