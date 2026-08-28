// Replays one recorded frame of Oblivion's dynamic-pool traffic against a
// local d3d9.dll, twice per frame the way OBVR's dual-pass stereo does,
// and compares what the two passes actually rendered.
//
// The input is the pool timeline OBVR dumped from a live frame
// (docs/verification/pool-timeline-20260828.txt): every lock on the
// software-skinning pool with its offset, size and flags, every unlock,
// every draw fed from that pool, in order, with markers at the pass
// boundaries. In the recorded frame the two passes were event-for-event
// identical - diff found nothing - and one of them still drew collapsed
// bodies in the headset. This program exists to corner that: it performs
// the same sequence with deterministic vertex data, reads both passes
// back, and reports any pixel that differs. A difference here is the bug
// on a desk, with no game and no headset in the loop.
//
// Deliberately plain fixed-function D3D9: the live evidence acquitted
// shaders and constants, so the first honest approximation leaves them
// out. If this replay does not reproduce, the next iteration adds the
// vertex shader path.

#include <windows.h>

#include <d3d9.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

// Stride 72 throughout: every recorded lock size divides by its draw's
// vertex count to exactly 72 bytes. Position + normal + three 4D texcoords
// is 12 + 12 + 48 = 72; the declaration below matches, and the vertex
// shader consumes exactly these inputs.
constexpr UINT kStride = 72;

