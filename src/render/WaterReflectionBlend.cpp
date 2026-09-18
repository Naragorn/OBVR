#include "render/WaterReflectionBlend.h"

#include <cstring>

#include "core/Config.h"
#include "core/Log.h"
#include "platform/Win32Min.h"
#include "render/D3D9Types.h"
#include "render/D3D11Types.h"

namespace obvr::render {
namespace {

using AssembleShaderFn = SInt32(__stdcall*)(const char*, UInt32, const void*,
	                                         const void*, UInt32, void**, void**);
using BufferPointerFn = void*(__stdcall*)(void*);

void* g_device = nullptr;
void* g_shader = nullptr;
void* g_diagnosticShader = nullptr;
void* g_textures[kWaterReflectionCaptureCount]{};
void* g_surfaces[kWaterReflectionCaptureCount]{};
UInt32 g_width = 0, g_height = 0, g_format = 0;
bool g_refused = false;
bool g_selected = false;
bool g_selectedDiagnostic = false;
bool g_stableSelected = false;
bool g_matricesValid = false;
float g_blendMatrices[12][4]{};
void* g_savedTextures[kWaterReflectionCaptureCount]{};
UInt32 g_savedSamplers[kWaterReflectionCaptureCount][4]{};
bool g_drawBound = false;
UInt32 g_drawSerial = 0;
UInt32 g_diagnosticDrawSerial = 0;
bool g_drawReported = false;

static const char kBlendShader[] =
#include "render/WaterReflectionBlendShader.inc"
;

// Test-only output: red=left, green=right, blue=center capture coverage.
// Black means none of the three reflection projections reaches the water pixel.
static const char kDiagnosticShader[] =
"ps_2_x\n"
"def c0, 1, 1, -1, 0\n"
"def c12, 0.5, 8.0, 0.0001, 0\n"
"dcl_centroid t0.xyz\n"
"dcl_centroid t1.xyz\n"
"mov r1.xyz, t0\n"
"mov r1.w, c0.x\n"
"dp4 r5.x, c13, r1\n"
"dp4 r5.y, c14, r1\n"
"dp4 r5.z, c15, r1\n"
"dp4 r5.w, c16, r1\n"
"mad r5.xy, r5, c12.x, r5.w\n"
"dp4 r6.x, c17, r1\n"
"dp4 r6.y, c18, r1\n"
"dp4 r6.z, c19, r1\n"
"dp4 r6.w, c20, r1\n"
"mad r6.xy, r6, c12.x, r6.w\n"
"dp4 r7.x, c21, r1\n"
"dp4 r7.y, c22, r1\n"
"dp4 r7.z, c23, r1\n"
"dp4 r7.w, c24, r1\n"
"mad r7.xy, r7, c12.x, r7.w\n"
"add r8.x, r5.w, -r5.x\n"
"add r8.y, r5.w, -r5.y\n"
"min r8.x, r8.x, r5.x\n"
"min r8.x, r8.x, r5.y\n"
"rcp r8.y, r5.w\n"
"mul r5.z, r8.x, r8.y\n"
"mul_sat r5.z, r5.z, c12.y\n"
"add r8.x, r7.w, -r7.x\n"
"add r8.y, r7.w, -r7.y\n"
"min r8.x, r8.x, r7.x\n"
"min r8.x, r8.x, r7.y\n"
"rcp r8.y, r7.w\n"
"mul r7.z, r8.x, r8.y\n"
"mul_sat r7.z, r7.z, c12.y\n"
"add r8.x, r6.w, -r6.x\n"
"add r8.y, r6.w, -r6.y\n"
"min r8.x, r8.x, r6.x\n"
"min r8.x, r8.x, r6.y\n"
"rcp r8.y, r6.w\n"
"mul r6.z, r8.x, r8.y\n"
"mul_sat r6.z, r6.z, c12.y\n"
"mov r0.x, r5.z\n"
"mov r0.y, r7.z\n"
"mov r0.z, r6.z\n"
"mov r0.w, c0.x\n"
"mov oC0, r0\n";

void ReleaseTextures() {
	for (unsigned i = 0; i < kWaterReflectionCaptureCount; ++i) {
		d3d11::Release(g_surfaces[i]);
		d3d11::Release(g_textures[i]);
		g_surfaces[i] = nullptr;
		g_textures[i] = nullptr;
	}
	g_width = g_height = g_format = 0;
}

void ReleaseAll() {
	ReleaseTextures();
	d3d11::Release(g_shader);
	d3d11::Release(g_diagnosticShader);
	g_shader = nullptr;
	g_diagnosticShader = nullptr;
	g_matricesValid = false;
}

bool CreateShader(void* device, const char* source, void** shader) {
	HMODULE d3dx = GetModuleHandleA("d3dx9_27.dll");
	if (d3dx == nullptr) d3dx = LoadLibraryA("d3dx9_27.dll");
	auto assemble = d3dx != nullptr
		? reinterpret_cast<AssembleShaderFn>(GetProcAddress(d3dx, "D3DXAssembleShader"))
		: nullptr;
	if (assemble == nullptr) return false;
	void* code = nullptr;
	void* errors = nullptr;
	const SInt32 result = assemble(source, static_cast<UInt32>(std::strlen(source)), nullptr,
	                              nullptr, 0, &code, &errors);
	if (result < 0 || code == nullptr) {
		auto pointer = errors != nullptr
			? d3d9::Method<BufferPointerFn>(errors, d3d9::kBufferGetPointer) : nullptr;
		OBVR_LOG("Water reflection: three-capture pixel shader assembly failed (%08X)%s%s",
		         result, pointer ? ": " : "", pointer ? static_cast<const char*>(pointer(errors)) : "");
		d3d11::Release(errors);
		d3d11::Release(code);
		return false;
	}
	auto pointer = d3d9::Method<BufferPointerFn>(code, d3d9::kBufferGetPointer);
	auto create = d3d9::Method<d3d9::CreatePixelShaderFn>(device,
	                                                    d3d9::kDeviceCreatePixelShader);
	const bool ok = pointer != nullptr && create != nullptr &&
		create(device, static_cast<const UInt32*>(pointer(code)), shader) >= 0 &&
		*shader != nullptr;
	d3d11::Release(errors);
	d3d11::Release(code);
	return ok;
}

}  // namespace

void PrepareWaterReflectionBlend(void* device) {
	if (device == nullptr || g_refused) return;
	if (g_device != device) {
		ReleaseAll();
		g_device = device;
	}
	if (g_shader != nullptr && (!GetConfig().vrTestWaterCoverageDiagnostic || g_diagnosticShader != nullptr)) return;
	if (g_shader == nullptr && !CreateShader(device, kBlendShader, &g_shader)) {
		g_refused = true;
		OBVR_LOG("Water reflection: three-capture blend unavailable; stable mode falls back");
		return;
	}
	if (GetConfig().vrTestWaterCoverageDiagnostic && g_diagnosticShader == nullptr &&
	    !CreateShader(device, kDiagnosticShader, &g_diagnosticShader)) {
		g_refused = true;
		OBVR_LOG("Water reflection: diagnostic pixel shader unavailable; test suite refuses incomplete evidence");
		return;
	}
	OBVR_LOG("Water reflection: three-capture overlap shader ready");
}

bool WaterReflectionBlendReady() {
	return g_shader != nullptr && !g_refused && (!GetConfig().vrTestWaterCoverageDiagnostic || g_diagnosticShader != nullptr);
}

void* SelectWaterReflectionPixelShader(void* device, void* requested,
	                                    bool waterShader, WaterReflectionMode mode) {
	g_selected = false;
	if (ShouldBypassWaterReprojectionInReflectionSubpass(IsWaterReflectionSubpass()))
		return requested;
	g_selectedDiagnostic = false;
	g_stableSelected = false;
	if (!waterShader || !UsesWaterReprojectionShader(mode)) return requested;
	PrepareWaterReflectionBlend(device);
	if (!WaterReflectionBlendReady() || !WaterReflectionTexturesReady()) return requested;
	g_selected = true;
	g_selectedDiagnostic = GetConfig().vrTestWaterCoverageDiagnostic;
	if (UsesStableWaterReflectionShader(mode, g_selectedDiagnostic)) {
		g_stableSelected = true;
		return g_shader;
	}
	return g_selectedDiagnostic ? g_diagnosticShader : g_shader;
}

void SetWaterReflectionBlendMatrices(const WaterMatrix& left,
	                                  const WaterMatrix& center,
	                                  const WaterMatrix& right, bool valid) {
	g_matricesValid = valid;
	if (!valid) return;
	std::memcpy(g_blendMatrices, left.m, sizeof(left.m));
	std::memcpy(g_blendMatrices + 4, center.m, sizeof(center.m));
	std::memcpy(g_blendMatrices + 8, right.m, sizeof(right.m));
}

bool StoreWaterReflectionTexture(void* device, void* sourceSurface,
	                              UInt32 width, UInt32 height, UInt32 format,
	                              unsigned slot) {
	if (device == nullptr || sourceSurface == nullptr ||
	    slot >= kWaterReflectionCaptureCount || width == 0 || height == 0) return false;
	if (g_device != device || g_width != width || g_height != height || g_format != format) {
		if (g_device != device) { ReleaseAll(); g_device = device; }
		else ReleaseTextures();
		g_width = width; g_height = height; g_format = format;
	}
	if (g_textures[slot] == nullptr) {
		auto create = d3d9::Method<d3d9::CreateTextureFn>(device, d3d9::kDeviceCreateTexture);
		if (create == nullptr || create(device, width, height, 1, d3d9::kUsageRenderTarget,
		                              format, d3d9::kPoolDefault, &g_textures[slot], nullptr) < 0 ||
		    g_textures[slot] == nullptr) return false;
		auto surface = d3d9::Method<d3d9::GetSurfaceLevelFn>(
			g_textures[slot], d3d9::kTextureGetSurfaceLevel);
		if (surface == nullptr || surface(g_textures[slot], 0, &g_surfaces[slot]) < 0 ||
		    g_surfaces[slot] == nullptr) return false;
	}
	auto copy = d3d9::Method<d3d9::StretchRectFn>(device, d3d9::kDeviceStretchRect);
	return copy != nullptr && copy(device, sourceSurface, nullptr, g_surfaces[slot],
	                               nullptr, d3d9::kTexFilterNone) >= 0;
}

bool WaterReflectionTexturesReady() {
	for (unsigned i = 0; i < kWaterReflectionCaptureCount; ++i)
		if (g_textures[i] == nullptr || g_surfaces[i] == nullptr) return false;
	return true;
}

void* GetWaterReflectionStoredSurface(unsigned slot) {
	return slot < kWaterReflectionCaptureCount ? g_surfaces[slot] : nullptr;
}

bool BeginWaterReflectionBlendDraw(void* device) {
	if (!g_selected || !g_matricesValid || !WaterReflectionTexturesReady() ||
	    g_drawBound || device == nullptr) return false;
	auto get = d3d9::Method<d3d9::GetTextureFn>(device, d3d9::kDeviceGetTexture);
	auto set = d3d9::Method<d3d9::SetTextureFn>(device, d3d9::kDeviceSetTexture);
	if (get == nullptr || set == nullptr) return false;
	auto getSampler = d3d9::Method<d3d9::GetSamplerStateFn>(device, d3d9::kDeviceGetSamplerState);
	auto setSampler = d3d9::Method<d3d9::SetSamplerStateFn>(device, d3d9::kDeviceSetSamplerState);
	auto constants = d3d9::Method<d3d9::SetPixelShaderConstantFFn>(
		device, d3d9::kDeviceSetPixelShaderConstantF);
	if (getSampler == nullptr || setSampler == nullptr || constants == nullptr) return false;
	const UInt32 states[4] = {d3d9::kSamplerAddressU, d3d9::kSamplerAddressV,
	                          d3d9::kSamplerMagFilter, d3d9::kSamplerMinFilter};
	for (unsigned stage = 0; stage < kWaterReflectionCaptureCount; ++stage)
		for (unsigned state = 0; state < 4; ++state)
			if (getSampler(device, stage, states[state], &g_savedSamplers[stage][state]) < 0)
				return false;
	for (unsigned i = 0; i < kWaterReflectionCaptureCount; ++i) {
		g_savedTextures[i] = nullptr;
		if (get(device, i, &g_savedTextures[i]) < 0 || set(device, i, g_textures[i]) < 0) {
			for (unsigned j = 0; j <= i; ++j) {
				set(device, j, g_savedTextures[j]);
				d3d11::Release(g_savedTextures[j]);
				g_savedTextures[j] = nullptr;
			}
			return false;
		}
	}
	bool statesOk = constants(device, 13, &g_blendMatrices[0][0], 12) >= 0;
	for (unsigned stage = 0; stage < kWaterReflectionCaptureCount; ++stage) {
		statesOk = setSampler(device, stage, d3d9::kSamplerAddressU,
		                     d3d9::kTextureAddressClamp) >= 0 && statesOk;
		statesOk = setSampler(device, stage, d3d9::kSamplerAddressV,
		                     d3d9::kTextureAddressClamp) >= 0 && statesOk;
		statesOk = setSampler(device, stage, d3d9::kSamplerMagFilter,
		                     d3d9::kTexFilterLinear) >= 0 && statesOk;
		statesOk = setSampler(device, stage, d3d9::kSamplerMinFilter,
		                     d3d9::kTexFilterLinear) >= 0 && statesOk;
	}
	if (!statesOk) {
		for (unsigned stage = 0; stage < kWaterReflectionCaptureCount; ++stage) {
			for (unsigned state = 0; state < 4; ++state)
				setSampler(device, stage, states[state], g_savedSamplers[stage][state]);
			set(device, stage, g_savedTextures[stage]);
			d3d11::Release(g_savedTextures[stage]);
			g_savedTextures[stage] = nullptr;
		}
		return false;
	}
	g_drawBound = true;
	return true;
}

void EndWaterReflectionBlendDraw(void* device) {
	if (!g_drawBound || device == nullptr) return;
	auto set = d3d9::Method<d3d9::SetTextureFn>(device, d3d9::kDeviceSetTexture);
	auto setSampler = d3d9::Method<d3d9::SetSamplerStateFn>(device, d3d9::kDeviceSetSamplerState);
	const UInt32 states[4] = {d3d9::kSamplerAddressU, d3d9::kSamplerAddressV,
	                          d3d9::kSamplerMagFilter, d3d9::kSamplerMinFilter};
	for (unsigned stage = 0; stage < kWaterReflectionCaptureCount; ++stage) {
		if (setSampler != nullptr)
			for (unsigned state = 0; state < 4; ++state)
				setSampler(device, stage, states[state], g_savedSamplers[stage][state]);
		if (set != nullptr) set(device, stage, g_savedTextures[stage]);
		d3d11::Release(g_savedTextures[stage]);
		g_savedTextures[stage] = nullptr;
	}
	g_drawBound = false;
	++g_drawSerial;
	if (g_selectedDiagnostic) ++g_diagnosticDrawSerial;
	if (!g_drawReported) {
		g_drawReported = true;
		OBVR_LOG("Water reflection: three captures bound and blended on a water draw");
	}
}

UInt32 WaterReflectionBlendDrawSerial() { return g_drawSerial; }
UInt32 WaterReflectionDiagnosticDrawSerial() { return g_diagnosticDrawSerial; }

}  // namespace obvr::render
