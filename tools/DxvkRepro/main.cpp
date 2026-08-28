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
// vertex count to exactly 72 bytes. XYZ + normal + three 4D texcoords is
// 12 + 12 + 48 = 72, which lets fixed function draw the same layout.
constexpr UINT kStride = 72;
constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX3 | D3DFVF_TEXCOORDSIZE4(0) |
                       D3DFVF_TEXCOORDSIZE4(1) | D3DFVF_TEXCOORDSIZE4(2);

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

struct Replayer {
	IDirect3DDevice9* device = nullptr;
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
			device->SetFVF(kFvf);
			const UINT startVertex = lastLockOffset[e.buffer] / kStride;
			const UINT prims = e.b >= 3 ? (e.b / 3 < e.c ? e.b / 3 : e.c) : 0;
			if (prims > 0) {
				device->DrawPrimitive(D3DPT_TRIANGLELIST, startVertex, prims);
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
	dev->SetRenderState(D3DRS_ZENABLE, FALSE);

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
		dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
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
		dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
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
