#pragma once

#include "core/Types.h"
#include "render/D3D11Types.h"
#include "render/TestPattern.h"

namespace obvr::render {

// A Direct3D 11 device of OBVR's own, and one texture per eye on it.
//
// Deliberately nothing to do with Oblivion's rendering. Oblivion draws in
// Direct3D 9, and OpenVR's Submit takes no Direct3D 9 texture - there is no
// entry for one in ETextureType at all. Bridging that gap is 0.1.0's problem
// and a large one; see HANDOFF.md section 13.
//
// This class exists so that 0.0.5 does not have to wait for it. It makes a
// picture out of nothing and hands it to the compositor, which separates two
// failures that would otherwise arrive together and look identical: "the
// compositor is not being talked to correctly" and "the pixels are not coming
// out of Direct3D 9 correctly". Getting a generated picture into the headset
// settles the first one on its own, and everything it proves - the frame
// timing, the projection, the texture bounds, the eye order - stays proven
// when the pixels later come from somewhere else.
//
// The device is created with no window and no swapchain. OBVR never presents
// anything itself; the compositor owns the headset's display and OBVR only
// hands it textures.
class EyeTextures {
public:
	// Loads d3d11.dll, creates a device and one texture per eye, each filled
	// with the test pattern.
	//
	// The size should come from the headset - see
	// OpenVRBackend::GetRecommendedRenderTargetSize - rather than be chosen,
	// or the compositor rescales every frame for as long as the mod runs.
	//
	// Returns false on any failure, having cleaned up whatever it got as far
	// as. Failure here must stay survivable: it costs the picture, not the
	// head tracking.
	bool Create(UInt32 width, UInt32 height);

	void Destroy();

	bool IsReady() const { return m_textures[0] != nullptr && m_textures[1] != nullptr; }

	// The texture for one eye, as the void* that OpenVR's Texture_t wants in
	// its handle field alongside kTextureTypeDirectX.
	void* GetTexture(Eye eye) const {
		return m_textures[eye == Eye::Left ? 0 : 1];
	}

	UInt32 GetWidth() const { return m_width; }
	UInt32 GetHeight() const { return m_height; }

private:
	bool CreateDevice();
	bool CreateEyeTexture(Eye eye, d3d11::Texture2D*& out);

	void* m_module = nullptr;  // d3d11.dll
	d3d11::Device* m_device = nullptr;
	void* m_context = nullptr;  // ID3D11DeviceContext, only held so it can be released
	d3d11::Texture2D* m_textures[2] = {nullptr, nullptr};

	UInt32 m_width = 0;
	UInt32 m_height = 0;
};

}  // namespace obvr::render
