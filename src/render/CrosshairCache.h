#pragma once

#include "core/Types.h"

namespace obvr::render {

// The cache is deliberately not an image format. It is one exact GPU texture:
// 256x256 A8R8G8B8, including alpha. A small header makes a stale, truncated or
// hand-edited file something to refuse rather than something to upload.
inline constexpr UInt32 kCrosshairCacheWidth = 256;
inline constexpr UInt32 kCrosshairCacheHeight = 256;
inline constexpr UInt32 kCrosshairCacheBytesPerPixel = 4;
inline constexpr UInt32 kCrosshairCacheRowBytes =
	kCrosshairCacheWidth * kCrosshairCacheBytesPerPixel;
inline constexpr UInt32 kCrosshairCachePixelBytes =
	kCrosshairCacheRowBytes * kCrosshairCacheHeight;

struct CrosshairCacheHeader {
	UInt32 magic;
	UInt32 version;
	UInt32 width;
	UInt32 height;
	UInt32 format;
	UInt32 rowBytes;
	UInt32 pixelBytes;
	UInt32 checksum;
};

static_assert(sizeof(CrosshairCacheHeader) == 32,
              "the crosshair cache header is eight 32-bit words");

enum class CrosshairCacheResult : UInt32 {
	Ok,
	BadPath,
	BadPixelBuffer,
	OpenFailed,
	SizeReadFailed,
	HeaderReadFailed,
	WrongMagic,
	WrongVersion,
	WrongDimensions,
	WrongFormat,
	WrongRowBytes,
	WrongPixelBytes,
	WrongFileSize,
	PixelReadFailed,
	ChecksumMismatch,
	HeaderWriteFailed,
	PixelWriteFailed,
	FlushFailed,
	MoveFailed,
};

inline constexpr UInt32 kCrosshairCacheMagic = 0x4358424F;  // "OBXC" in the file
inline constexpr UInt32 kCrosshairCacheVersion = 1;
inline constexpr UInt32 kCrosshairCacheFormatA8R8G8B8 = 21;

// Pure pieces used by both the file code and its exhaustive tests.
bool CrosshairCachePixelBufferValid(const void* pixels, SInt32 pitch);
UInt32 CrosshairCacheChecksum(const void* pixels, SInt32 pitch);
CrosshairCacheHeader MakeCrosshairCacheHeader(UInt32 checksum);
CrosshairCacheResult ValidateCrosshairCacheHeader(const CrosshairCacheHeader& header,
                                                  UInt32 fileBytes);
const char* CrosshairCacheResultName(CrosshairCacheResult result);
bool CrosshairCacheLoadWanted(bool enabled, bool alreadyHavePicture, bool alreadyTried);
bool CrosshairCacheSaveWanted(bool enabled, bool cleanPictureCaptured, bool alreadyTried);

// The file mechanism is injectable so every refusal path can be exercised
// without asking a real disk to fail on command. The production implementation
// below is Win32; tests provide a deterministic in-memory one.
class CrosshairCacheIo {
public:
	virtual ~CrosshairCacheIo() = default;
	virtual void* OpenRead(const char* path) = 0;
	virtual void* OpenWrite(const char* path) = 0;
	virtual bool Size(void* file, UInt32& bytes) = 0;
	virtual bool Read(void* file, void* destination, UInt32 bytes) = 0;
	virtual bool Write(void* file, const void* source, UInt32 bytes) = 0;
	virtual bool Flush(void* file) = 0;
	virtual void Close(void* file) = 0;
	virtual bool Replace(const char* temporaryPath, const char* finalPath) = 0;
	virtual void Remove(const char* path) = 0;
};

// Reads/writes rows without their Direct3D pitch padding. Read never makes a
// partly read image visible: its caller uploads the staging surface only after
// this returns Ok. Write goes to temporaryPath and replaces finalPath only
// after every byte has been written and flushed.
CrosshairCacheResult ReadCrosshairCache(const char* path, void* pixels, SInt32 pitch);
CrosshairCacheResult WriteCrosshairCache(const char* finalPath, const char* temporaryPath,
                                         const void* pixels, SInt32 pitch);
CrosshairCacheResult ReadCrosshairCacheWithIo(CrosshairCacheIo& io, const char* path,
                                              void* pixels, SInt32 pitch);
CrosshairCacheResult WriteCrosshairCacheWithIo(CrosshairCacheIo& io,
                                               const char* finalPath,
                                               const char* temporaryPath,
                                               const void* pixels, SInt32 pitch);

}  // namespace obvr::render
