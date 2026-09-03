#include "render/CrosshairCache.h"

#include "platform/Win32Min.h"

namespace obvr::render {
namespace {

constexpr UInt32 kFnvOffset = 2166136261u;
constexpr UInt32 kFnvPrime = 16777619u;

bool PathValid(const char* path) { return path != nullptr && path[0] != '\0'; }

class Win32CrosshairCacheIo final : public CrosshairCacheIo {
public:
	void* OpenRead(const char* path) override {
		HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		                          FILE_ATTRIBUTE_NORMAL, nullptr);
		return file == InvalidHandle() ? nullptr : file;
	}

	void* OpenWrite(const char* path) override {
		HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
		                          FILE_ATTRIBUTE_NORMAL, nullptr);
		return file == InvalidHandle() ? nullptr : file;
	}

	bool Size(void* file, UInt32& bytes) override {
		const DWORD size = GetFileSize(file, nullptr);
		if (size == INVALID_FILE_SIZE) {
			return false;
		}
		bytes = size;
		return true;
	}

	bool Read(void* file, void* destination, UInt32 bytes) override {
		DWORD read = 0;
		return ReadFile(file, destination, bytes, &read, nullptr) != 0 && read == bytes;
	}

	bool Write(void* file, const void* source, UInt32 bytes) override {
		DWORD written = 0;
		return WriteFile(file, source, bytes, &written, nullptr) != 0 && written == bytes;
	}

	bool Flush(void* file) override { return FlushFileBuffers(file) != 0; }
	void Close(void* file) override { CloseHandle(file); }
	bool Replace(const char* temporaryPath, const char* finalPath) override {
		return MoveFileExA(temporaryPath, finalPath,
		                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
	}
	void Remove(const char* path) override { DeleteFileA(path); }
};

}  // namespace

bool CrosshairCachePixelBufferValid(const void* pixels, SInt32 pitch) {
	return pixels != nullptr && pitch >= static_cast<SInt32>(kCrosshairCacheRowBytes);
}

UInt32 CrosshairCacheChecksum(const void* pixels, SInt32 pitch) {
	if (!CrosshairCachePixelBufferValid(pixels, pitch)) {
		return 0;
	}

	UInt32 hash = kFnvOffset;
	for (UInt32 y = 0; y < kCrosshairCacheHeight; ++y) {
		const auto* row = static_cast<const UInt8*>(pixels) + y * pitch;
		for (UInt32 x = 0; x < kCrosshairCacheRowBytes; ++x) {
			hash ^= row[x];
			hash *= kFnvPrime;
		}
	}
	return hash;
}

CrosshairCacheHeader MakeCrosshairCacheHeader(UInt32 checksum) {
	CrosshairCacheHeader header{};
	header.magic = kCrosshairCacheMagic;
	header.version = kCrosshairCacheVersion;
	header.width = kCrosshairCacheWidth;
	header.height = kCrosshairCacheHeight;
	header.format = kCrosshairCacheFormatA8R8G8B8;
	header.rowBytes = kCrosshairCacheRowBytes;
	header.pixelBytes = kCrosshairCachePixelBytes;
	header.checksum = checksum;
	return header;
}

CrosshairCacheResult ValidateCrosshairCacheHeader(const CrosshairCacheHeader& header,
                                                  UInt32 fileBytes) {
	if (header.magic != kCrosshairCacheMagic) {
		return CrosshairCacheResult::WrongMagic;
	}
	if (header.version != kCrosshairCacheVersion) {
		return CrosshairCacheResult::WrongVersion;
	}
	if (header.width != kCrosshairCacheWidth || header.height != kCrosshairCacheHeight) {
		return CrosshairCacheResult::WrongDimensions;
	}
	if (header.format != kCrosshairCacheFormatA8R8G8B8) {
		return CrosshairCacheResult::WrongFormat;
	}
	if (header.rowBytes != kCrosshairCacheRowBytes) {
		return CrosshairCacheResult::WrongRowBytes;
	}
	if (header.pixelBytes != kCrosshairCachePixelBytes) {
		return CrosshairCacheResult::WrongPixelBytes;
	}
	if (fileBytes != sizeof(CrosshairCacheHeader) + kCrosshairCachePixelBytes) {
		return CrosshairCacheResult::WrongFileSize;
	}
	return CrosshairCacheResult::Ok;
}

const char* CrosshairCacheResultName(CrosshairCacheResult result) {
	switch (result) {
	case CrosshairCacheResult::Ok: return "ok";
	case CrosshairCacheResult::BadPath: return "bad path";
	case CrosshairCacheResult::BadPixelBuffer: return "bad pixel buffer";
	case CrosshairCacheResult::OpenFailed: return "file not found or inaccessible";
	case CrosshairCacheResult::SizeReadFailed: return "file size unavailable";
	case CrosshairCacheResult::HeaderReadFailed: return "truncated header";
	case CrosshairCacheResult::WrongMagic: return "wrong file signature";
	case CrosshairCacheResult::WrongVersion: return "unsupported version";
	case CrosshairCacheResult::WrongDimensions: return "wrong dimensions";
	case CrosshairCacheResult::WrongFormat: return "wrong pixel format";
	case CrosshairCacheResult::WrongRowBytes: return "wrong row size";
	case CrosshairCacheResult::WrongPixelBytes: return "wrong pixel count";
	case CrosshairCacheResult::WrongFileSize: return "wrong file size";
	case CrosshairCacheResult::PixelReadFailed: return "truncated pixels";
	case CrosshairCacheResult::ChecksumMismatch: return "checksum mismatch";
	case CrosshairCacheResult::HeaderWriteFailed: return "header write failed";
	case CrosshairCacheResult::PixelWriteFailed: return "pixel write failed";
	case CrosshairCacheResult::FlushFailed: return "flush failed";
	case CrosshairCacheResult::MoveFailed: return "replace failed";
	}
	return "unknown result";
}

