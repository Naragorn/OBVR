#include "render/ControllerModels.h"

#include <new>

#include "core/Log.h"
#include "core/Rotation.h"
#include "render/ControllerProjection.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {
namespace {

struct ModelVertex {
	float x, y, z, rhw;
	UInt32 colour;
};
static_assert(sizeof(ModelVertex) == 20, "XYZRHW|DIFFUSE is a 20-byte vertex");

// The depth range the model is tested in: a controller is never nearer the
// eye than a few centimetres nor further than an arm.
constexpr float kNearMetres = 0.03f;
constexpr float kFarMetres = 4.0f;

// Sixteen-bit indices address this many vertices; a model with more is
// refused rather than drawn wrong.
constexpr UInt32 kMaxModelVertices = 65535;

}  // namespace

ControllerModels::~ControllerModels() {
	// CPU memory only. The model belongs to SteamVR and the depth surface to
	// the game's device; neither is touched after the process has started to
	// come down.
	for (Model& model : m_models) {
		delete[] model.colours;
		model.colours = nullptr;
	}
	delete[] static_cast<ModelVertex*>(m_vertices);
	delete[] m_indices;
}

void ControllerModels::Release(const vr::OpenVRBackend* backend) {
	vr::openvr::IVRRenderModelsFnTable* models = backend != nullptr ? backend->RenderModels() : nullptr;
	for (Model& model : m_models) {
		if (model.loaded != nullptr && models != nullptr && models->FreeRenderModel != nullptr) {
			models->FreeRenderModel(static_cast<vr::openvr::RenderModel*>(model.loaded));
		}
		model.loaded = nullptr;
		delete[] model.colours;
		model.colours = nullptr;
		model.failed = false;
		model.name[0] = '\0';
	}
	d3d11::Release(m_depth);
	m_depthWidth = 0;
	m_depthHeight = 0;
	m_depthFailed = false;
}

bool ControllerModels::EnsureModel(const vr::OpenVRBackend& backend, bool rightHand) {
	Model& model = m_models[rightHand ? 0 : 1];
	if (model.failed) {
		return false;
	}
	if (model.loaded != nullptr && model.colours != nullptr) {
		return true;
	}
	vr::openvr::IVRRenderModelsFnTable* models = backend.RenderModels();
	if (models == nullptr || models->LoadRenderModel_Async == nullptr) {
		model.failed = true;
		return false;
	}
	if (model.name[0] == '\0' &&
	    !backend.HandRenderModelName(rightHand, model.name, sizeof(model.name))) {
		return false;  // no controller in that hand yet: asked again next frame
	}
	vr::openvr::RenderModel* loaded = nullptr;
	const int error = models->LoadRenderModel_Async(model.name, &loaded);
	if (error == vr::openvr::kRenderModelErrorLoading) {
		return false;
	}
	if (error != vr::openvr::kRenderModelErrorNone || loaded == nullptr ||
	    loaded->vertexData == nullptr || loaded->indexData == nullptr ||
	    loaded->vertexCount == 0 || loaded->vertexCount > kMaxModelVertices ||
	    loaded->triangleCount == 0) {
		model.failed = true;
		OBVR_LOG("Controllers: the %s controller's model \"%s\" could not be loaded (%d) - it "
		         "is not drawn",
		         rightHand ? "right" : "left", model.name, error);
		return false;
	}
	model.colours = new (std::nothrow) UInt32[loaded->vertexCount];
	if (model.colours == nullptr) {
		models->FreeRenderModel(loaded);
		model.failed = true;
		return false;
	}
	for (UInt32 i = 0; i < loaded->vertexCount; ++i) {
		const vr::openvr::HmdVector3& n = loaded->vertexData[i].normal;
		model.colours[i] = ShadedModelColour(NiPoint3{n.v[0], n.v[1], n.v[2]});
	}
	model.loaded = loaded;
	OBVR_LOG("Controllers: the %s controller's model \"%s\" loaded - %u vertices, %u triangles",
	         rightHand ? "right" : "left", model.name, loaded->vertexCount,
	         loaded->triangleCount);
	return true;
}

