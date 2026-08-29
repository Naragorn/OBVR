#include "render/LayoutProbe.h"

#include "render/D3D11Types.h"
#include "render/D3D9Types.h"

namespace obvr::render {

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