bool CrosshairCacheLoadWanted(bool enabled, bool alreadyHavePicture, bool alreadyTried) {
	return enabled && !alreadyHavePicture && !alreadyTried;
}

bool CrosshairCacheSaveWanted(bool enabled, bool cleanPictureCaptured, bool alreadyTried) {
	return enabled && cleanPictureCaptured && !alreadyTried;
}

CrosshairCacheResult ReadCrosshairCache(const char* path, void* pixels, SInt32 pitch) {
	Win32CrosshairCacheIo io;
	return ReadCrosshairCacheWithIo(io, path, pixels, pitch);
}

CrosshairCacheResult ReadCrosshairCacheWithIo(CrosshairCacheIo& io, const char* path,
                                              void* pixels, SInt32 pitch) {
	if (!PathValid(path)) {
		return CrosshairCacheResult::BadPath;
	}
	if (!CrosshairCachePixelBufferValid(pixels, pitch)) {
		return CrosshairCacheResult::BadPixelBuffer;
	}

	void* file = io.OpenRead(path);
	if (file == nullptr) {
		return CrosshairCacheResult::OpenFailed;
	}

	UInt32 fileBytes = 0;
	if (!io.Size(file, fileBytes)) {
		io.Close(file);
		return CrosshairCacheResult::SizeReadFailed;
	}

	CrosshairCacheHeader header{};
	if (!io.Read(file, &header, sizeof(header))) {
		io.Close(file);
		return CrosshairCacheResult::HeaderReadFailed;
	}
	const CrosshairCacheResult validation = ValidateCrosshairCacheHeader(header, fileBytes);
	if (validation != CrosshairCacheResult::Ok) {
		io.Close(file);
		return validation;
	}

	for (UInt32 y = 0; y < kCrosshairCacheHeight; ++y) {
		auto* row = static_cast<UInt8*>(pixels) + y * pitch;
		if (!io.Read(file, row, kCrosshairCacheRowBytes)) {
			io.Close(file);
			return CrosshairCacheResult::PixelReadFailed;
		}
	}
	io.Close(file);

	return CrosshairCacheChecksum(pixels, pitch) == header.checksum
	           ? CrosshairCacheResult::Ok
	           : CrosshairCacheResult::ChecksumMismatch;
}

CrosshairCacheResult WriteCrosshairCache(const char* finalPath, const char* temporaryPath,
                                         const void* pixels, SInt32 pitch) {
	Win32CrosshairCacheIo io;
	return WriteCrosshairCacheWithIo(io, finalPath, temporaryPath, pixels, pitch);
}

CrosshairCacheResult WriteCrosshairCacheWithIo(CrosshairCacheIo& io,
                                               const char* finalPath,
                                               const char* temporaryPath,
                                               const void* pixels, SInt32 pitch) {
	if (!PathValid(finalPath) || !PathValid(temporaryPath)) {
		return CrosshairCacheResult::BadPath;
	}
	if (!CrosshairCachePixelBufferValid(pixels, pitch)) {
		return CrosshairCacheResult::BadPixelBuffer;
	}

	void* file = io.OpenWrite(temporaryPath);
	if (file == nullptr) {
		return CrosshairCacheResult::OpenFailed;
	}

	const CrosshairCacheHeader header =
		MakeCrosshairCacheHeader(CrosshairCacheChecksum(pixels, pitch));
	if (!io.Write(file, &header, sizeof(header))) {
		io.Close(file);
		io.Remove(temporaryPath);
		return CrosshairCacheResult::HeaderWriteFailed;
	}

	for (UInt32 y = 0; y < kCrosshairCacheHeight; ++y) {
		const auto* row = static_cast<const UInt8*>(pixels) + y * pitch;
		if (!io.Write(file, row, kCrosshairCacheRowBytes)) {
			io.Close(file);
			io.Remove(temporaryPath);
			return CrosshairCacheResult::PixelWriteFailed;
		}
	}

	if (!io.Flush(file)) {
		io.Close(file);
		io.Remove(temporaryPath);
		return CrosshairCacheResult::FlushFailed;
	}
	io.Close(file);

	if (!io.Replace(temporaryPath, finalPath)) {
		io.Remove(temporaryPath);
		return CrosshairCacheResult::MoveFailed;
	}
	return CrosshairCacheResult::Ok;
}

}  // namespace obvr::render
