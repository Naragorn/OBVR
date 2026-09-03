#include <cstdio>
#include <cstring>
#include <vector>

#include "render/CrosshairCache.h"

namespace {

using namespace obvr::render;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void CheckResult(CrosshairCacheResult actual, CrosshairCacheResult expected,
                 const char* what) {
	if (actual == expected) {
		std::printf("  ok    %s\n", what);
	} else {
		std::printf("  FAIL  %s: %s, expected %s\n", what,
		            CrosshairCacheResultName(actual), CrosshairCacheResultName(expected));
		++g_failures;
	}
}

class MemoryIo final : public CrosshairCacheIo {
public:
	void* OpenRead(const char*) override {
		offset = 0;
		return readOpens ? this : nullptr;
	}
	void* OpenWrite(const char*) override {
		offset = 0;
		writeCalls = 0;
		bytes.clear();
		return writeOpens ? this : nullptr;
	}
	bool Size(void*, UInt32& out) override {
		if (!sizeWorks) {
			return false;
		}
		out = reportedSizeSet ? reportedSize : static_cast<UInt32>(bytes.size());
		return true;
	}
	bool Read(void*, void* destination, UInt32 count) override {
		++readCalls;
		if (readCalls == failReadCall || offset + count > bytes.size()) {
			return false;
		}
		std::memcpy(destination, bytes.data() + offset, count);
		offset += count;
		return true;
	}
	bool Write(void*, const void* source, UInt32 count) override {
		++writeCalls;
		if (writeCalls == failWriteCall) {
			return false;
		}
		const auto* begin = static_cast<const UInt8*>(source);
		bytes.insert(bytes.end(), begin, begin + count);
		offset += count;
		return true;
	}
	bool Flush(void*) override { return flushWorks; }
	void Close(void*) override { ++closes; }
	bool Replace(const char*, const char*) override {
		++replaces;
		return replaceWorks;
	}
	void Remove(const char*) override { ++removes; }

	void ResetCalls() {
		offset = 0;
		readCalls = 0;
		writeCalls = 0;
		closes = 0;
		replaces = 0;
		removes = 0;
		failReadCall = -1;
		failWriteCall = -1;
		readOpens = true;
		writeOpens = true;
		sizeWorks = true;
		reportedSizeSet = false;
		flushWorks = true;
		replaceWorks = true;
	}

