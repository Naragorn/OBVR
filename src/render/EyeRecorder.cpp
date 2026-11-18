#include "render/EyeRecorder.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

#include "core/Log.h"
#include "platform/Win32Min.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"
#include "render/HeadsetRenderer.h"

namespace obvr::render {
namespace {

// The writer's buffers: enough for the lag and a slow disk at a few frames.
constexpr UInt32 kWriterBuffers = 8;
constexpr int kFree = 0;
constexpr int kFilled = 1;
constexpr int kWriting = 2;

struct WriterBuffer {
	std::atomic<int> state{kFree};
	char path[128] = "";
	UInt32 width = 0;
	UInt32 height = 0;
	std::vector<UInt8> rows;  // bottom-up BGR rows, each padded to 4 bytes
};

struct Recorder {
	void* device = nullptr;
	UInt32 format = 0;
	test::RecordCut cut;
	UInt32 steps = 0;
	UInt32 stepWidth[test::kRecordMaxSteps] = {};
	UInt32 stepHeight[test::kRecordMaxSteps] = {};
	void* stepTexture[test::kRecordMaxSteps] = {};
	void* stepSurface[test::kRecordMaxSteps] = {};
	void* readback[test::kRecordSlots] = {};
	char readbackName[test::kRecordSlots][96] = {};
	test::RecordSlots slots;
	test::RecordRun run;
	UInt32 frame = 0;
	bool broken = false;  // resources refused once: no more tries this session
};

// The recorder's one state, owned here: the render thread's side and the
// writer's buffers it shares with the writer thread.
struct RecorderState {
	Recorder recorder;
	WriterBuffer buffers[kWriterBuffers];
	std::atomic<UInt32> written{0};
	std::atomic<UInt32> writeFailed{0};
	EyeRecorderStats stats;
	bool writerStarted = false;
};

RecorderState& State() {
	static RecorderState s;
	return s;
}

void PutU32(UInt8* at, UInt32 v) {
	at[0] = static_cast<UInt8>(v);
	at[1] = static_cast<UInt8>(v >> 8);
	at[2] = static_cast<UInt8>(v >> 16);
	at[3] = static_cast<UInt8>(v >> 24);
}

bool WriteBmp(const WriterBuffer& b) {
	const UInt32 rowBytes = (b.width * 3 + 3) & ~3u;
	const UInt32 imageBytes = rowBytes * b.height;
	HANDLE file = CreateFileA(b.path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
	                          nullptr);
	if (file == InvalidHandle()) {
		return false;
	}
	UInt8 header[54] = {};
	header[0] = 'B';
	header[1] = 'M';
	PutU32(header + 2, 54 + imageBytes);
	PutU32(header + 10, 54);
	PutU32(header + 14, 40);
	PutU32(header + 18, b.width);
	PutU32(header + 22, b.height);
	header[26] = 1;
	header[28] = 24;
	PutU32(header + 34, imageBytes);
	DWORD written = 0;
	bool ok = WriteFile(file, header, sizeof(header), &written, nullptr) != 0;
	ok = ok && WriteFile(file, b.rows.data(), imageBytes, &written, nullptr) != 0 && written == imageBytes;
	CloseHandle(file);
	return ok;
}

DWORD __stdcall WriterMain(void*) {
	for (;;) {
		bool any = false;
		for (WriterBuffer& b : State().buffers) {
			int filled = kFilled;
			if (!b.state.compare_exchange_strong(filled, kWriting)) {
				continue;
			}
			any = true;
			if (WriteBmp(b)) {
				State().written.fetch_add(1);
			} else {
				State().writeFailed.fetch_add(1);
			}
			b.state.store(kFree);
		}
		if (!any) {
			Sleep(2);
		}
	}
}

void ReleaseResources(Recorder& r) {
	for (UInt32 i = 0; i < test::kRecordMaxSteps; ++i) {
		d3d11::Release(r.stepSurface[i]);
		d3d11::Release(r.stepTexture[i]);
	}
	for (void*& s : r.readback) {
		d3d11::Release(s);
	}
	r.slots = test::RecordSlots{};
	r.steps = 0;
	r.device = nullptr;
}

bool SameCut(const test::RecordCut& a, const test::RecordCut& b) {
	return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom &&
	       a.outWidth == b.outWidth && a.outHeight == b.outHeight;
}

// The shrinking chain and the read-back surfaces for this cut, made again
// when the device, the format or the cut changes.
bool Ensure(Recorder& r, void* device, UInt32 format, const test::RecordCut& cut) {
	if (r.device == device && r.format == format && r.steps > 0 && SameCut(r.cut, cut)) {
		return true;
	}
	ReleaseResources(r);
	auto createTexture = d3d9::Method<d3d9::CreateTextureFn>(device, d3d9::kDeviceCreateTexture);
	auto createPlain =
		d3d9::Method<d3d9::CreateOffscreenPlainSurfaceFn>(device, d3d9::kDeviceCreateOffscreenPlainSurface);
	if (createTexture == nullptr || createPlain == nullptr) {
		return false;
	}
	r.steps = test::PlanRecordSteps(static_cast<UInt32>(cut.right - cut.left), static_cast<UInt32>(cut.bottom - cut.top),
	                                cut.outWidth, cut.outHeight, r.stepWidth, r.stepHeight);
	for (UInt32 i = 0; i < r.steps; ++i) {
		if (d3d11::Failed(createTexture(device, r.stepWidth[i], r.stepHeight[i], 1, d3d9::kUsageRenderTarget, format,
		                                d3d9::kPoolDefault, &r.stepTexture[i], nullptr)) ||
		    r.stepTexture[i] == nullptr) {
			ReleaseResources(r);
			return false;
		}
		auto level = d3d9::Method<d3d9::GetSurfaceLevelFn>(r.stepTexture[i], d3d9::kTextureGetSurfaceLevel);
		if (level == nullptr || d3d11::Failed(level(r.stepTexture[i], 0, &r.stepSurface[i])) ||
		    r.stepSurface[i] == nullptr) {
			ReleaseResources(r);
			return false;
		}
	}
	for (void*& s : r.readback) {
		if (d3d11::Failed(createPlain(device, cut.outWidth, cut.outHeight, format, d3d9::kPoolSystemMem, &s, nullptr)) ||
		    s == nullptr) {
			ReleaseResources(r);
			return false;
		}
	}
	for (WriterBuffer& b : State().buffers) {
		if (b.state.load() == kFree) {
			b.rows.assign(((cut.outWidth * 3 + 3) & ~3u) * cut.outHeight, 0);
		}
	}
	r.device = device;
	r.format = format;
	r.cut = cut;
	return true;
}

// The cut shrunk step by step into the last step's surface, then copied to
// the slot's system-memory surface (the copy is the GPU's until read).
bool Film(Recorder& r, void* device, void* eye, UInt32 slot) {
	auto stretch = d3d9::Method<d3d9::StretchRectFn>(device, d3d9::kDeviceStretchRect);
	auto getData = d3d9::Method<d3d9::GetRenderTargetDataFn>(device, d3d9::kDeviceGetRenderTargetData);
	if (stretch == nullptr || getData == nullptr) {
		return false;
	}
	const d3d9::Rect source{r.cut.left, r.cut.top, r.cut.right, r.cut.bottom};
	void* from = eye;
	const d3d9::Rect* fromRect = &source;
	for (UInt32 i = 0; i < r.steps; ++i) {
		if (d3d11::Failed(stretch(device, from, fromRect, r.stepSurface[i], nullptr, d3d9::kTexFilterLinear))) {
			return false;
		}
		from = r.stepSurface[i];
		fromRect = nullptr;
	}
	return !d3d11::Failed(getData(device, r.stepSurface[r.steps - 1], r.readback[slot]));
}

// A finished copy into a free writer buffer; false when none is free (the
// picture is dropped) or the lock is refused.
bool HandOver(Recorder& r, UInt32 slot, bool& noBuffer) {
	noBuffer = false;
	WriterBuffer* target = nullptr;
	for (WriterBuffer& b : State().buffers) {
		if (b.state.load() == kFree) {
			target = &b;
			break;
		}
	}
	if (target == nullptr) {
		noBuffer = true;
		return false;
	}
	void* surface = r.readback[slot];
	auto lockRect = d3d9::Method<d3d9::LockRectFn>(surface, d3d9::kSurfaceLockRect);
	auto unlockRect = d3d9::Method<d3d9::UnlockRectFn>(surface, d3d9::kSurfaceUnlockRect);
	d3d9::LockedRect locked{};
	if (lockRect == nullptr || unlockRect == nullptr ||
	    d3d11::Failed(lockRect(surface, &locked, nullptr, d3d9::kLockReadOnly)) || locked.bits == nullptr) {
		return false;
	}
	const UInt32 w = r.cut.outWidth;
	const UInt32 h = r.cut.outHeight;
	const UInt32 rowBytes = (w * 3 + 3) & ~3u;
	target->rows.resize(rowBytes * h);
	for (UInt32 y = 0; y < h; ++y) {
		const auto* src = reinterpret_cast<const UInt32*>(static_cast<const UInt8*>(locked.bits) +
		                                                 static_cast<SInt32>(h - 1 - y) * locked.pitch);
		UInt8* dst = target->rows.data() + y * rowBytes;
		for (UInt32 x = 0; x < w; ++x) {
			const UInt32 p = src[x];
			dst[x * 3 + 0] = static_cast<UInt8>(p);
			dst[x * 3 + 1] = static_cast<UInt8>(p >> 8);
			dst[x * 3 + 2] = static_cast<UInt8>(p >> 16);
		}
		for (UInt32 pad = w * 3; pad < rowBytes; ++pad) {
			dst[pad] = 0;
		}
	}
	unlockRect(surface);
	std::snprintf(target->path, sizeof(target->path), "%s", r.readbackName[slot]);
	target->width = w;
	target->height = h;
	target->state.store(kFilled);
	return true;
}

}  // namespace

bool StepEyeRecorder(void* gameDevice, void* eyeSurface, UInt32 width, UInt32 height, UInt32 format,
                     const test::RecordSettings& settings, bool marked, const char* markName, float dt) {
	Recorder& r = State().recorder;
	++r.frame;
	// What the GPU finished first: the slots copied kRecordLagFrames ago.
	for (UInt32 slot = 0; slot < test::kRecordSlots; ++slot) {
		if (!test::RecordSlotReady(r.slots, slot, r.frame)) {
			continue;
		}
		bool noBuffer = false;
		if (HandOver(r, slot, noBuffer)) {
			r.slots.busy[slot] = false;
		} else if (noBuffer) {
			// kept: tried again next frame, the writer may have caught up
		} else {
			r.slots.busy[slot] = false;
			++State().stats.failed;
		}
	}
	char name[96];
	if (!test::StepRecordRun(r.run, settings, marked, markName, dt, name, sizeof(name)) || r.broken ||
	    gameDevice == nullptr || eyeSurface == nullptr) {
		return false;
	}
	if (!State().writerStarted) {
		State().writerStarted = true;
		HANDLE thread = CreateThread(nullptr, 0, &WriterMain, nullptr, 0, nullptr);
		if (thread == nullptr) {
			r.broken = true;
			OBVR_LOG("Record: no thread to write the film on - the recording is off for this session");
			return false;
		}
		CloseHandle(thread);
	}
	const test::RecordCut cut = test::PlanRecordCut(width, height, settings);
	if (!cut.valid || !Ensure(r, gameDevice, format, cut)) {
		r.broken = true;
		OBVR_LOG("Record: the film's surfaces were refused (eye %ux%u, format %u, film %ux%u) - the recording is off "
		         "for this session",
		         width, height, format, cut.outWidth, cut.outHeight);
		return false;
	}
	const UInt32 slot = test::FreeRecordSlot(r.slots);
	if (slot == test::kRecordSlots) {
		++State().stats.skippedSlots;
		return false;
	}
	if (!Film(r, gameDevice, eyeSurface, slot)) {
		++State().stats.failed;
		return false;
	}
	r.slots.busy[slot] = true;
	r.slots.copiedAt[slot] = r.frame;
	std::snprintf(r.readbackName[slot], sizeof(r.readbackName[slot]), "%s", name);
	++State().stats.filmed;
	return true;
}

EyeRecorderStats EyeRecorderTotals() {
	EyeRecorderStats s = State().stats;
	s.written = State().written.load();
	s.failed += State().writeFailed.load();
	return s;
}

void EyeRecorderSize(UInt32& width, UInt32& height) {
	width = State().recorder.cut.outWidth;
	height = State().recorder.cut.outHeight;
}

void RecordRightEye(const HeadsetRenderer& renderer, void* gameDevice, const test::RecordSettings& settings,
                    bool marked, const char* markName, float dt) {
	if (!settings.enabled) {
		return;
	}
	void* surface = nullptr;
	UInt32 width = 0;
	UInt32 height = 0;
	UInt32 format = 0;
	const bool eye = renderer.EyeForRecording(1, surface, width, height, format);
	StepEyeRecorder(gameDevice, eye ? surface : nullptr, width, height, format, settings, marked, markName, dt);
	if (marked) {
		const EyeRecorderStats s = EyeRecorderTotals();
		OBVR_LOG("Record: at the mark %s - %s; so far %u pictures filmed, %u written, %u dropped (the writer behind), "
		         "%u refused; the film %ux%u, every %u. frame for %.1f s",
		         markName != nullptr ? markName : "?", eye ? "filming the right eye" : "no eye picture to film",
		         s.filmed, s.written, s.skippedSlots, s.failed, State().recorder.cut.outWidth, State().recorder.cut.outHeight,
		         settings.every > 0 ? settings.every : 1, static_cast<double>(settings.seconds));
	}
}

}  // namespace obvr::render
