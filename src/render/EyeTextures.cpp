#include "render/EyeTextures.h"

#include "core/Log.h"
#include "platform/Win32Min.h"

namespace obvr::render {
namespace {

constexpr const char* kD3D11Library = "d3d11.dll";

template <typename Fn>
Fn Resolve(void* module, const char* name) {
	return reinterpret_cast<Fn>(GetProcAddress(static_cast<HMODULE>(module), name));
}

}  // namespace

bool EyeTextures::CreateDevice() {
	m_module = LoadLibraryA(kD3D11Library);
	if (m_module == nullptr) {
		OBVR_LOG("Render: %s could not be loaded", kD3D11Library);
		return false;
	}

	auto createDevice = Resolve<d3d11::CreateDeviceFn>(m_module, "D3D11CreateDevice");
	if (createDevice == nullptr) {
		OBVR_LOG("Render: %s has no D3D11CreateDevice", kD3D11Library);
		return false;
	}

	// No adapter, no flags, no feature level list: the default is the primary
	// adapter at the highest level it supports, which is what OBVR wants and
	// saves replicating IDXGIAdapter and D3D_FEATURE_LEVEL to say so.
	//
	// The immediate context is asked for and then only held. OBVR never draws
	// with it - the texture is filled at creation from a buffer in system
	// memory - but a context the caller declines to take is one the device
	// keeps a reference to anyway, and holding it makes the release explicit
	// rather than implicit.
	d3d11::ResultCode result =
		createDevice(nullptr, d3d11::kDriverTypeHardware, nullptr, 0, nullptr, 0,
	                 d3d11::kSdkVersion, &m_device, nullptr, &m_context);

	if (d3d11::Failed(result) || m_device == nullptr) {
		// WARP is Microsoft's software rasteriser. It has no business
		// rendering a game, but this milestone only needs a picture to exist,
		// and falling back means a machine with an awkward driver still tells
		// us whether the compositor path works - which is the whole question
		// 0.0.5 is asking.
		OBVR_LOG("Render: no hardware device (0x%08X), trying WARP",
		         static_cast<UInt32>(result));

		m_device = nullptr;
		m_context = nullptr;
		result = createDevice(nullptr, d3d11::kDriverTypeWarp, nullptr, 0, nullptr, 0,
		                      d3d11::kSdkVersion, &m_device, nullptr, &m_context);
	}

	if (d3d11::Failed(result) || m_device == nullptr) {
		OBVR_LOG("Render: D3D11CreateDevice failed (0x%08X)", static_cast<UInt32>(result));
		m_device = nullptr;
		m_context = nullptr;
		return false;
	}

	return true;
}

bool EyeTextures::CreateEyeTexture(Eye eye, float crossU, d3d11::Texture2D*& out) {
	const UInt32 bytes = PatternBufferBytes(m_width, m_height);
	if (bytes == 0) {
		return false;
	}

	// VirtualAlloc rather than an allocator: OBVR has no C runtime heap in
	// the freestanding build, and this is one large short-lived block, which
	// is exactly what VirtualAlloc is for.
	auto* staging = static_cast<UInt8*>(
		VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (staging == nullptr) {
		OBVR_LOG("Render: could not reserve %u bytes for the %s eye", bytes,
		         eye == Eye::Left ? "left" : "right");
		return false;
	}

	FillPattern(staging, m_width, m_height, m_width * 4u, eye, crossU);

	d3d11::Texture2DDesc desc{};
	desc.width = m_width;
	desc.height = m_height;
	desc.mipLevels = 1;
	desc.arraySize = 1;
	desc.format = d3d11::kFormatR8G8B8A8Unorm;
	desc.sampleDesc.count = 1;
	desc.sampleDesc.quality = 0;
	desc.usage = d3d11::kUsageDefault;

	// Render target as well as shader resource, even though nothing renders
	// into it yet. It is what the compositor expects of a submitted texture,
	// and it is what a texture shared out of D3D9 must have in 0.1.0 - so the
	// description does not have to change when the pixels start coming from
	// the game instead of from here.
	desc.bindFlags = d3d11::kBindShaderResource | d3d11::kBindRenderTarget;
	desc.cpuAccessFlags = 0;
	desc.miscFlags = 0;

	d3d11::SubresourceData initial{};
	initial.sysMem = staging;
	initial.sysMemPitch = m_width * 4u;
	initial.sysMemSlicePitch = 0;

	const d3d11::ResultCode result =
		m_device->vtbl->CreateTexture2D(m_device, &desc, &initial, &out);

	VirtualFree(staging, 0, MEM_RELEASE);

	if (d3d11::Failed(result) || out == nullptr) {
		OBVR_LOG("Render: CreateTexture2D failed for the %s eye (0x%08X)",
		         eye == Eye::Left ? "left" : "right", static_cast<UInt32>(result));
		out = nullptr;
		return false;
	}

	return true;
}

bool EyeTextures::Create(UInt32 width, UInt32 height, float crossULeft, float crossURight) {
	Destroy();

	if (PatternBufferBytes(width, height) == 0) {
		OBVR_LOG("Render: %ux%u is not a usable eye texture size", width, height);
		return false;
	}

	m_width = width;
	m_height = height;

	if (!CreateDevice() || !CreateEyeTexture(Eye::Left, crossULeft, m_textures[0]) ||
	    !CreateEyeTexture(Eye::Right, crossURight, m_textures[1])) {
		Destroy();
		return false;
	}

	OBVR_LOG("Render: two %ux%u eye textures ready, cross at u=%.3f and u=%.3f", width, height,
	         static_cast<double>(crossULeft), static_cast<double>(crossURight));
	return true;
}

void EyeTextures::Destroy() {
	// Textures before the device that made them, and the module last of all.
	// Releasing the device while a texture it owns is still alive leaves the
	// texture pointing at a freed device, and freeing the library while
	// anything from it is alive means the release calls jump into unmapped
	// memory - a crash on exit, which users report as "it crashes when I
	// quit" and nobody can place.
	d3d11::Release(reinterpret_cast<void*&>(m_textures[0]));
	d3d11::Release(reinterpret_cast<void*&>(m_textures[1]));
	d3d11::Release(m_context);
	d3d11::Release(reinterpret_cast<void*&>(m_device));

	if (m_module != nullptr) {
		FreeLibrary(static_cast<HMODULE>(m_module));
		m_module = nullptr;
	}

	m_width = 0;
	m_height = 0;
}

}  // namespace obvr::render
