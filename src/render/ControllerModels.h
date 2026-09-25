#pragma once

#include "core/Types.h"
#include "render/EyeGeometry.h"

namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::render {

// The real controllers, drawn into the eye pictures - SteamVR's own render
// model of each, where the controller is - for adjusting the hands: the
// in-game hand is lined up with the controller the player can see.
//
// SteamVR no longer draws models for an application (SetOverlayRenderModel
// has been disabled since SteamVR 1.10: ValveSoftware/openvr issue #1309,
// "the implementation for this function isn't coming back ... render the 3D
// stuff on your side"), so they are drawn here: the model is loaded through
// IVRRenderModels, every vertex projected on the CPU (ControllerProjection.h)
// and drawn pre-transformed into each eye's texture before the submit, with
// a depth buffer of its own so the model hides its own back.
class ControllerModels {
public:
	ControllerModels() = default;
	ControllerModels(const ControllerModels&) = delete;
	ControllerModels& operator=(const ControllerModels&) = delete;
	~ControllerModels();

	// Draws both controllers into both eye surfaces (IDirect3DSurface9, the
	// eye textures' level 0), each width x height, whose frustums are given.
	// Loads the models on the first frames it is asked, which takes a few:
	// nothing is drawn until they are there. Leaves the device as it found it.
	void Draw(const vr::OpenVRBackend& backend, void* device, void* const eyeSurfaces[2],
	          UInt32 width, UInt32 height, const EyeProjection eyes[2]);

	// Frees the models and the depth surface.
	void Release(const vr::OpenVRBackend* backend);

private:
	struct Model {
		char name[128] = {};
		void* loaded = nullptr;  // vr::openvr::RenderModel*
		UInt32* colours = nullptr;
		bool failed = false;
	};
	bool EnsureModel(const vr::OpenVRBackend& backend, bool rightHand);
	bool EnsureDepth(void* device, UInt32 width, UInt32 height);
	bool EnsureScratch(UInt32 vertices, UInt32 indices);

	Model m_models[2];
	void* m_depth = nullptr;
	UInt32 m_depthWidth = 0;
	UInt32 m_depthHeight = 0;
	bool m_depthFailed = false;
	void* m_vertices = nullptr;  // pre-transformed, coloured vertices
	UInt16* m_indices = nullptr;
	UInt32 m_vertexCapacity = 0;
	UInt32 m_indexCapacity = 0;
	bool m_reported = false;
};

}  // namespace obvr::render