bool ControllerModels::EnsureDepth(void* device, UInt32 width, UInt32 height) {
	if (m_depth != nullptr && m_depthWidth == width && m_depthHeight == height) {
		return true;
	}
	if (m_depthFailed) {
		return false;
	}
	d3d11::Release(m_depth);
	auto create = d3d9::Method<d3d9::CreateDepthStencilSurfaceFn>(
		device, d3d9::kDeviceCreateDepthStencilSurface);
	if (create == nullptr ||
	    d3d11::Failed(create(device, width, height, d3d9::kFormatD24S8, 0, 0, 0, &m_depth,
	                         nullptr)) ||
	    m_depth == nullptr) {
		m_depth = nullptr;
		m_depthFailed = true;
		OBVR_LOG("Controllers: no depth surface for the models (%ux%u) - they are not drawn",
		         width, height);
		return false;
	}
	m_depthWidth = width;
	m_depthHeight = height;
	return true;
}

bool ControllerModels::EnsureScratch(UInt32 vertices, UInt32 indices) {
	if (vertices > m_vertexCapacity) {
		delete[] static_cast<ModelVertex*>(m_vertices);
		m_vertices = new (std::nothrow) ModelVertex[vertices];
		m_vertexCapacity = m_vertices != nullptr ? vertices : 0;
	}
	if (indices > m_indexCapacity) {
		delete[] m_indices;
		m_indices = new (std::nothrow) UInt16[indices];
		m_indexCapacity = m_indices != nullptr ? indices : 0;
	}
	return m_vertices != nullptr && m_indices != nullptr && vertices <= m_vertexCapacity &&
	       indices <= m_indexCapacity;
}

