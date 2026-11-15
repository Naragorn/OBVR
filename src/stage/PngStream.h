#pragma once

// A PNG written a row at a time (the dome's atlas, stage/DomeBuild.h): eight
// bits a channel, red green blue, never the whole picture in memory - the
// atlas is 100 MB unpacked and is made inside a 32-bit game. The layout is
// the PNG specification's (W3C, "Portable Network Graphics", third edition):
// the signature, IHDR, the zlib stream cut into IDAT chunks, IEND. A row is
// stored as it is, or as its differences to the pixel to the left or to the
// row above, whichever sums smallest (the specification's own heuristic, 12.8).
// The packing is miniz' (third_party/miniz).

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "miniz.h"

namespace obvr::stage {

class PngStream {
public:
	using Sink = std::function<bool(const void* bytes, std::size_t count)>;

	// level: 1 (fast) to 9 (small), zlib's.
	bool Open(Sink sink, int width, int height, int level) {
		m_sink = std::move(sink);
		m_rowsLeft = height;
		m_packer.reset(new (std::nothrow) tdefl_compressor);
		if (!m_sink || width <= 0 || height <= 0 || !m_packer) {
			return false;
		}
		const std::size_t rowBytes = static_cast<std::size_t>(width) * 3;
		m_last.assign(rowBytes, 0);
		m_filtered.assign(rowBytes + 1, 0);
		m_candidate.assign(rowBytes + 1, 0);
		static const std::uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
		std::uint8_t header[13];
		PutU32(header, static_cast<std::uint32_t>(width));
		PutU32(header + 4, static_cast<std::uint32_t>(height));
		header[8] = 8;   // bits a channel
		header[9] = 2;   // truecolour
		header[10] = 0;  // deflate
		header[11] = 0;  // the five filters
		header[12] = 0;  // not interlaced
		const int flags = static_cast<int>(tdefl_create_comp_flags_from_zip_params(level, 15, MZ_DEFAULT_STRATEGY));
		m_ok = m_sink(kSignature, sizeof(kSignature)) && WriteChunk("IHDR", header, sizeof(header)) &&
		       tdefl_init(m_packer.get(), &PngStream::Packed, this, flags) == TDEFL_STATUS_OKAY;
		return m_ok;
	}

	// The next row from the top, width * 3 bytes.
	bool WriteRow(const std::uint8_t* rgb) {
		if (!m_ok || m_rowsLeft <= 0) {
			return false;
		}
		ChooseFilter(rgb);
		m_last.assign(rgb, rgb + m_last.size());
		--m_rowsLeft;
		m_ok = tdefl_compress_buffer(m_packer.get(), m_filtered.data(), m_filtered.size(), TDEFL_NO_FLUSH) ==
		       TDEFL_STATUS_OKAY;
		return m_ok;
	}

	// After the last row: the stream's end and IEND. False when a row is
	// missing or a write failed.
	bool Close() {
		if (!m_ok || m_rowsLeft != 0) {
			return false;
		}
		m_ok = tdefl_compress_buffer(m_packer.get(), nullptr, 0, TDEFL_FINISH) == TDEFL_STATUS_DONE && FlushIdat() &&
		       WriteChunk("IEND", nullptr, 0);
		const bool done = m_ok;
		m_ok = false;
		return done;
	}

private:
	static void PutU32(std::uint8_t* out, std::uint32_t v) {
		out[0] = static_cast<std::uint8_t>(v >> 24);
		out[1] = static_cast<std::uint8_t>(v >> 16);
		out[2] = static_cast<std::uint8_t>(v >> 8);
		out[3] = static_cast<std::uint8_t>(v);
	}

	bool WriteChunk(const char* type, const std::uint8_t* data, std::size_t size) {
		std::uint8_t head[8];
		PutU32(head, static_cast<std::uint32_t>(size));
		for (int i = 0; i < 4; ++i) {
			head[4 + i] = static_cast<std::uint8_t>(type[i]);
		}
		mz_ulong crc = mz_crc32(MZ_CRC32_INIT, head + 4, 4);
		if (size > 0) {
			crc = mz_crc32(crc, data, size);
		}
		std::uint8_t tail[4];
		PutU32(tail, static_cast<std::uint32_t>(crc));
		return m_sink(head, sizeof(head)) && (size == 0 || m_sink(data, size)) && m_sink(tail, sizeof(tail));
	}

	bool FlushIdat() {
		if (m_idat.empty()) {
			return true;
		}
		const bool ok = WriteChunk("IDAT", m_idat.data(), m_idat.size());
		m_idat.clear();
		return ok;
	}

	// miniz hands the packed bytes here; an IDAT chunk goes out every 64 KB.
	static mz_bool Packed(const void* bytes, int count, void* self) {
		PngStream& png = *static_cast<PngStream*>(self);
		const std::uint8_t* from = static_cast<const std::uint8_t*>(bytes);
		png.m_idat.insert(png.m_idat.end(), from, from + count);
		return png.m_idat.size() < 65536 || png.FlushIdat() ? MZ_TRUE : MZ_FALSE;
	}

	// One filter's row into out (the filter's byte first), its cost returned.
	std::uint32_t Filter(int type, const std::uint8_t* rgb, std::uint8_t* out) const {
		out[0] = static_cast<std::uint8_t>(type);
		std::uint32_t cost = 0;
		const std::size_t count = m_last.size();
		for (std::size_t i = 0; i < count; ++i) {
			const std::uint8_t before = type == 1 ? (i >= 3 ? rgb[i - 3] : 0) : type == 2 ? m_last[i] : 0;
			const std::uint8_t value = static_cast<std::uint8_t>(rgb[i] - before);
			out[i + 1] = value;
			cost += value < 128 ? value : 256u - value;
		}
		return cost;
	}

	// The cheapest of none (0), left (1) and above (2) into m_filtered.
	void ChooseFilter(const std::uint8_t* rgb) {
		std::uint32_t best = Filter(0, rgb, m_filtered.data());
		for (int type = 1; type <= 2; ++type) {
			const std::uint32_t cost = Filter(type, rgb, m_candidate.data());
			if (cost < best) {
				best = cost;
				m_filtered.swap(m_candidate);
			}
		}
	}

	Sink m_sink;
	std::unique_ptr<tdefl_compressor> m_packer;
	std::vector<std::uint8_t> m_last;       // the row above, zero for the first
	std::vector<std::uint8_t> m_filtered;   // the row as it goes to the packer
	std::vector<std::uint8_t> m_candidate;
	std::vector<std::uint8_t> m_idat;
	int m_rowsLeft = 0;
	bool m_ok = false;
};

}  // namespace obvr::stage
