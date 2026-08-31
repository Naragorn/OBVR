#include "ui/MenuCanvas.h"

#include "ui/MenuFont.h"

namespace obvr::ui {

void Canvas::SetPixel(SInt32 x, SInt32 y, render::Pixel colour) {
	if (m_pixels == nullptr) {
		return;
	}
	if (x < 0 || y < 0) {
		return;
	}
	if (static_cast<UInt32>(x) >= m_width || static_cast<UInt32>(y) >= m_height) {
		return;
	}

	m_pixels[static_cast<UInt32>(y) * m_width + static_cast<UInt32>(x)] = colour;
}

render::Pixel Canvas::GetPixel(SInt32 x, SInt32 y) const {
	if (m_pixels == nullptr || x < 0 || y < 0) {
		return render::Pixel{0, 0, 0, 0};
	}
	if (static_cast<UInt32>(x) >= m_width || static_cast<UInt32>(y) >= m_height) {
		return render::Pixel{0, 0, 0, 0};
	}

	return m_pixels[static_cast<UInt32>(y) * m_width + static_cast<UInt32>(x)];
}

void Canvas::Fill(render::Pixel colour) {
	if (m_pixels == nullptr) {
		return;
	}

	const UInt32 count = m_width * m_height;
	for (UInt32 at = 0; at < count; ++at) {
		m_pixels[at] = colour;
	}
}

void Canvas::FillRect(SInt32 x, SInt32 y, SInt32 width, SInt32 height, render::Pixel colour) {
	if (width <= 0 || height <= 0) {
		return;
	}

	// Clipped here rather than per pixel. Both give the same picture, but this
	// one does not walk the length of a rectangle that is entirely off the
	// canvas, and a value bar at the far end of its range is exactly that.
	SInt32 left = x;
	SInt32 top = y;
	SInt32 right = x + width;
	SInt32 bottom = y + height;

	if (left < 0) {
		left = 0;
	}
	if (top < 0) {
		top = 0;
	}
	if (right > static_cast<SInt32>(m_width)) {
		right = static_cast<SInt32>(m_width);
	}
	if (bottom > static_cast<SInt32>(m_height)) {
		bottom = static_cast<SInt32>(m_height);
	}

	for (SInt32 row = top; row < bottom; ++row) {
		for (SInt32 column = left; column < right; ++column) {
			SetPixel(column, row, colour);
		}
	}
}

void Canvas::DrawFrame(SInt32 x, SInt32 y, SInt32 width, SInt32 height, SInt32 thickness,
                       render::Pixel colour) {
	if (width <= 0 || height <= 0 || thickness <= 0) {
		return;
	}

	// Four bars rather than a loop with a test in it. A frame thicker than the
	// box is half the box drawn twice, which is what it should look like -
	// filled - rather than an empty rectangle or nothing at all.
	FillRect(x, y, width, thickness, colour);
	FillRect(x, y + height - thickness, width, thickness, colour);
	FillRect(x, y, thickness, height, colour);
	FillRect(x + width - thickness, y, thickness, height, colour);
}

void Canvas::DrawText(SInt32 x, SInt32 y, const char* text, SInt32 scale, render::Pixel colour) {
	if (text == nullptr || scale <= 0) {
		return;
	}

	SInt32 penX = x;
	for (const char* at = text; *at != '\0'; ++at) {
		for (UInt32 row = 0; row < kGlyphHeight; ++row) {
			for (UInt32 column = 0; column < kGlyphWidth; ++column) {
				if (!GlyphPixel(*at, column, row)) {
					continue;
				}

				// One font pixel is a square block, never a stretched one -
				// see the note on scale in the header.
				FillRect(penX + static_cast<SInt32>(column) * scale,
				         y + static_cast<SInt32>(row) * scale, scale, scale, colour);
			}
		}

		penX += static_cast<SInt32>(kGlyphAdvance) * scale;
	}
}

}  // namespace obvr::ui
