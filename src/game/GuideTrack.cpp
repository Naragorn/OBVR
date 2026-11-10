#include "game/GuideTrack.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/MenuType.h"
#include "platform/Win32Min.h"
#include "ui/GuideStage.h"

namespace obvr::game {

namespace {

// The COM calls by their places in each interface's table (strmif.h,
// control.h): IUnknown 0 QueryInterface, 2 Release; IGraphBuilder 13
// RenderFile (the game calls +0x34); IMediaControl and IBasicAudio and
// IMediaPosition are IDispatch-based, so their own methods start at 7:
// IMediaControl 7 Run (the game calls +0x1C), 9 Stop; IBasicAudio 7
// put_Volume (+0x1C, the game's volume setter), 8 get_Volume; IMediaPosition
// 7 get_Duration, 8 put_CurrentPosition, 9 get_CurrentPosition.
using QueryFn = long(__stdcall*)(void* self, const void* iid, void** out);
using ReleaseFn = unsigned long(__stdcall*)(void* self);
using RenderFileFn = long(__stdcall*)(void* self, const wchar_t* file, const wchar_t* playList);
using NoArgFn = long(__stdcall*)(void* self);
using PutLongFn = long(__stdcall*)(void* self, long value);
using GetLongFn = long(__stdcall*)(void* self, long* value);
using PutDoubleFn = long(__stdcall*)(void* self, double value);
using GetDoubleFn = long(__stdcall*)(void* self, double* value);
using CreateFn = long(__stdcall*)(const void* clsid, void* outer, UInt32 context, const void* iid, void** out);

template <typename Fn>
Fn Slot(void* object, UInt32 index) {
	return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[index]);
}

// The GUIDs as the game holds them (read 2026-11-09 at 0x00A78F64: 16 bytes
// each, IGraphBuilder 56A868A9, IBasicAudio 56A868B3, IMediaPosition
// 56A868B2, IMediaEvent 56A868B6, IMediaControl 56A868B1, then the
// FilterGraph class E436EBB3 at 0x00A78FB4).
constexpr UInt32 kIidGraphBuilder = 0x00A78F64;
constexpr UInt32 kIidBasicAudio = 0x00A78F74;
constexpr UInt32 kIidMediaPosition = 0x00A78F84;
constexpr UInt32 kIidMediaControl = 0x00A78FA4;
constexpr UInt32 kClsidFilterGraph = 0x00A78FB4;
constexpr UInt32 kClsctxInproc = 1;

// The game's music player: *(0x00B33398) + 0x24 (0x005B5B25..36), its
// IBasicAudio at +0x74, its state word at +0xDC (bit 0: a graph is open),
// its volume now at +0x2F0; the setter 0x006AA1A0 (thiscall: volume, keep).
constexpr UInt32 kSoundGlobalsPointer = 0x00B33398;
constexpr UInt32 kMusicPlayerOffset = 0x24;
constexpr UInt32 kMusicAudioOffset = 0x74;
constexpr UInt32 kMusicStateOffset = 0xDC;
constexpr UInt32 kMusicVolumeOffset = 0x2F0;
constexpr UInt32 kMusicSetVolume = 0x006AA1A0;
constexpr long kSilent = -10000;

struct Track {
	void* graph = nullptr;
	void* control = nullptr;
	void* audio = nullptr;
	void* position = nullptr;
};
// The one track, owned here.
Track& TheTrack() {
	static Track s_track;
	return s_track;
}

void Release(void*& object) {
	if (object != nullptr) {
		Slot<ReleaseFn>(object, 2)(object);
		object = nullptr;
	}
}

CreateFn CoCreate() {
	static CreateFn s_create = nullptr;
	static bool s_tried = false;
	if (!s_tried) {
		s_tried = true;
		const HMODULE ole = LoadLibraryA("ole32.dll");
		s_create = ole ? reinterpret_cast<CreateFn>(GetProcAddress(ole, "CoCreateInstance")) : nullptr;
	}
	return s_create;
}

void* MusicPlayer() {
	const UInt32 globals = *reinterpret_cast<const UInt32*>(kSoundGlobalsPointer);
	if (!mem::LooksLikeObjectAddress(globals)) {
		return nullptr;
	}
	const UInt32 player = *reinterpret_cast<const UInt32*>(globals + kMusicPlayerOffset);
	return mem::LooksLikeObjectAddress(player) ? reinterpret_cast<void*>(player) : nullptr;
}

void* MusicAudio(void* player) {
	const UInt8* p = static_cast<const UInt8*>(player);
	if ((*reinterpret_cast<const UInt32*>(p + kMusicStateOffset) & 1u) == 0) {
		return nullptr;  // no graph open
	}
	const UInt32 audio = *reinterpret_cast<const UInt32*>(p + kMusicAudioOffset);
	return mem::LooksLikeObjectAddress(audio) ? reinterpret_cast<void*>(audio) : nullptr;
}

void* MainMenuTile() {
	auto* const data = *reinterpret_cast<UInt32* const*>(addr::kTileMenuArrayData);
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(data))) {
		return nullptr;
	}
	const UInt32 count = *reinterpret_cast<const UInt16*>(addr::kTileMenuArrayCount);
	const UInt32 index = kMenuIdMain - kMenuIdFirst;
	if (index >= count) {
		return nullptr;
	}
	const UInt32 tile = data[index];
	return mem::LooksLikeObjectAddress(tile) ? reinterpret_cast<void*>(tile) : nullptr;
}

}  // namespace