// Iteration three: the game draws through vertex declarations and vs_1_1
// shaders, never FVF - the live counters showed declarations 58 per pass
// and fvf 0. This replay does the same: a declaration for the 72-byte
// layout and a vs_1_1 compiled at startup through d3dcompiler_47, with
// the ModelViewProj in c0 the way every shader of the game's package
// declares it, and a 21-vector block at c10 uploaded per draw the way
// the live traffic does.
const D3DVERTEXELEMENT9 kDeclaration[] = {
    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
    {0, 24, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    {0, 40, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
    {0, 56, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
    D3DDECL_END()};

const char kShaderSource[] =
    "float4x4 mvp : register(c0);\n"
    "struct VSIn { float3 pos : POSITION; float3 n : NORMAL;\n"
    "              float4 t0 : TEXCOORD0; float4 t1 : TEXCOORD1; float4 t2 : TEXCOORD2; };\n"
    "struct VSOut { float4 pos : POSITION; float4 color : COLOR0; };\n"
    "VSOut main(VSIn i) {\n"
    "  VSOut o;\n"
    "  o.pos = mul(float4(i.pos, 1.0), mvp);\n"
    "  o.color = float4(0.5 + 0.5 * i.n, 1.0);\n"
    "  return o;\n"
    "}\n";

// d3dcompiler_47, loaded at runtime so no SDK import library is needed.
struct MiniBlob {
	void** vtbl;
	void* Pointer() {
		using GetPointerFn = void*(__stdcall*)(void*);
		return reinterpret_cast<GetPointerFn>(vtbl[3])(this);
	}
	SIZE_T Size() {
		using GetSizeFn = SIZE_T(__stdcall*)(void*);
		return reinterpret_cast<GetSizeFn>(vtbl[4])(this);
	}
	void Release() {
		using ReleaseFn = ULONG(__stdcall*)(void*);
		reinterpret_cast<ReleaseFn>(vtbl[2])(this);
	}
};
using D3DCompileFn = HRESULT(__stdcall*)(const void* src, SIZE_T srcSize, const char* name,
                                         const void* defines, void* include, const char* entry,
                                         const char* target, UINT flags1, UINT flags2,
                                         MiniBlob** code, MiniBlob** errors);

IDirect3DVertexShader9* CompileShader(IDirect3DDevice9* device) {
	HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
	if (compiler == nullptr) {
		std::printf("d3dcompiler_47.dll not found\n");
		std::exit(2);
	}
	auto compile = reinterpret_cast<D3DCompileFn>(GetProcAddress(compiler, "D3DCompile"));
	MiniBlob* code = nullptr;
	MiniBlob* errors = nullptr;
	const HRESULT hr = compile(kShaderSource, sizeof(kShaderSource) - 1, "repro", nullptr,
	                           nullptr, "main", "vs_1_1", 0, 0, &code, &errors);
	if (FAILED(hr) || code == nullptr) {
		std::printf("shader compile failed: %s\n",
		            errors != nullptr ? static_cast<const char*>(errors->Pointer()) : "?");
		std::exit(2);
	}
	IDirect3DVertexShader9* shader = nullptr;
	if (FAILED(device->CreateVertexShader(static_cast<const DWORD*>(code->Pointer()),
	                                      &shader))) {
		std::printf("CreateVertexShader failed\n");
		std::exit(2);
	}
	code->Release();
	return shader;
}

constexpr UINT kTarget = 512;  // backbuffer is kTarget x kTarget

struct Event {
	char kind;  // 'L', 'U', 'D', 'M'
	std::string label;
	std::string buffer;
	UINT a = 0, b = 0, c = 0;  // lock: offset,size,flags | draw: offset,verts,prims
};

std::vector<Event> ParseTimeline(const char* path) {
	std::vector<Event> events;
	FILE* file = std::fopen(path, "r");
	if (file == nullptr) {
		std::printf("cannot open timeline %s\n", path);
		std::exit(2);
	}
	char line[256];
	while (std::fgets(line, sizeof(line), file) != nullptr) {
		Event e{};
		char buf[64] = {};
		if (std::strncmp(line, "T ---- ", 7) == 0) {
			e.kind = 'M';
			e.label = line + 7;
			while (!e.label.empty() && (e.label.back() == '\n' || e.label.back() == '\r')) {
				e.label.pop_back();
			}
		} else if (std::sscanf(line, "T lock %63s o=%u s=%u f=%x", buf, &e.a, &e.b, &e.c) == 4) {
			e.kind = 'L';
			e.buffer = buf;
		} else if (std::sscanf(line, "T unlock %63s", buf) == 1) {
			e.kind = 'U';
			e.buffer = buf;
		} else if (std::sscanf(line, "T draw %63s o=%u v=%u p=%u", buf, &e.a, &e.b, &e.c) == 4) {
			e.kind = 'D';
			e.buffer = buf;
		} else {
			continue;
		}
		events.push_back(e);
	}
	std::fclose(file);
	return events;
}

// Deterministic vertex data: the same lock writes the same bytes in both
// passes, exactly as the live fingerprints showed the game doing. The
// positions tile a small visible grid so a draw against the wrong region
// (or against an unwritten fresh slice) changes the picture.
void FillVertices(void* data, const std::string& bufferName, UINT offset, UINT size) {
	unsigned char* bytes = static_cast<unsigned char*>(data);
	const UINT count = size / kStride;
	unsigned seed = static_cast<unsigned>(offset);
	for (char ch : bufferName) {
		seed = seed * 31u + static_cast<unsigned>(ch);
	}
	for (UINT i = 0; i < count; ++i) {
		float* v = reinterpret_cast<float*>(bytes + i * kStride);
		const unsigned h = seed + i * 2654435761u;
		// Positions inside clip space; three consecutive vertices form a
		// small triangle near a cell decided by the vertex's identity.
		const float cx = -0.9f + 1.8f * static_cast<float>((h >> 4) & 63) / 63.0f;
		const float cy = -0.9f + 1.8f * static_cast<float>((h >> 10) & 63) / 63.0f;
		const float corner = static_cast<float>(i % 3);
		v[0] = cx + 0.02f * corner;
		v[1] = cy + 0.02f * (corner == 2.0f ? 1.0f : 0.0f);
		v[2] = 0.5f;
		v[3] = 0.0f;
		v[4] = 0.0f;
		v[5] = 1.0f;
		for (UINT f = 6; f < kStride / 4; ++f) {
			v[f] = 0.0f;
		}
	}
}

// Identity ModelViewProj for c0 (row-major times mul(v, m) keeps clip
// coordinates as written), and a deterministic 21-vector block for c10 -
// the size the live traffic uploads there per draw.
const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

struct Replayer {
	IDirect3DDevice9* device = nullptr;
	IDirect3DVertexDeclaration9* declaration = nullptr;
	IDirect3DVertexShader9* shader = nullptr;
	IDirect3DIndexBuffer9* indexBuffers[4] = {};  // index patterns modulo 4/16/64/256
	float blockC10[21 * 4] = {};
	std::map<std::string, IDirect3DVertexBuffer9*> buffers;
	std::map<std::string, UINT> sizes;
	std::map<std::string, UINT> lastLockOffset;
	IDirect3DSurface9* backBuffer = nullptr;
	IDirect3DTexture9* captureTexture = nullptr;  // the CaptureEye stand-in
	IDirect3DSurface9* captureSurface = nullptr;
	IDirect3DSurface9* readback = nullptr;

	IDirect3DVertexBuffer9* Buffer(const std::string& name) {
		auto found = buffers.find(name);
		if (found != buffers.end()) {
			return found->second;
		}
		IDirect3DVertexBuffer9* buffer = nullptr;
		const UINT size = sizes[name];
		if (FAILED(device->CreateVertexBuffer(size, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
		                                      0, D3DPOOL_DEFAULT, &buffer, nullptr))) {
			std::printf("CreateVertexBuffer(%u) failed for %s\n", size, name.c_str());
			std::exit(2);
		}
		// Seed the whole buffer once, the way the never-locked buffer in the
		// recording carries content packed in an earlier frame.
		void* data = nullptr;
		if (SUCCEEDED(buffer->Lock(0, size, &data, D3DLOCK_DISCARD)) && data != nullptr) {
			FillVertices(data, name, 0, size);
			buffer->Unlock();
		}
		buffers[name] = buffer;
		return buffer;
	}

	void RunEvent(const Event& e) {
		if (e.kind == 'L') {
			IDirect3DVertexBuffer9* buffer = Buffer(e.buffer);
			void* data = nullptr;
			if (SUCCEEDED(buffer->Lock(e.a, e.b, &data, e.c)) && data != nullptr) {
				FillVertices(data, e.buffer, e.a, e.b);
			}
			lastLockOffset[e.buffer] = e.a;
		} else if (e.kind == 'U') {
			Buffer(e.buffer)->Unlock();
		} else if (e.kind == 'D') {
			IDirect3DVertexBuffer9* buffer = Buffer(e.buffer);
			device->SetStreamSource(0, buffer, 0, kStride);
			device->SetVertexDeclaration(declaration);
			device->SetVertexShader(shader);
			// The per-draw constant traffic of the live frame: c0 and the
			// 21-vector block at c10, deterministic on both passes.
			device->SetVertexShaderConstantF(0, kIdentity, 4);
			device->SetVertexShaderConstantF(10, blockC10, 21);

			// The largest index pattern that stays inside this draw's
			// vertex range - the game's meshes reuse vertices heavily, and
			// modulo patterns imitate that without per-draw index uploads.
			static const UINT mods[4] = {4, 16, 64, 256};
			int pick = -1;
			for (int k = 0; k < 4; ++k) {
				if (mods[k] <= e.b) {
					pick = k;
				}
			}
			if (pick >= 0 && e.c > 0) {
				const UINT primCount = e.c < 4096u / 3u ? e.c : 4096u / 3u;
				const UINT startVertex = lastLockOffset[e.buffer] / kStride;
				device->SetIndices(indexBuffers[pick]);
				device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
				                             static_cast<INT>(startVertex), 0, e.b, 0,
				                             primCount);
			}
		}
	}
};

}  // namespace

int main(int argc, char** argv) {
	const char* path = argc > 1 ? argv[1] : "pool-timeline.txt";
	// Repeat factor: each pass replays its sequence this many times, to
	// scale the discard traffic up to what a real game frame carries. DXVK
	// throttles staging memory per submission (10MB on 32-bit) and submits
	// implicitly mid-frame once a frame crosses it - the recorded frame's
	// pool traffic alone stays under that, but the live frame's total does
	// not, and the specular path raising exactly this pressure would explain
	// the distance threshold. Deterministic data keeps repeats idempotent.
	const int repeat = argc > 2 ? std::atoi(argv[2]) : 1;
	std::vector<Event> events = ParseTimeline(path);
	std::printf("timeline: %u events, repeat x%d\n", static_cast<unsigned>(events.size()),
	            repeat);

	// Buffer sizes: the largest byte reached by any lock, and for buffers
	// that are only ever drawn, the largest vertex count seen.
	std::map<std::string, UINT> sizes;
	for (const Event& e : events) {
		if (e.kind == 'L') {
			const UINT end = e.a + e.b;
			if (end > sizes[e.buffer]) {
				sizes[e.buffer] = end;
			}
		} else if (e.kind == 'D') {
			const UINT end = e.b * kStride;
			if (end > sizes[e.buffer]) {
				sizes[e.buffer] = end;
			}
		}
	}

	WNDCLASSA wc{};
	wc.lpfnWndProc = DefWindowProcA;
	wc.hInstance = GetModuleHandleA(nullptr);
	wc.lpszClassName = "DxvkRepro";
	RegisterClassA(&wc);
	HWND window = CreateWindowA("DxvkRepro", "DxvkRepro", WS_OVERLAPPEDWINDOW, 0, 0,
	                            kTarget, kTarget, nullptr, nullptr, wc.hInstance, nullptr);

	IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (d3d == nullptr) {
		std::printf("Direct3DCreate9 failed\n");
		return 2;
	}

	D3DPRESENT_PARAMETERS pp{};
	pp.Windowed = TRUE;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferWidth = kTarget;
	pp.BackBufferHeight = kTarget;
	pp.BackBufferFormat = D3DFMT_A8R8G8B8;
	pp.hDeviceWindow = window;
	pp.EnableAutoDepthStencil = TRUE;
	pp.AutoDepthStencilFormat = D3DFMT_D24S8;

	Replayer replay{};
	if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
	                             D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &replay.device))) {
		std::printf("CreateDevice failed\n");
		return 2;
	}
	replay.sizes = sizes;

	D3DADAPTER_IDENTIFIER9 who{};
	d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &who);
	std::printf("driver: %s | %s\n", who.Driver, who.Description);

	IDirect3DDevice9* dev = replay.device;
	dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &replay.backBuffer);
	dev->CreateTexture(kTarget, kTarget, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
	                   D3DPOOL_DEFAULT, &replay.captureTexture, nullptr);
	replay.captureTexture->GetSurfaceLevel(0, &replay.captureSurface);
	dev->CreateOffscreenPlainSurface(kTarget, kTarget, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
	                                 &replay.readback, nullptr);

	dev->SetRenderState(D3DRS_LIGHTING, FALSE);
	dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	dev->SetRenderState(D3DRS_ZENABLE, TRUE);

	if (FAILED(dev->CreateVertexDeclaration(kDeclaration, &replay.declaration))) {
		std::printf("CreateVertexDeclaration failed\n");
		return 2;
	}
	replay.shader = CompileShader(dev);

	// The static index patterns. Static like the game's own index buffers,
	// which the live watch showed are never dynamic and never locked.
	{
		static const UINT mods[4] = {4, 16, 64, 256};
		for (int k = 0; k < 4; ++k) {
			IDirect3DIndexBuffer9* ib = nullptr;
			if (FAILED(dev->CreateIndexBuffer(4096 * 2, D3DUSAGE_WRITEONLY,
			                                  D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib,
			                                  nullptr))) {
				std::printf("CreateIndexBuffer failed\n");
				return 2;
			}
			void* data = nullptr;
			ib->Lock(0, 0, &data, 0);
			unsigned short* indices = static_cast<unsigned short*>(data);
			for (UINT i = 0; i < 4096; ++i) {
				indices[i] = static_cast<unsigned short>(i % mods[k]);
			}
			ib->Unlock();
			replay.indexBuffers[k] = ib;
		}
	}
	for (UINT i = 0; i < 21 * 4; ++i) {
		replay.blockC10[i] = 0.25f * static_cast<float>(i % 7);
	}

	std::vector<unsigned char> passOne(kTarget * kTarget * 4);
	std::vector<unsigned char> passTwo(kTarget * kTarget * 4);

	auto readbackInto = [&](std::vector<unsigned char>& out) {
		if (FAILED(dev->GetRenderTargetData(replay.backBuffer, replay.readback))) {
			std::printf("GetRenderTargetData failed\n");
			std::exit(2);
		}
		D3DLOCKED_RECT rect{};
		replay.readback->LockRect(&rect, nullptr, D3DLOCK_READONLY);
		for (UINT y = 0; y < kTarget; ++y) {
			std::memcpy(&out[y * kTarget * 4],
			            static_cast<unsigned char*>(rect.pBits) + y * rect.Pitch,
			            kTarget * 4);
		}
		replay.readback->UnlockRect();
	};

	// Slice the timeline at its markers so each pass can be replayed as a
	// unit, more than once when the repeat factor asks for pressure.
	std::vector<Event> passEvents;
	std::vector<Event> betweenEvents;
	{
		int phase = 0;
		for (const Event& e : events) {
			if (e.kind == 'M') {
				if (e.label == "between the passes") {
					phase = 1;
				} else if (e.label == "second pass begins") {
					phase = 2;
				} else if (e.label == "frame ends") {
					phase = 3;
				}
				continue;
			}
			if (phase == 0) {
				passEvents.push_back(e);
			} else if (phase == 1) {
				betweenEvents.push_back(e);
			}
			// The recorded second pass is event-for-event identical to the
			// first (diff proved it), so the first pass's slice stands in
			// for both and the repeats stay trivially symmetric.
		}
	}

	const int frames = 10;
	int worstDiff = 0;
	for (int frame = 0; frame < frames; ++frame) {
		dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
		dev->BeginScene();
		for (int r = 0; r < repeat; ++r) {
			for (const Event& e : passEvents) {
				replay.RunEvent(e);
			}
		}
		// End of the first pass: capture it the way OBVR's CaptureEye does,
		// then read it back for comparison.
		dev->EndScene();
		dev->StretchRect(replay.backBuffer, nullptr, replay.captureSurface, nullptr,
		                 D3DTEXF_NONE);
		readbackInto(passOne);
		dev->BeginScene();
		// The between-window traffic (the HUD buffer) replays as-is; its
		// draws land on the first pass's picture after the capture, which
		// is exactly where the live frame put them.
		for (const Event& e : betweenEvents) {
			replay.RunEvent(e);
		}
		dev->EndScene();
		dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
		dev->BeginScene();
		for (int r = 0; r < repeat; ++r) {
			for (const Event& e : passEvents) {
				replay.RunEvent(e);
			}
		}
		dev->EndScene();
		readbackInto(passTwo);
		dev->Present(nullptr, nullptr, nullptr, nullptr);

		int diff = 0;
		for (size_t i = 0; i < passOne.size(); i += 4) {
			if (passOne[i] != passTwo[i] || passOne[i + 1] != passTwo[i + 1] ||
			    passOne[i + 2] != passTwo[i + 2]) {
				++diff;
			}
		}
		if (diff > worstDiff) {
			worstDiff = diff;
		}
		std::printf("frame %d: %d pixels differ between the passes\n", frame, diff);
	}

	std::printf(worstDiff == 0
	                ? "IDENTICAL: both passes rendered the same picture every frame\n"
	                : "DIVERGED: worst frame had %d differing pixels\n",
	            worstDiff);
	return worstDiff == 0 ? 0 : 1;
}