	std::vector<UInt8> bytes;
	size_t offset = 0;
	int readCalls = 0;
	int writeCalls = 0;
	int closes = 0;
	int replaces = 0;
	int removes = 0;
	int failReadCall = -1;
	int failWriteCall = -1;
	bool readOpens = true;
	bool writeOpens = true;
	bool sizeWorks = true;
	bool reportedSizeSet = false;
	UInt32 reportedSize = 0;
	bool flushWorks = true;
	bool replaceWorks = true;
};

std::vector<UInt8> Pixels(SInt32 pitch, UInt8 padding = 0) {
	std::vector<UInt8> pixels(static_cast<size_t>(pitch) * kCrosshairCacheHeight, padding);
	for (UInt32 y = 0; y < kCrosshairCacheHeight; ++y) {
		for (UInt32 x = 0; x < kCrosshairCacheRowBytes; ++x) {
			pixels[static_cast<size_t>(y) * pitch + x] =
				static_cast<UInt8>((x * 17u + y * 31u) & 0xFFu);
		}
	}
	return pixels;
}

void PutFile(MemoryIo& io, const CrosshairCacheHeader& header,
             const std::vector<UInt8>& pixels, SInt32 pitch) {
	io.bytes.resize(sizeof(header) + kCrosshairCachePixelBytes);
	std::memcpy(io.bytes.data(), &header, sizeof(header));
	for (UInt32 y = 0; y < kCrosshairCacheHeight; ++y) {
		std::memcpy(io.bytes.data() + sizeof(header) + y * kCrosshairCacheRowBytes,
		            pixels.data() + static_cast<size_t>(y) * pitch,
		            kCrosshairCacheRowBytes);
	}
	io.offset = 0;
	io.readCalls = 0;
}

void TestBufferAndChecksum() {
	std::printf("Pixel buffer and checksum\n");
	const SInt32 exact = static_cast<SInt32>(kCrosshairCacheRowBytes);
	const SInt32 padded = exact + 19;
	auto a = Pixels(exact);
	auto b = Pixels(padded, 0xA5);

	Check(!CrosshairCachePixelBufferValid(nullptr, exact), "a null pixel buffer is refused");
	Check(!CrosshairCachePixelBufferValid(a.data(), exact - 1), "a short pitch is refused");
	Check(CrosshairCachePixelBufferValid(a.data(), exact), "an exact pitch is accepted");
	Check(CrosshairCachePixelBufferValid(b.data(), padded), "a padded pitch is accepted");
	Check(CrosshairCacheChecksum(nullptr, exact) == 0, "an invalid buffer has no checksum");
	Check(CrosshairCacheChecksum(a.data(), exact) == CrosshairCacheChecksum(b.data(), padded),
	      "pitch padding is not persisted or hashed");
	b[0] ^= 1;
	Check(CrosshairCacheChecksum(a.data(), exact) != CrosshairCacheChecksum(b.data(), padded),
	      "a changed stored pixel changes the checksum");
}

void TestDecisions() {
	std::printf("One-shot load and save decisions\n");
	for (UInt32 mask = 0; mask < 8; ++mask) {
		const bool enabled = (mask & 1) != 0;
		const bool haveOrCaptured = (mask & 2) != 0;
		const bool tried = (mask & 4) != 0;
		Check(CrosshairCacheLoadWanted(enabled, haveOrCaptured, tried) ==
		          (enabled && !haveOrCaptured && !tried),
		      "every load gate combination follows the one-shot rule");
		Check(CrosshairCacheSaveWanted(enabled, haveOrCaptured, tried) ==
		          (enabled && haveOrCaptured && !tried),
		      "every save gate combination follows the one-shot rule");
	}
}

void TestHeaderValidation() {
	std::printf("Header validation\n");
	const UInt32 fileBytes = sizeof(CrosshairCacheHeader) + kCrosshairCachePixelBytes;
	const CrosshairCacheHeader valid = MakeCrosshairCacheHeader(0x12345678);
	CheckResult(ValidateCrosshairCacheHeader(valid, fileBytes), CrosshairCacheResult::Ok,
	            "the exact current header is accepted");
	const auto* magic = reinterpret_cast<const UInt8*>(&valid.magic);
	Check(magic[0] == 'O' && magic[1] == 'B' && magic[2] == 'X' && magic[3] == 'C',
	      "the file signature is literally OBXC on this little-endian target");

	CrosshairCacheHeader bad = valid;
	bad.magic ^= 1;
	CheckResult(ValidateCrosshairCacheHeader(bad, fileBytes), CrosshairCacheResult::WrongMagic,
	            "another file signature is refused");
	bad = valid; ++bad.version;
	CheckResult(ValidateCrosshairCacheHeader(bad, fileBytes), CrosshairCacheResult::WrongVersion,
	            "another cache version is refused");
	bad = valid; --bad.width;
	CheckResult(ValidateCrosshairCacheHeader(bad, fileBytes), CrosshairCacheResult::WrongDimensions,
	            "another width is refused");
	bad = valid; --bad.height;
	CheckResult(ValidateCrosshairCacheHeader(bad, fileBytes), CrosshairCacheResult::WrongDimensions,
	            "another height is refused");
	bad = valid; ++bad.format;
	CheckResult(ValidateCrosshairCacheHeader(bad, fileBytes), CrosshairCacheResult::WrongFormat,
	            "another pixel format is refused");
	bad = valid; --bad.rowBytes;
	CheckResult(ValidateCrosshairCacheHeader(bad, fileBytes), CrosshairCacheResult::WrongRowBytes,
	            "another row size is refused");
	bad = valid; --bad.pixelBytes;
	CheckResult(ValidateCrosshairCacheHeader(bad, fileBytes), CrosshairCacheResult::WrongPixelBytes,
	            "another payload size is refused");
	CheckResult(ValidateCrosshairCacheHeader(valid, fileBytes - 1),
	            CrosshairCacheResult::WrongFileSize, "a truncated file is refused");
	CheckResult(ValidateCrosshairCacheHeader(valid, fileBytes + 1),
	            CrosshairCacheResult::WrongFileSize, "trailing data is refused");
}

void TestReadFlows() {
	std::printf("Read flows\n");
	const SInt32 pitch = static_cast<SInt32>(kCrosshairCacheRowBytes) + 13;
	auto source = Pixels(pitch, 0x44);
	auto destination = Pixels(pitch, 0xCC);
	MemoryIo io;
	const CrosshairCacheHeader valid =
		MakeCrosshairCacheHeader(CrosshairCacheChecksum(source.data(), pitch));
	PutFile(io, valid, source, pitch);

	CheckResult(ReadCrosshairCacheWithIo(io, nullptr, destination.data(), pitch),
	            CrosshairCacheResult::BadPath, "a null path is refused before opening");
	CheckResult(ReadCrosshairCacheWithIo(io, "", destination.data(), pitch),
	            CrosshairCacheResult::BadPath, "an empty path is refused before opening");
	CheckResult(ReadCrosshairCacheWithIo(io, "cache", nullptr, pitch),
	            CrosshairCacheResult::BadPixelBuffer, "a null destination is refused");
	CheckResult(ReadCrosshairCacheWithIo(io, "cache", destination.data(), pitch - 100),
	            CrosshairCacheResult::BadPixelBuffer, "a short destination pitch is refused");

	io.readOpens = false;
	CheckResult(ReadCrosshairCacheWithIo(io, "cache", destination.data(), pitch),
	            CrosshairCacheResult::OpenFailed, "an unavailable file is a clean miss");
	io.ResetCalls(); PutFile(io, valid, source, pitch); io.sizeWorks = false;
	CheckResult(ReadCrosshairCacheWithIo(io, "cache", destination.data(), pitch),
	            CrosshairCacheResult::SizeReadFailed, "an unreadable file size is refused");
	io.ResetCalls(); PutFile(io, valid, source, pitch); io.failReadCall = 1;
	CheckResult(ReadCrosshairCacheWithIo(io, "cache", destination.data(), pitch),
	            CrosshairCacheResult::HeaderReadFailed, "a failed header read is refused");
	io.ResetCalls(); PutFile(io, valid, source, pitch); io.failReadCall = 2;
	CheckResult(ReadCrosshairCacheWithIo(io, "cache", destination.data(), pitch),
	            CrosshairCacheResult::PixelReadFailed, "a failed pixel read is refused");
	io.ResetCalls();
	auto wrongChecksum = valid; wrongChecksum.checksum ^= 1; PutFile(io, wrongChecksum, source, pitch);
	CheckResult(ReadCrosshairCacheWithIo(io, "cache", destination.data(), pitch),
	            CrosshairCacheResult::ChecksumMismatch, "changed pixels are refused");

	io.ResetCalls(); PutFile(io, valid, source, pitch);
	std::memset(destination.data(), 0xCC, destination.size());
	CheckResult(ReadCrosshairCacheWithIo(io, "cache", destination.data(), pitch),
	            CrosshairCacheResult::Ok, "a complete matching cache is read");
	bool pixelsMatch = true;
	bool paddingUntouched = true;
	for (UInt32 y = 0; y < kCrosshairCacheHeight; ++y) {
		pixelsMatch &= std::memcmp(source.data() + static_cast<size_t>(y) * pitch,
		                           destination.data() + static_cast<size_t>(y) * pitch,
		                           kCrosshairCacheRowBytes) == 0;
		for (SInt32 x = static_cast<SInt32>(kCrosshairCacheRowBytes); x < pitch; ++x) {
			paddingUntouched &= destination[static_cast<size_t>(y) * pitch + x] == 0xCC;
		}
	}
	Check(pixelsMatch, "all stored colour and alpha bytes survive");
	Check(paddingUntouched, "destination pitch padding stays untouched");
}

void TestWriteFlows() {
	std::printf("Atomic write flows\n");
	const SInt32 pitch = static_cast<SInt32>(kCrosshairCacheRowBytes) + 7;
	auto pixels = Pixels(pitch, 0xEF);
	MemoryIo io;

	CheckResult(WriteCrosshairCacheWithIo(io, nullptr, "temp", pixels.data(), pitch),
	            CrosshairCacheResult::BadPath, "a null final path is refused");
	CheckResult(WriteCrosshairCacheWithIo(io, "final", "", pixels.data(), pitch),
	            CrosshairCacheResult::BadPath, "an empty temporary path is refused");
	CheckResult(WriteCrosshairCacheWithIo(io, "final", "temp", nullptr, pitch),
	            CrosshairCacheResult::BadPixelBuffer, "a null source is refused");
	CheckResult(WriteCrosshairCacheWithIo(io, "final", "temp", pixels.data(), pitch - 100),
	            CrosshairCacheResult::BadPixelBuffer, "a short source pitch is refused");

	io.writeOpens = false;
	CheckResult(WriteCrosshairCacheWithIo(io, "final", "temp", pixels.data(), pitch),
	            CrosshairCacheResult::OpenFailed, "an unwritable temporary file is refused");
	io.ResetCalls(); io.failWriteCall = 1;
	CheckResult(WriteCrosshairCacheWithIo(io, "final", "temp", pixels.data(), pitch),
	            CrosshairCacheResult::HeaderWriteFailed, "a failed header write removes temp");
	Check(io.closes == 1 && io.removes == 1 && io.replaces == 0,
	      "header failure closes and removes without replacing");
	io.ResetCalls(); io.failWriteCall = 2;
	CheckResult(WriteCrosshairCacheWithIo(io, "final", "temp", pixels.data(), pitch),
	            CrosshairCacheResult::PixelWriteFailed, "a failed pixel write removes temp");
	Check(io.closes == 1 && io.removes == 1 && io.replaces == 0,
	      "pixel failure closes and removes without replacing");
	io.ResetCalls(); io.flushWorks = false;
	CheckResult(WriteCrosshairCacheWithIo(io, "final", "temp", pixels.data(), pitch),
	            CrosshairCacheResult::FlushFailed, "an unflushed cache is not published");
	Check(io.closes == 1 && io.removes == 1 && io.replaces == 0,
	      "flush failure closes and removes without replacing");
	io.ResetCalls(); io.replaceWorks = false;
	CheckResult(WriteCrosshairCacheWithIo(io, "final", "temp", pixels.data(), pitch),
	            CrosshairCacheResult::MoveFailed, "a failed atomic replace removes temp");
	Check(io.closes == 1 && io.removes == 1 && io.replaces == 1,
	      "replace failure cleans only the unpublished file");
	io.ResetCalls();
	CheckResult(WriteCrosshairCacheWithIo(io, "final", "temp", pixels.data(), pitch),
	            CrosshairCacheResult::Ok, "a complete file is atomically published");
	Check(io.closes == 1 && io.removes == 0 && io.replaces == 1,
	      "success closes, keeps and replaces exactly once");
	Check(io.bytes.size() == sizeof(CrosshairCacheHeader) + kCrosshairCachePixelBytes,
	      "the persisted file has one exact size without pitch padding");
}

void TestResultNames() {
	std::printf("Diagnostic names\n");
	for (UInt32 value = static_cast<UInt32>(CrosshairCacheResult::Ok);
	     value <= static_cast<UInt32>(CrosshairCacheResult::MoveFailed); ++value) {
		const char* name = CrosshairCacheResultName(static_cast<CrosshairCacheResult>(value));
		Check(name != nullptr && name[0] != '\0' && std::strcmp(name, "unknown result") != 0,
		      "every result has a specific log name");
	}
	Check(std::strcmp(CrosshairCacheResultName(static_cast<CrosshairCacheResult>(999)),
	                  "unknown result") == 0,
	      "an impossible result is still safe to log");
}

void TestRealFileRoundTrip() {
	std::printf("Win32 file round trip\n");
	const char* finalPath = "CrosshairCacheTest.bin";
	const char* temporaryPath = "CrosshairCacheTest.bin.tmp";
	std::remove(finalPath);
	std::remove(temporaryPath);

	const SInt32 sourcePitch = static_cast<SInt32>(kCrosshairCacheRowBytes) + 11;
	const SInt32 destinationPitch = static_cast<SInt32>(kCrosshairCacheRowBytes) + 3;
	auto source = Pixels(sourcePitch, 0x61);
	auto destination = Pixels(destinationPitch, 0x7A);
	CheckResult(WriteCrosshairCache(finalPath, temporaryPath, source.data(), sourcePitch),
	            CrosshairCacheResult::Ok, "the real atomic writer creates a cache");
	CheckResult(ReadCrosshairCache(finalPath, destination.data(), destinationPitch),
	            CrosshairCacheResult::Ok, "the real reader accepts what was written");
	CheckResult(ReadCrosshairCache("CrosshairCacheTest.missing", destination.data(),
	                               destinationPitch),
	            CrosshairCacheResult::OpenFailed, "the real reader handles a missing cache");

	bool same = true;
	for (UInt32 y = 0; y < kCrosshairCacheHeight; ++y) {
		same &= std::memcmp(source.data() + static_cast<size_t>(y) * sourcePitch,
		                    destination.data() + static_cast<size_t>(y) * destinationPitch,
		                    kCrosshairCacheRowBytes) == 0;
	}
	Check(same, "the real file preserves all 256x256 ARGB pixels");
	std::remove(finalPath);
	std::remove(temporaryPath);
}

}  // namespace

int main() {
	std::printf("OBVR crosshair cache test\n\n");
	TestBufferAndChecksum();
	std::printf("\n");
	TestDecisions();
	std::printf("\n");
	TestHeaderValidation();
	std::printf("\n");
	TestReadFlows();
	std::printf("\n");
	TestWriteFlows();
	std::printf("\n");
	TestResultNames();
	std::printf("\n");
	TestRealFileRoundTrip();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}
	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
