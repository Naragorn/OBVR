#include "render/LayoutProbe.h"

#include "platform/Win32Min.h"
#include "render/D3D11Types.h"
#include "render/D3D9Types.h"

namespace obvr::render {
namespace {

// One output row of the quarter-scale dump, plus BMP row padding. The frame
// is at most ~4100 wide, so a quarter of it fits with room to spare.
UInt8 g_dumpRow[4096];

void PutU32(UInt8* at, UInt32 value) {
	at[0] = static_cast<UInt8>(value);
	at[1] = static_cast<UInt8>(value >> 8);
	at[2] = static_cast<UInt8>(value >> 16);
	at[3] = static_cast<UInt8>(value >> 24);
}

}  // namespace

bool PixelCovered(UInt32 pixel, bool byAlpha) {
	if (byAlpha) {
		return (pixel >> 24) != 0;
	}
	return (pixel & 0x00F0F0F0u) != 0;
}

void AccumulateCoveredRow(const UInt32* row, UInt32 width, UInt32 y, bool byAlpha,
                          CoveredRect& box) {
	for (UInt32 x = 0; x < width; ++x) {
		if (!PixelCovered(row[x], byAlpha)) {
			continue;
		}
		if (box.covered == 0) {
			box.minX = x;
			box.maxX = x;
			box.minY = y;
			box.maxY = y;
		} else {
			if (x < box.minX) {
				box.minX = x;
			}
			if (x > box.maxX) {
				box.maxX = x;
			}
			// Rows arrive top to bottom, so y only ever grows.
			box.maxY = y;
		}
		++box.covered;
	}
}

bool MeasureCoveredRect(void* gameDevice, void* surface, UInt32 width, UInt32 height,
                        UInt32 format, bool byAlpha, CoveredRect& out) {
	out = CoveredRect{};
	if (gameDevice == nullptr || surface == nullptr || width == 0 || height == 0) {
		return false;
	}

	// The same route the HUD content dump takes: a render target cannot be
	// read by the CPU, GetRenderTargetData copies it into system memory.
	auto createPlain = d3d9::Method<d3d9::CreateOffscreenPlainSurfaceFn>(
		gameDevice, d3d9::kDeviceCreateOffscreenPlainSurface);
	auto getData = d3d9::Method<d3d9::GetRenderTargetDataFn>(
		gameDevice, d3d9::kDeviceGetRenderTargetData);
	if (createPlain == nullptr || getData == nullptr) {
		return false;
	}

	void* staging = nullptr;
	if (d3d11::Failed(createPlain(gameDevice, width, height, format, d3d9::kPoolSystemMem,
	                              &staging, nullptr)) ||
	    staging == nullptr) {
		return false;
	}

	if (d3d11::Failed(getData(gameDevice, surface, staging))) {
		d3d11::Release(staging);
		return false;
	}

	auto lockRect = d3d9::Method<d3d9::LockRectFn>(staging, d3d9::kSurfaceLockRect);
	auto unlockRect = d3d9::Method<d3d9::UnlockRectFn>(staging, d3d9::kSurfaceUnlockRect);
	d3d9::LockedRect locked{};
	if (lockRect == nullptr || unlockRect == nullptr ||
	    d3d11::Failed(lockRect(staging, &locked, nullptr, d3d9::kLockReadOnly)) ||
	    locked.bits == nullptr) {
		d3d11::Release(staging);
		return false;
	}

	for (UInt32 y = 0; y < height; ++y) {
		const auto* row = reinterpret_cast<const UInt32*>(
			static_cast<const UInt8*>(locked.bits) + static_cast<SInt32>(y) * locked.pitch);
		AccumulateCoveredRow(row, width, y, byAlpha, out);
	}

	unlockRect(staging);
	d3d11::Release(staging);
	return true;
}

bool DumpSurfaceBmp(void* gameDevice, void* surface, UInt32 width, UInt32 height,
                    UInt32 format, const char* path) {
	if (gameDevice == nullptr || surface == nullptr || width < 4 || height < 4) {
		return false;
	}

	auto createPlain = d3d9::Method<d3d9::CreateOffscreenPlainSurfaceFn>(
		gameDevice, d3d9::kDeviceCreateOffscreenPlainSurface);
	auto getData = d3d9::Method<d3d9::GetRenderTargetDataFn>(
		gameDevice, d3d9::kDeviceGetRenderTargetData);
	if (createPlain == nullptr || getData == nullptr) {
		return false;
	}

	void* staging = nullptr;
	if (d3d11::Failed(createPlain(gameDevice, width, height, format, d3d9::kPoolSystemMem,
	                              &staging, nullptr)) ||
	    staging == nullptr) {
		return false;
	}
	if (d3d11::Failed(getData(gameDevice, surface, staging))) {
		d3d11::Release(staging);
		return false;
	}

	auto lockRect = d3d9::Method<d3d9::LockRectFn>(staging, d3d9::kSurfaceLockRect);
	auto unlockRect = d3d9::Method<d3d9::UnlockRectFn>(staging, d3d9::kSurfaceUnlockRect);
	d3d9::LockedRect locked{};
	if (lockRect == nullptr || unlockRect == nullptr ||
	    d3d11::Failed(lockRect(staging, &locked, nullptr, d3d9::kLockReadOnly)) ||
	    locked.bits == nullptr) {
		d3d11::Release(staging);
		return false;
	}

	const UInt32 outWidth = width / 4;
	const UInt32 outHeight = height / 4;
	const UInt32 rowBytes = (outWidth * 3 + 3) & ~3u;
	const UInt32 imageBytes = rowBytes * outHeight;

	bool ok = false;
	HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
	                          FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file != InvalidHandle()) {
		UInt8 header[54] = {};
		header[0] = 'B';
		header[1] = 'M';
		PutU32(header + 2, 54 + imageBytes);
		PutU32(header + 10, 54);
		PutU32(header + 14, 40);
		PutU32(header + 18, outWidth);
		PutU32(header + 22, outHeight);
		header[26] = 1;
		header[28] = 24;
		PutU32(header + 34, imageBytes);
		DWORD written = 0;
		ok = WriteFile(file, header, sizeof(header), &written, nullptr) != 0;

		// BMP rows run bottom-up; every fourth source pixel of every fourth
		// source row, colour only - the picture is the question, not the
		// alpha.
		for (UInt32 outY = 0; ok && outY < outHeight; ++outY) {
			const UInt32 sourceY = (outHeight - 1 - outY) * 4;
			const auto* row = reinterpret_cast<const UInt32*>(
				static_cast<const UInt8*>(locked.bits) +
				static_cast<SInt32>(sourceY) * locked.pitch);
			for (UInt32 outX = 0; outX < outWidth; ++outX) {
				const UInt32 pixel = row[outX * 4];
				g_dumpRow[outX * 3 + 0] = static_cast<UInt8>(pixel);
				g_dumpRow[outX * 3 + 1] = static_cast<UInt8>(pixel >> 8);
				g_dumpRow[outX * 3 + 2] = static_cast<UInt8>(pixel >> 16);
			}
			for (UInt32 pad = outWidth * 3; pad < rowBytes; ++pad) {
				g_dumpRow[pad] = 0;
			}
			ok = WriteFile(file, g_dumpRow, rowBytes, &written, nullptr) != 0;
		}
		CloseHandle(file);
	}