void ControllerModels::Draw(const vr::OpenVRBackend& backend, void* device,
                            void* const eyeSurfaces[2],
                            UInt32 width, UInt32 height, const EyeProjection eyes[2]) {
	if (device == nullptr || eyeSurfaces[0] == nullptr || eyeSurfaces[1] == nullptr ||
	    width == 0 || height == 0) {
		return;
	}

	// Index 0 the right controller, 1 the left - the physical ones, whatever
	// roles they play.
	bool ready[2] = {false, false};
	vr::openvr::HmdMatrix34 poses[2]{};
	UInt32 mostVertices = 0;
	UInt32 mostIndices = 0;
	for (int hand = 0; hand < 2; ++hand) {
		const bool right = hand == 0;
		ready[hand] = EnsureModel(backend, right) && backend.HandPoseMatrix(right, poses[hand]);
		if (ready[hand]) {
			const auto* model = static_cast<const vr::openvr::RenderModel*>(m_models[hand].loaded);
			if (model->vertexCount > mostVertices) {
				mostVertices = model->vertexCount;
			}
			if (model->triangleCount * 3 > mostIndices) {
				mostIndices = model->triangleCount * 3;
			}
		}
	}
	if (!ready[0] && !ready[1]) {
		return;
	}
	vr::openvr::HmdMatrix34 head{};
	vr::openvr::HmdMatrix34 eyeToHead[2]{};
	if (!backend.GetRenderPoseMatrix(head) ||
	    !backend.GetEyeToHead(vr::openvr::kEyeLeft, eyeToHead[0]) ||
	    !backend.GetEyeToHead(vr::openvr::kEyeRight, eyeToHead[1]) ||
	    !EnsureDepth(device, width, height) || !EnsureScratch(mostVertices, mostIndices)) {
		return;
	}

	auto getTarget = d3d9::Method<d3d9::GetRenderTargetFn>(device, d3d9::kDeviceGetRenderTarget);
	auto setTarget = d3d9::Method<d3d9::SetRenderTargetFn>(device, d3d9::kDeviceSetRenderTarget);
	auto getDepth =
		d3d9::Method<d3d9::GetDepthStencilSurfaceFn>(device, d3d9::kDeviceGetDepthStencilSurface);
	auto setDepth =
		d3d9::Method<d3d9::SetDepthStencilSurfaceFn>(device, d3d9::kDeviceSetDepthStencilSurface);
	auto createBlock = d3d9::Method<d3d9::CreateStateBlockFn>(device, d3d9::kDeviceCreateStateBlock);
	auto setState = d3d9::Method<d3d9::SetRenderStateFn>(device, d3d9::kDeviceSetRenderState);
	auto setStage =
		d3d9::Method<d3d9::SetTextureStageStateFn>(device, d3d9::kDeviceSetTextureStageState);
	auto setTexture = d3d9::Method<d3d9::SetTextureFn>(device, d3d9::kDeviceSetTexture);
	auto setFvf = d3d9::Method<d3d9::SetFVFFn>(device, d3d9::kDeviceSetFVF);
	auto setVertexShader = d3d9::Method<d3d9::SetVertexShaderFn>(device, d3d9::kDeviceSetVertexShader);
	auto setPixelShader = d3d9::Method<d3d9::SetPixelShaderFn>(device, d3d9::kDeviceSetPixelShader);
	auto beginScene = d3d9::Method<d3d9::SceneBracketFn>(device, d3d9::kDeviceBeginScene);
	auto endScene = d3d9::Method<d3d9::SceneBracketFn>(device, d3d9::kDeviceEndScene);
	auto clear = d3d9::Method<d3d9::ClearFn>(device, d3d9::kDeviceClear);
	auto draw =
		d3d9::Method<d3d9::DrawIndexedPrimitiveUPFn>(device, d3d9::kDeviceDrawIndexedPrimitiveUP);
	if (getTarget == nullptr || setTarget == nullptr || getDepth == nullptr ||
	    setDepth == nullptr || createBlock == nullptr || setState == nullptr ||
	    setStage == nullptr || setTexture == nullptr || setFvf == nullptr ||
	    setVertexShader == nullptr || setPixelShader == nullptr || beginScene == nullptr ||
	    endScene == nullptr || clear == nullptr || draw == nullptr) {
		return;
	}

	// A guest on the game's device, as the menu shade is: everything touched
	// is captured first and put back after; the targets by hand, since no
	// state block carries them.
	void* previousTarget = nullptr;
	void* previousDepth = nullptr;
	getTarget(device, 0, &previousTarget);
	getDepth(device, &previousDepth);
	void* block = nullptr;
	if (d3d11::Failed(createBlock(device, d3d9::kStateBlockTypeAll, &block)) || block == nullptr) {
		d3d11::Release(previousTarget);
		d3d11::Release(previousDepth);
		return;
	}

	setVertexShader(device, nullptr);
	setPixelShader(device, nullptr);
	setTexture(device, 0, nullptr);
	setStage(device, 0, d3d9::kTssColorOp, d3d9::kTopSelectArg1);
	setStage(device, 0, d3d9::kTssColorArg1, d3d9::kTaDiffuse);
	setStage(device, 0, d3d9::kTssAlphaOp, d3d9::kTopSelectArg1);
	setStage(device, 0, d3d9::kTssAlphaArg1, d3d9::kTaDiffuse);
	setStage(device, 1, d3d9::kTssColorOp, d3d9::kTopDisable);
	setStage(device, 1, d3d9::kTssAlphaOp, d3d9::kTopDisable);
	setState(device, d3d9::kRenderStateZEnable, 1);
	setState(device, d3d9::kRenderStateZWriteEnable, 1);
	setState(device, d3d9::kRenderStateZFunc, d3d9::kCmpLessEqual);
	setState(device, d3d9::kRenderStateAlphaTestEnable, 0);
	setState(device, d3d9::kRenderStateAlphaBlendEnable, 0);
	setState(device, d3d9::kRenderStateFogEnable, 0);
	setState(device, d3d9::kRenderStateLighting, 0);
	setState(device, d3d9::kRenderStateCullMode, d3d9::kCullNone);
	setState(device, d3d9::kRenderStateStencilEnable, 0);
	setState(device, d3d9::kRenderStateScissorTestEnable, 0);
	setState(device, d3d9::kRenderStateClipping, 0);
	setState(device, d3d9::kRenderStateClipPlaneEnable, 0);
	setState(device, d3d9::kRenderStateColorWriteEnable, d3d9::kColorWriteAll);
	setFvf(device, d3d9::kFvfXyzRhwDiffuse);

	const bool openedScene = !d3d11::Failed(beginScene(device));
	auto* vertices = static_cast<ModelVertex*>(m_vertices);
	UInt32 drawn = 0;
	for (int eye = 0; eye < 2; ++eye) {
		setTarget(device, 0, eyeSurfaces[eye]);
		setDepth(device, m_depth);
		clear(device, 0, nullptr, d3d9::kClearZBuffer, 0, 1.0f, 0);
		const vr::openvr::HmdMatrix34 eyeFromTracking =
			InvertRigid(ComposeRigid(head, eyeToHead[eye]));
		for (int hand = 0; hand < 2; ++hand) {
			if (!ready[hand]) {
				continue;
			}
			const auto* model = static_cast<const vr::openvr::RenderModel*>(m_models[hand].loaded);
			const vr::openvr::HmdMatrix34 toEye = ComposeRigid(eyeFromTracking, poses[hand]);
			for (UInt32 i = 0; i < model->vertexCount; ++i) {
				const vr::openvr::HmdVector3& p = model->vertexData[i].position;
				EyePixel pixel;
				ModelVertex& v = vertices[i];
				if (ProjectToEyePixel(TransformPoint(toEye, NiPoint3{p.v[0], p.v[1], p.v[2]}),
				                      eyes[eye], static_cast<float>(width),
				                      static_cast<float>(height), kNearMetres, kFarMetres, pixel)) {
					v = ModelVertex{pixel.x, pixel.y, pixel.depth, pixel.rhw,
					                m_models[hand].colours[i]};
				} else {
					v = ModelVertex{0.0f, 0.0f, 0.0f, 0.0f, 0};  // rhw 0 marks it unusable
				}
			}
			// Only triangles whose three corners all projected: one reaching
			// behind the eye would fold across the picture.
			UInt32 kept = 0;
			const UInt32 corners = model->triangleCount * 3;
			for (UInt32 i = 0; i + 2 < corners; i += 3) {
				const UInt16 a = model->indexData[i];
				const UInt16 b = model->indexData[i + 1];
				const UInt16 c = model->indexData[i + 2];
				if (a >= model->vertexCount || b >= model->vertexCount || c >= model->vertexCount ||
				    vertices[a].rhw == 0.0f || vertices[b].rhw == 0.0f || vertices[c].rhw == 0.0f) {
					continue;
				}
				m_indices[kept++] = a;
				m_indices[kept++] = b;
				m_indices[kept++] = c;
			}
			if (kept > 0 &&
			    !d3d11::Failed(draw(device, d3d9::kPrimitiveTriangleList, 0, model->vertexCount,
			                        kept / 3, m_indices, d3d9::kFormatIndex16, vertices,
			                        sizeof(ModelVertex)))) {
				++drawn;
			}
		}
	}
	if (openedScene) {
		endScene(device);
	}

	// The game's targets first, then its states: SetRenderTarget resets the
	// viewport, the state block's Apply puts the real one back.
	setTarget(device, 0, previousTarget);
	setDepth(device, previousDepth);
	auto apply = d3d9::Method<d3d9::StateBlockMethodFn>(block, d3d9::kStateBlockApply);
	if (apply != nullptr) {
		apply(block);
	}
	d3d11::Release(block);
	d3d11::Release(previousTarget);
	d3d11::Release(previousDepth);

	if (!m_reported && drawn > 0) {
		m_reported = true;
		OBVR_LOG("Controllers: the controller models are drawn into the eyes (%u draws this "
		         "frame)",
		         drawn);
	}
}

