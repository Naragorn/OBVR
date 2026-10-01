#include "game/BowDrawSound.h"

#include "core/AddressSpace.h"
#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/SoundSlice.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// The key handler's call that makes a key's sound (0x0051AF70's, at
// 0x0051B059): thiscall(sound system, form id, flags, 1), ret 0Ch; a null
// sound skips the key (0x0051B062).
constexpr UInt32 kKeySoundCall = 0x0051B059;
constexpr UInt32 kMakeSound = 0x006AE0A0;
constexpr UInt32 kWpnBowDraw = 0x00097C38;

// OSGlobals (0x00B33398): its window at +0x08, its sound system at +0x24
// (GameSound.cpp); the sound system's master volume at +0xB8 and its effects
// volume at +0xC4 (Oblivion Reloaded's SoundControl, TESReloaded10
// Framework/Oblivion/Game.h - not read from the binary here; the log shows
// what was found).
constexpr UInt32 kOsGlobalsPointer = 0x00B33398;
constexpr UInt32 kOsGlobalsWindow = 0x08;
constexpr UInt32 kOsGlobalsSound = 0x24;
constexpr UInt32 kSoundMasterVolume = 0xB8;
constexpr UInt32 kSoundEffectsVolume = 0xC4;

constexpr const char* kLooseFile = "Data\\Sound\\fx\\wpn\\wpn_bowdraw.wav";
constexpr const char* kArchive = "Data\\Oblivion - Sounds.bsa";
constexpr const char* kArchiveFolder = "sound\\fx\\wpn";
constexpr const char* kArchiveName = "wpn_bowdraw.wav";

// The file's two parts, in seconds (the loudness in BowDrawSound.h), and 5 ms
// of fade at each cut.
struct PartSpan {
	float start;
	float end;
};
constexpr PartSpan kParts[2] = {{0.30f, 0.98f}, {1.04f, 1.60f}};
constexpr float kFadeSeconds = 0.005f;

// DirectSound, as dsound.h (Windows SDK 10.0.26100) declares it: the
// IDirectSound methods CreateSoundBuffer 3 and SetCooperativeLevel 6; the
// IDirectSoundBuffer methods Lock 11, Play 12, SetCurrentPosition 13,
// SetVolume 15, Stop 18 and Unlock 19; DSSCL_NORMAL 1; DSBCAPS_CTRLVOLUME
// 0x80 and DSBCAPS_GETCURRENTPOSITION2 0x10000.
constexpr UInt32 kCreateSoundBuffer = 3;
constexpr UInt32 kSetCooperativeLevel = 6;
constexpr UInt32 kBufferLock = 11;
constexpr UInt32 kBufferPlay = 12;
constexpr UInt32 kBufferSetPosition = 13;
constexpr UInt32 kBufferSetVolume = 15;
constexpr UInt32 kBufferStop = 18;
constexpr UInt32 kBufferUnlock = 19;
constexpr UInt32 kCooperativeNormal = 1;
constexpr UInt32 kBufferFlags = 0x80 | 0x10000;

#pragma pack(push, 1)
struct WaveFormat {
	UInt16 formatTag;
	UInt16 channels;
	UInt32 samplesPerSecond;
	UInt32 averageBytesPerSecond;
	UInt16 blockAlign;
	UInt16 bitsPerSample;
	UInt16 extraSize;
};
#pragma pack(pop)
static_assert(sizeof(WaveFormat) == 18, "WAVEFORMATEX is 18 bytes");

struct BufferDescription {
	UInt32 size;
	UInt32 flags;
	UInt32 bufferBytes;
	UInt32 reserved;
	WaveFormat* format;
	UInt8 algorithm3d[16];  // GUID_NULL: the default
};
static_assert(sizeof(BufferDescription) == 36, "DSBUFFERDESC is 36 bytes on x86");

template <typename Fn>
Fn Method(void* object, UInt32 index) {
	return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[index]);
}

enum class Load : UInt8 { NotTried, Ready, Failed };
Load g_load = Load::NotTried;
void* g_device = nullptr;
void* g_buffers[2] = {nullptr, nullptr};
bool g_playing[2] = {false, false};
bool g_byHand = false;
bool g_installed = false;
UInt32 g_quietLines = 4;