bool OpenGuideTrack(const char* path, const char** why) {
	CloseGuideTrack();
	const CreateFn create = CoCreate();
	if (create == nullptr) {
		*why = "CoCreateInstance not found in ole32.dll";
		return false;
	}
	if (create(reinterpret_cast<const void*>(kClsidFilterGraph), nullptr, kClsctxInproc,
	           reinterpret_cast<const void*>(kIidGraphBuilder), &TheTrack().graph) < 0 ||
	    TheTrack().graph == nullptr) {
		*why = "no filter graph";
		return false;
	}
	// The path widened byte for byte: ASCII only, as the game's own paths.
	wchar_t wide[260];
	UInt32 i = 0;
	for (; path[i] != '\0' && i + 1 < 260; ++i) {
		wide[i] = static_cast<wchar_t>(static_cast<unsigned char>(path[i]));
	}
	wide[i] = 0;
	if (Slot<RenderFileFn>(TheTrack().graph, 13)(TheTrack().graph, wide, nullptr) < 0) {
		*why = "RenderFile refused the file";
		CloseGuideTrack();
		return false;
	}
	const QueryFn query = Slot<QueryFn>(TheTrack().graph, 0);
	query(TheTrack().graph, reinterpret_cast<const void*>(kIidMediaControl), &TheTrack().control);
	query(TheTrack().graph, reinterpret_cast<const void*>(kIidBasicAudio), &TheTrack().audio);
	query(TheTrack().graph, reinterpret_cast<const void*>(kIidMediaPosition), &TheTrack().position);
	if (TheTrack().control == nullptr || TheTrack().audio == nullptr) {
		*why = "no control or audio on the graph";
		CloseGuideTrack();
		return false;
	}
	Slot<PutLongFn>(TheTrack().audio, 7)(TheTrack().audio, kSilent);
	if (Slot<NoArgFn>(TheTrack().control, 7)(TheTrack().control) < 0) {
		*why = "Run refused";
		CloseGuideTrack();
		return false;
	}
	return true;
}

void CloseGuideTrack() {
	if (TheTrack().control != nullptr) {
		Slot<NoArgFn>(TheTrack().control, 9)(TheTrack().control);
	}
	Release(TheTrack().position);
	Release(TheTrack().audio);
	Release(TheTrack().control);
	Release(TheTrack().graph);
}

bool GuideTrackOpen() { return TheTrack().graph != nullptr; }

void SetGuideTrackVolume(float volume) {
	if (TheTrack().audio != nullptr) {
		Slot<PutLongFn>(TheTrack().audio, 7)(TheTrack().audio, ui::GuideVolumeToHundredthsDb(volume));
	}
}

float GuideTrackPosition() {
	double seconds = -1.0;
	if (TheTrack().position == nullptr || Slot<GetDoubleFn>(TheTrack().position, 9)(TheTrack().position, &seconds) < 0) {
		return -1.0f;
	}
	return static_cast<float>(seconds);
}

float GuideTrackDuration() {
	double seconds = -1.0;
	if (TheTrack().position == nullptr || Slot<GetDoubleFn>(TheTrack().position, 7)(TheTrack().position, &seconds) < 0) {
		return -1.0f;
	}
	return static_cast<float>(seconds);
}

void SeekGuideTrack(float seconds) {
	if (TheTrack().position != nullptr) {
		Slot<PutDoubleFn>(TheTrack().position, 8)(TheTrack().position, static_cast<double>(seconds));
	}
}

bool HoldTitleMusicSilent() {
	void* const player = MusicPlayer();
	if (player == nullptr) {
		return false;
	}
	void* const audio = MusicAudio(player);
	if (audio != nullptr) {
		long now = 0;
		if (Slot<GetLongFn>(audio, 8)(audio, &now) < 0 || now != kSilent) {
			Slot<PutLongFn>(audio, 7)(audio, kSilent);
		}
	}
	return true;
}

bool GiveTitleMusicBack() {
	void* const player = MusicPlayer();
	if (player == nullptr) {
		return false;
	}
	const float volume = *reinterpret_cast<const float*>(static_cast<const UInt8*>(player) + kMusicVolumeOffset);
	using SetVolumeFn = void(__thiscall*)(void* self, float volume, bool keep);
	reinterpret_cast<SetVolumeFn>(kMusicSetVolume)(player, volume, false);
	return true;
}

bool MainMenuLoaded() { return MainMenuTile() != nullptr; }

bool SetMainMenuShown(bool shown) {
	void* const tile = MainMenuTile();
	if (tile == nullptr) {
		return false;
	}
	using UpdateFloatFn = void(__thiscall*)(void* self, UInt32 trait, float value);
	reinterpret_cast<UpdateFloatFn>(addr::kTileUpdateFloat)(tile, 0x00000FA1u, ui::MainMenuVisibleTrait(shown));
	return true;
}

}  // namespace obvr::game