void ControllerModels::Prepare(const vr::OpenVRBackend& backend) {
	EnsureModel(backend, true);
	EnsureModel(backend, false);
}

bool ControllerModels::DrawIntoWorld(void* device, const WorldView& view, bool leftEye) {
	if (device == nullptr || !view.eyeValid) {
		return false;
	}
	UInt32 mostVertices = 0;
	UInt32 mostIndices = 0;
	bool any = false;
	for (const WorldController& hand : view.hands) {
		const Model& model = m_models[hand.rightModel ? 0 : 1];
		if (!hand.valid || model.loaded == nullptr || model.colours == nullptr) {
			continue;
		}
		any = true;
		const auto* loaded = static_cast<const vr::openvr::RenderModel*>(model.loaded);
		if (loaded->vertexCount > mostVertices) {
			mostVertices = loaded->vertexCount;
		}
		if (loaded->triangleCount * 3 > mostIndices) {
			mostIndices = loaded->triangleCount * 3;
		}
	}
	if (!any || !EnsureScratch(mostVertices, mostIndices)) {
		return false;
	}

	// The projection the eye's picture was drawn with - if what the device
	// holds is the world camera's. After the world the last matrix set can
	// belong to anything, so it is recognised before it is used.
	auto getTransform = d3d9::Method<d3d9::GetTransformFn>(device, d3d9::kDeviceGetTransform);
	auto getViewport = d3d9::Method<d3d9::GetViewportFn>(device, d3d9::kDeviceGetViewport);
	d3d9::Matrix4 projection{};
	d3d9::Viewport viewport{};
	if (getTransform == nullptr || getViewport == nullptr ||
	    d3d11::Failed(getTransform(device, d3d9::kTransformProjection, &projection)) ||
	    d3d11::Failed(getViewport(device, &viewport)) || viewport.width == 0 ||
	    viewport.height == 0 || !PerspectiveMatchesCamera(projection.m, view.tanHalfWidth)) {
		if (!m_worldRefusedReported) {
			m_worldRefusedReported = true;
			OBVR_LOG("Controllers: the device's projection is not the world camera's (m00 %.4f "
			         "m11 %.4f m22 %.4f m23 %.4f m32 %.4f, camera tangent %.4f) - the models go "
			         "on top of the picture instead",
			         static_cast<double>(projection.m[0][0]),
			         static_cast<double>(projection.m[1][1]),
			         static_cast<double>(projection.m[2][2]),
			         static_cast<double>(projection.m[2][3]),
			         static_cast<double>(projection.m[3][2]),
			         static_cast<double>(view.tanHalfWidth));
		}
		return false;
	}

	auto createBlock = d3d9::Method<d3d9::CreateStateBlockFn>(device, d3d9::kDeviceCreateStateBlock);
	auto setState = d3d9::Method<d3d9::SetRenderStateFn>(device, d3d9::kDeviceSetRenderState);
	auto setStage =
		d3d9::Method<d3d9::SetTextureStageStateFn>(device, d3d9::kDeviceSetTextureStageState);
	auto setTexture = d3d9::Method<d3d9::SetTextureFn>(device, d3d9::kDeviceSetTexture);
	auto setFvf = d3d9::Method<d3d9::SetFVFFn>(device, d3d9::kDeviceSetFVF);
	auto setVertexShader = d3d9::Method<d3d9::SetVertexShaderFn>(device, d3d9::kDeviceSetVertexShader);
	auto setPixelShader = d3d9::Method<d3d9::SetPixelShaderFn>(device, d3d9::kDeviceSetPixelShader);
	auto beginScene = d3d9::Method<d3d9::SceneBracketFn>(device, d3d9::kDeviceBeginScene);
	auto endScene = d3d9::Method<d3d9::SceneBracketFn>(device, d3d9::kDeviceEndScene);
	auto draw =
		d3d9::Method<d3d9::DrawIndexedPrimitiveUPFn>(device, d3d9::kDeviceDrawIndexedPrimitiveUP);
	if (createBlock == nullptr || setState == nullptr || setStage == nullptr ||
	    setTexture == nullptr || setFvf == nullptr || setVertexShader == nullptr ||
	    setPixelShader == nullptr || beginScene == nullptr || endScene == nullptr ||
	    draw == nullptr) {
		return false;
	}
	void* block = nullptr;
	if (d3d11::Failed(createBlock(device, d3d9::kStateBlockTypeAll, &block)) || block == nullptr) {
		return false;
	}

	// The game's target and depth stay bound: that is the point. Tested
	// against the depth, not written into it - the picture is captured next
	// and the depth has no later reader this frame that should see a model.
	setVertexShader(device, nullptr);
	setPixelShader(device, nullptr);
	setTexture(device, 0, nullptr);
	setStage(device, 0, d3d9::kTssColorOp, d3d9::kTopSelectArg1);
	setStage(device, 0, d3d9::kTssColorArg1, d3d9::kTaDiffuse);
	setStage(device, 0, d3d9::kTssAlphaOp, d3d9::kTopSelectArg1);
	setStage(device, 0, d3d9::kTssAlphaArg1, d3d9::kTaDiffuse);
	setStage(device, 1, d3d9::kTssColorOp, d3d9::kTopDisable);
	setStage(device, 1, d3d9::kTssAlphaOp, d3d9::kTopDisable);
	setState(device, d3d9::kRenderStateZEnable, 1);
	setState(device, d3d9::kRenderStateZWriteEnable, 0);
	setState(device, d3d9::kRenderStateZFunc, d3d9::kCmpLessEqual);
	setState(device, d3d9::kRenderStateAlphaTestEnable, 0);
	setState(device, d3d9::kRenderStateAlphaBlendEnable, 0);
	setState(device, d3d9::kRenderStateFogEnable, 0);
	setState(device, d3d9::kRenderStateLighting, 0);
	setState(device, d3d9::kRenderStateCullMode, d3d9::kCullNone);
	setState(device, d3d9::kRenderStateStencilEnable, 0);
	setState(device, d3d9::kRenderStateScissorTestEnable, 0);
	setState(device, d3d9::kRenderStateClipping, 0);
	setState(device, d3d9::kRenderStateClipPlaneEnable, 0);
	setState(device, d3d9::kRenderStateColorWriteEnable, d3d9::kColorWriteAll);
	setFvf(device, d3d9::kFvfXyzRhwDiffuse);
	const bool openedScene = !d3d11::Failed(beginScene(device));

	const NiMatrix33 toEye = InverseRotation(view.eyeRot);
	auto* vertices = static_cast<ModelVertex*>(m_vertices);
	UInt32 drawn = 0;
	for (const WorldController& hand : view.hands) {
		const Model& model = m_models[hand.rightModel ? 0 : 1];
		if (!hand.valid || model.loaded == nullptr || model.colours == nullptr) {
			continue;
		}
		const auto* loaded = static_cast<const vr::openvr::RenderModel*>(model.loaded);
		for (UInt32 i = 0; i < loaded->vertexCount; ++i) {
			const NiPoint3 local = ControllerVertexToGame(loaded->vertexData[i].position);
			const NiPoint3 world = hand.pos + hand.rot * (local * view.unitsPerMetre);
			EyePixel pixel;
			ModelVertex& v = vertices[i];
			if (ProjectThroughD3D(toEye * (world - view.eyePos), projection.m,
			                      static_cast<float>(viewport.x), static_cast<float>(viewport.y),
			                      static_cast<float>(viewport.width),
			                      static_cast<float>(viewport.height), viewport.minZ,
			                      viewport.maxZ, pixel)) {
				v = ModelVertex{pixel.x, pixel.y, pixel.depth, pixel.rhw, model.colours[i]};
			} else {
				v = ModelVertex{0.0f, 0.0f, 0.0f, 0.0f, 0};
			}
		}
		UInt32 kept = 0;
		const UInt32 corners = loaded->triangleCount * 3;
		for (UInt32 i = 0; i + 2 < corners; i += 3) {
			const UInt16 a = loaded->indexData[i];
			const UInt16 b = loaded->indexData[i + 1];
			const UInt16 c = loaded->indexData[i + 2];
			if (a >= loaded->vertexCount || b >= loaded->vertexCount || c >= loaded->vertexCount ||
			    vertices[a].rhw == 0.0f || vertices[b].rhw == 0.0f || vertices[c].rhw == 0.0f) {
				continue;
			}
			m_indices[kept++] = a;
			m_indices[kept++] = b;
			m_indices[kept++] = c;
		}
		if (kept > 0 &&
		    !d3d11::Failed(draw(device, d3d9::kPrimitiveTriangleList, 0, loaded->vertexCount,
		                        kept / 3, m_indices, d3d9::kFormatIndex16, vertices,
		                        sizeof(ModelVertex)))) {
			++drawn;
		}
	}
	if (openedScene) {
		endScene(device);
	}
	auto apply = d3d9::Method<d3d9::StateBlockMethodFn>(block, d3d9::kStateBlockApply);
	if (apply != nullptr) {
		apply(block);
	}
	d3d11::Release(block);

	m_worldEyes |= leftEye ? 1u : 2u;
	if (!m_worldReported) {
		m_worldReported = true;
		OBVR_LOG("Controllers: drawn into the world with the game's projection and depth "
		         "(%u models, viewport %ux%u)",
		         drawn, viewport.width, viewport.height);
	}
	return true;
}

}  // namespace obvr::render