UInt32 Read32(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

// Pages of its own for a file read: the DLL has no heap of its own (the
// SDK-free build has no operator new).
UInt8* Allocate(UInt32 bytes) {
	return static_cast<UInt8*>(VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
}
void Free(UInt8* bytes) {
	if (bytes != nullptr) {
		VirtualFree(bytes, 0, MEM_RELEASE);
	}
}

UInt32 SoundSystem() {
	const UInt32 globals = Read32(kOsGlobalsPointer);
	return mem::LooksLikeObjectAddress(globals) ? Read32(globals + kOsGlobalsSound) : 0;
}

// The game's master and effects volume, each taken as 1 when it is not a
// gain - the log says which.
float GameEffectsGain(float& master, float& effects) {
	master = 1.0f;
	effects = 1.0f;
	const UInt32 system = SoundSystem();
	if (!mem::LooksLikeObjectAddress(system)) {
		return 1.0f;
	}
	const float m = *reinterpret_cast<const float*>(system + kSoundMasterVolume);
	const float e = *reinterpret_cast<const float*>(system + kSoundEffectsVolume);
	master = m >= 0.0f && m <= 1.0f ? m : 1.0f;
	effects = e >= 0.0f && e <= 1.0f ? e : 1.0f;
	return master * effects;
}

void* __fastcall OnKeySound(void* system, void* /*edx*/, UInt32 formId, UInt32 flags, UInt32 last) {
	if (formId == kWpnBowDraw && g_byHand && g_load == Load::Ready) {
		if (g_quietLines > 0) {
			--g_quietLines;
			OBVR_LOG("Bow draw sound: the engine's WPNBowDraw at the draw's key kept quiet - OBVR plays its parts");
		}
		return nullptr;
	}
	using MakeFn = void*(__thiscall*)(void* self, UInt32 formId, UInt32 flags, UInt32 last);
	return reinterpret_cast<MakeFn>(kMakeSound)(system, formId, flags, last);
}

// A whole file, or `size` bytes of it from `offset`; null when it cannot be
// read. The caller frees it.
UInt8* ReadFileBytes(const char* path, UInt32 offset, UInt32& size, bool whole) {
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == InvalidHandle()) {
		return nullptr;
	}
	if (whole) {
		const DWORD length = GetFileSize(file, nullptr);
		if (length == INVALID_FILE_SIZE || length == 0 || length > 0x01000000) {
			CloseHandle(file);
			return nullptr;
		}
		size = length;
	} else if (SetFilePointer(file, static_cast<long>(offset), nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		CloseHandle(file);
		return nullptr;
	}
	UInt8* const bytes = Allocate(size);
	if (bytes == nullptr) {
		CloseHandle(file);
		return nullptr;
	}
	DWORD got = 0;
	const bool ok = ReadFile(file, bytes, size, &got, nullptr) != 0 && got == size;
	CloseHandle(file);
	if (!ok) {
		Free(bytes);
		return nullptr;
	}
	return bytes;
}

// The draw sound's file: loose, else out of the game's sound archive.
UInt8* LoadDrawSound(UInt32& size, const char*& from) {
	char path[520];
	if (platform::BuildGamePath(kLooseFile, path, sizeof(path)) && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
		from = "the loose file";
		return ReadFileBytes(path, 0, size, true);
	}
	from = "Oblivion - Sounds.bsa";
	if (!platform::BuildGamePath(kArchive, path, sizeof(path))) {
		return nullptr;
	}
	UInt32 headerSize = kBsaHeaderBytes;
	UInt8* const header = ReadFileBytes(path, 0, headerSize, false);
	if (header == nullptr) {
		return nullptr;
	}
	UInt32 indexSize = BsaIndexBytes(header, headerSize);
	Free(header);
	if (indexSize == 0) {
		return nullptr;
	}
	UInt8* const index = ReadFileBytes(path, 0, indexSize, false);
	if (index == nullptr) {
		return nullptr;
	}
	BsaEntry entry;
	const bool found = BsaFind(index, indexSize, kArchiveFolder, kArchiveName, entry);
	Free(index);
	if (!found || entry.size == 0 || entry.size > 0x01000000) {
		return nullptr;
	}
	size = entry.size;
	return ReadFileBytes(path, entry.offset, size, false);
}

void* MakeBuffer(const WavPcm& pcm, const UInt8* file, const PartSpan& span) {
	UInt32 offset = 0;
	UInt32 bytes = 0;
	if (!SliceBytes(pcm, span.start, span.end, offset, bytes)) {
		return nullptr;
	}
	WaveFormat format{1,
	                  pcm.channels,
	                  pcm.samplesPerSecond,
	                  pcm.samplesPerSecond * pcm.blockAlign,
	                  static_cast<UInt16>(pcm.blockAlign),
	                  pcm.bitsPerSample,
	                  0};
	BufferDescription description{};
	description.size = sizeof(description);
	description.flags = kBufferFlags;
	description.bufferBytes = bytes;
	description.format = &format;
	void* buffer = nullptr;
	using CreateFn = SInt32(__stdcall*)(void* self, const BufferDescription* description, void** buffer, void* outer);
	if (Method<CreateFn>(g_device, kCreateSoundBuffer)(g_device, &description, &buffer, nullptr) < 0 ||
	    buffer == nullptr) {
		return nullptr;
	}
	void* first = nullptr;
	DWORD firstBytes = 0;
	void* second = nullptr;
	DWORD secondBytes = 0;
	using LockFn = SInt32(__stdcall*)(void* self, UInt32 offset, UInt32 bytes, void** first, DWORD* firstBytes,
	                                  void** second, DWORD* secondBytes, UInt32 flags);
	using UnlockFn = SInt32(__stdcall*)(void* self, void* first, UInt32 firstBytes, void* second, UInt32 secondBytes);
	if (Method<LockFn>(buffer, kBufferLock)(buffer, 0, bytes, &first, &firstBytes, &second, &secondBytes, 0) < 0 ||
	    first == nullptr || firstBytes < bytes) {
		return nullptr;
	}
	UInt8* const out = static_cast<UInt8*>(first);
	for (UInt32 i = 0; i < bytes; ++i) {
		out[i] = file[offset + i];
	}
	if (pcm.bitsPerSample == 16) {
		FadeEdges(reinterpret_cast<SInt16*>(out), bytes / pcm.blockAlign, pcm.channels,
		          static_cast<UInt32>(kFadeSeconds * static_cast<float>(pcm.samplesPerSecond)));
	}
	Method<UnlockFn>(buffer, kBufferUnlock)(buffer, first, firstBytes, second, secondBytes);
	return buffer;
}

void Prepare() {
	if (g_load != Load::NotTried) {
		return;
	}
	g_load = Load::Failed;
	const UInt32 globals = Read32(kOsGlobalsPointer);
	void* const window = mem::LooksLikeObjectAddress(globals)
	                         ? *reinterpret_cast<void* const*>(globals + kOsGlobalsWindow)
	                         : nullptr;
	UInt32 size = 0;
	const char* from = "";
	UInt8* const file = LoadDrawSound(size, from);
	WavPcm pcm;
	if (file == nullptr || !ParseWav(file, size, pcm)) {
		OBVR_LOG("Bow draw sound: fx\\wpn\\wpn_bowdraw.wav not read from %s - the engine's draw sound stays whole",
		         from);
		Free(file);
		return;
	}
	HMODULE dsound = LoadLibraryA("dsound.dll");
	using CreateDeviceFn = SInt32(__stdcall*)(const void* device, void** directSound, void* outer);
	const auto create =
		dsound != nullptr ? reinterpret_cast<CreateDeviceFn>(GetProcAddress(dsound, "DirectSoundCreate")) : nullptr;
	using CooperateFn = SInt32(__stdcall*)(void* self, void* window, UInt32 level);
	if (create == nullptr || window == nullptr || create(nullptr, &g_device, nullptr) < 0 || g_device == nullptr ||
	    Method<CooperateFn>(g_device, kSetCooperativeLevel)(g_device, window, kCooperativeNormal) < 0) {
		OBVR_LOG("Bow draw sound: no DirectSound device (the library %s, the game's window %s) - the engine's draw "
		         "sound stays whole",
		         create != nullptr ? "found" : "MISSING", window != nullptr ? "found" : "MISSING");
		Free(file);
		return;
	}
	for (UInt32 i = 0; i < 2; ++i) {
		g_buffers[i] = MakeBuffer(pcm, file, kParts[i]);
	}
	Free(file);
	if (g_buffers[0] == nullptr || g_buffers[1] == nullptr) {
		OBVR_LOG("Bow draw sound: the parts could not be made - the engine's draw sound stays whole");
		return;
	}
	float master = 0.0f;
	float effects = 0.0f;
	GameEffectsGain(master, effects);
	OBVR_LOG("Bow draw sound: fx\\wpn\\wpn_bowdraw.wav from %s, %u Hz, %u channel(s), %u bits, %.2f s; parts %.2f-%.2f "
	         "and %.2f-%.2f s ready; the game's volume read as master %.2f, effects %.2f",
	         from, pcm.samplesPerSecond, static_cast<unsigned>(pcm.channels), static_cast<unsigned>(pcm.bitsPerSample),
	         static_cast<double>(pcm.dataBytes / pcm.blockAlign) / static_cast<double>(pcm.samplesPerSecond),
	         static_cast<double>(kParts[0].start), static_cast<double>(kParts[0].end),
	         static_cast<double>(kParts[1].start), static_cast<double>(kParts[1].end), static_cast<double>(master),
	         static_cast<double>(effects));
	g_load = Load::Ready;
}

}  // namespace

bool InstallBowDrawSound() {
	const UInt32 displacement = mem::CallRelativeDisplacement(kKeySoundCall, kMakeSound);
	const UInt8 expected[5] = {0xE8, static_cast<UInt8>(displacement & 0xFF), static_cast<UInt8>((displacement >> 8) & 0xFF),
	                           static_cast<UInt8>((displacement >> 16) & 0xFF),
	                           static_cast<UInt8>((displacement >> 24) & 0xFF)};
	if (!mem::Verify(kKeySoundCall, expected, sizeof(expected))) {
		OBVR_LOG("Bow draw sound: the call at %08X is not `call %08X` - the engine's draw sound stays whole",
		         kKeySoundCall, kMakeSound);
		mem::ReportForeignCode("Bow draw sound", kKeySoundCall);
		return false;
	}
	UInt8 patch[5];
	if (mem::BuildCallSitePatch(patch, sizeof(patch), kKeySoundCall, reinterpret_cast<UInt32>(&OnKeySound)) !=
	        sizeof(patch) ||
	    !mem::SafeWrite(kKeySoundCall, patch, sizeof(patch))) {
		OBVR_LOG("Bow draw sound: could not reroute the call at %08X", kKeySoundCall);
		return false;
	}
	g_installed = true;
	return true;
}

void SetBowDrawSoundByHand(bool byHand) {
	if (byHand && g_installed) {
		Prepare();
	}
	g_byHand = byHand;
}

bool PlayBowDrawPart(BowDrawPart part) {
	if (g_load != Load::Ready) {
		return false;
	}
	void* const buffer = g_buffers[static_cast<UInt32>(part)];
	float master = 0.0f;
	float effects = 0.0f;
	const float gain = GameEffectsGain(master, effects);
	using StopFn = SInt32(__stdcall*)(void* self);
	using PositionFn = SInt32(__stdcall*)(void* self, UInt32 position);
	using VolumeFn = SInt32(__stdcall*)(void* self, SInt32 volume);
	using PlayFn = SInt32(__stdcall*)(void* self, UInt32 reserved, UInt32 priority, UInt32 flags);
	Method<StopFn>(buffer, kBufferStop)(buffer);
	Method<PositionFn>(buffer, kBufferSetPosition)(buffer, 0);
	Method<VolumeFn>(buffer, kBufferSetVolume)(buffer, DirectSoundVolume(gain));
	const bool played = Method<PlayFn>(buffer, kBufferPlay)(buffer, 0, 0, 0) >= 0;
	g_playing[static_cast<UInt32>(part)] = played;
	static UInt32 s_lines = 8;
	if (s_lines > 0) {
		--s_lines;
		OBVR_LOG("Bow draw sound: the %s part %s", part == BowDrawPart::Nock ? "first (nock)" : "second (stretch)",
		         played ? "played" : "COULD NOT BE PLAYED");
	}
	return played;
}

void StopBowDrawPart(BowDrawPart part) {
	// Only a part OBVR started: called each frame the bow is not drawn.
	if (g_load != Load::Ready || !g_playing[static_cast<UInt32>(part)]) {
		return;
	}
	g_playing[static_cast<UInt32>(part)] = false;
	void* const buffer = g_buffers[static_cast<UInt32>(part)];
	using StopFn = SInt32(__stdcall*)(void* self);
	Method<StopFn>(buffer, kBufferStop)(buffer);
}

}  // namespace obvr::game