	unlockRect(staging);
	d3d11::Release(staging);
	return ok;
}

bool DumpBackBufferBmp(void* gameDevice, const char* path) {
	if (gameDevice == nullptr) {
		return false;
	}
	auto getBackBuffer =
		d3d9::Method<d3d9::GetBackBufferFn>(gameDevice, d3d9::kDeviceGetBackBuffer);
	if (getBackBuffer == nullptr) {
		return false;
	}
	void* surface = nullptr;
	if (d3d11::Failed(getBackBuffer(gameDevice, 0, 0, d3d9::kBackBufferTypeMono, &surface)) ||
	    surface == nullptr) {
		return false;
	}
	auto getDesc = d3d9::Method<d3d9::GetDescFn>(surface, d3d9::kSurfaceGetDesc);
	d3d9::SurfaceDesc desc{};
	bool ok = getDesc != nullptr && !d3d11::Failed(getDesc(surface, &desc));
	if (ok) {
		ok = DumpSurfaceBmp(gameDevice, surface, desc.width, desc.height, desc.format, path);
	}
	d3d11::Release(surface);
	return ok;
}

bool MeasureBackBufferCoveredRect(void* gameDevice, UInt32& widthOut, UInt32& heightOut,
                                  CoveredRect& out) {
	if (gameDevice == nullptr) {
		return false;
	}

	auto getBackBuffer =
		d3d9::Method<d3d9::GetBackBufferFn>(gameDevice, d3d9::kDeviceGetBackBuffer);
	if (getBackBuffer == nullptr) {
		return false;
	}
	void* surface = nullptr;
	if (d3d11::Failed(getBackBuffer(gameDevice, 0, 0, d3d9::kBackBufferTypeMono, &surface)) ||
	    surface == nullptr) {
		return false;
	}

	auto getDesc = d3d9::Method<d3d9::GetDescFn>(surface, d3d9::kSurfaceGetDesc);
	d3d9::SurfaceDesc desc{};
	bool ok = getDesc != nullptr && !d3d11::Failed(getDesc(surface, &desc));
	if (ok) {
		widthOut = desc.width;
		heightOut = desc.height;
		// The buffer's own format, because GetRenderTargetData insists the
		// staging copy match it - and colour, not alpha, because the back
		// buffer's alpha channel means nothing.
		ok = MeasureCoveredRect(gameDevice, surface, desc.width, desc.height, desc.format,
		                        false, out);
	}

	d3d11::Release(surface);
	return ok;
}

}  // namespace obvr::render
