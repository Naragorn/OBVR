#include "game/FittedCapsules.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "core/Log.h"
#include "core/AddressSpace.h"
#include "core/Memory.h"
#include "game/BladeMaterial.h"
#include "game/BodyViewLogic.h"
#include "game/GameAddresses.h"
#include "game/FitDump.h"
#include "game/RadialFieldBuild.h"
#include "game/RefFingerprint.h"
#include "game/GameTypes.h"
#include "game/SkinRead.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// Shapes left out of the fit by name: the blood decals the engine lays on a
// body (skinned, a few dozen vertices each; worn-shapes.txt counted 16 after
// one fight).
bool LeftOut(const char* name) { return std::strncmp(name, "Decal:", 6) == 0; }

// Every vertex of a shape where the drawn pose has it now, skinned with its
// own shape's skin: the shapes' bind spaces differ (a FaceGen head's from
// the body's - its vertices put by another skin's bind landed at the feet,
// fitted-capsules.txt 2026-10-25 13:33), the bones' world transforms do
// not. False when its weights do not add up to one.
bool SkinShapeNow(const SkinnedShape& s, std::vector<NiTransform>& boneWorld, std::vector<NiPoint3>& world) {
	const UInt32 shapeBones = static_cast<UInt32>(s.boneNode.size());
	boneWorld.resize(shapeBones);
	for (UInt32 b = 0; b < shapeBones; ++b) {
		boneWorld[b] = s.boneNode[b]->worldTransform;
	}
	world.assign(s.vertexCount, NiPoint3{0.0f, 0.0f, 0.0f});
	std::vector<float> weightSum(s.vertexCount, 0.0f);
	SkinVertices(s.bind, s.vertexCount, boneWorld.data(), s.skinToBone.data(), shapeBones, s.weights.data(),
	             static_cast<UInt32>(s.weights.size()), world.data(), weightSum.data());
	return SkinWeightMisses(weightSum.data(), s.vertexCount) == 0;
}

// What a worn item is (game/BladeMaterial.h): its form by ID, its type, the
// heavy flag of its biped model, armour's rating. Each item said once in the
// log with the raw values read, so the offsets can be checked against known
// armour (the spec, docs/blade-penetration-spec.md step 1).
using LookupFormFn = UInt32(__cdecl*)(UInt32 id);
constexpr UInt32 kLookupFormById = 0x0046B250;  // xOBSE GameAPI.cpp, cdecl
using DynamicCastFn = void*(__cdecl*)(void*, UInt32, const void*, const void*, UInt32);

BodyMaterial WornMaterial(UInt32 wornId, UInt8& rating) {
	rating = 0;
	if (wornId == 0) {
		return BodyMaterial::Skin;
	}
	const UInt32 form = reinterpret_cast<LookupFormFn>(kLookupFormById)(wornId);
	if (!mem::LooksLikeObjectAddress(form)) {
		return MaterialOfWorn(wornId, false, 0, false);
	}
	const UInt8 type = *reinterpret_cast<const UInt8*>(form + addr::kFormTypeOffset);
	const auto cast = reinterpret_cast<DynamicCastFn>(addr::kDynamicCast);
	const UInt32 biped = reinterpret_cast<UInt32>(cast(reinterpret_cast<void*>(form), 0,
	                                                   reinterpret_cast<const void*>(addr::kRttiTESForm),
	                                                   reinterpret_cast<const void*>(addr::kRttiTESBipedModelForm), 0));
	const UInt8 flags = mem::LooksLikeObjectAddress(biped) ? *reinterpret_cast<const UInt8*>(biped + kBipedFlagsOffset) : 0;
	const UInt16 raw = type == kWornTypeArmor ? *reinterpret_cast<const UInt16*>(form + kArmorRatingOffset) : 0;
	const BodyMaterial m = MaterialOfWorn(wornId, true, type, (flags & kBipedHeavyArmorBit) != 0);
	rating = ArmorPointsOf(raw);
	static UInt32 s_lines = 40;
	static UInt32 s_said[64] = {};
	static UInt32 s_saidCount = 0;
	bool said = false;
	for (UInt32 i = 0; i < s_saidCount; ++i) {
		said = said || s_said[i] == wornId;
	}
	if (!said && s_lines > 0) {
		--s_lines;
		if (s_saidCount < 64) {
			s_said[s_saidCount++] = wornId;
		}
		OBVR_LOG("Blade material: worn %08X - form type %02X, biped %08X flags %02X, rating raw %u - %s, %u points", wornId,
		         type, biped, flags, raw, BodyMaterialName(m), rating);
	}
	return m;
}

}  // namespace

struct FitJob {
	std::vector<std::string> names;  // the bones' names, copied: the worker outlives no engine string
	std::vector<FitBone> bones;
	std::vector<const NiAVObject*> boneNodes;
	std::vector<FitVertex> vertices;
	float lengthSplitRatio = kFitLengthSplitRatio;
	FittedBody result;
	std::atomic<bool> done{false};
	// the game's thread and the worker each hold the job; the last to let
	// go deletes it (an actor gone mid-fit abandons its job)
	std::atomic<int> holders{2};
};

namespace {

void Release(FitJob* job) {
	if (job->holders.fetch_sub(1) == 1) {
		delete job;
	}
}

// The fits whose workers have not ended yet, abandoned ones included: they
// still hold a thread and their copy of the vertices (game/BodyKeepLogic.h,
// FitMayStart).
std::atomic<UInt32>& RunningFits() {
	static std::atomic<UInt32> s_running{0};
	return s_running;
}

void ComputeFit(FitJob* job, long long started) {
	FittedBody& out = job->result;
	out.count = FitBodyCapsules(job->bones.data(), static_cast<UInt32>(job->bones.size()), job->vertices.data(),
	                            static_cast<UInt32>(job->vertices.size()), out.cap, kFitCapsulesMax, kFitPercentile,
	                            kFitSplitFlatness, job->lengthSplitRatio);
	for (UInt32 i = 0; i < out.count; ++i) {
		out.frame[i] = job->boneNodes[out.cap[i].frameBone];
	}
	const long long fieldsFrom = ReadPerformanceCounter();
	if (out.count > 0) {
		out.fields = std::make_shared<const std::vector<RadialField>>(
			BuildFittedFields(job->bones.data(), static_cast<UInt32>(job->bones.size()), job->vertices.data(),
			                  static_cast<UInt32>(job->vertices.size()), out.cap, out.count));
	}
	const long long frequency = ReadPerformanceFrequency();
	out.fieldMilliseconds =
		frequency > 0 ? static_cast<double>(ReadPerformanceCounter() - fieldsFrom) * 1000.0 / static_cast<double>(frequency)
		              : 0.0;
	out.fitMilliseconds =
		frequency > 0 ? static_cast<double>(ReadPerformanceCounter() - started) * 1000.0 / static_cast<double>(frequency)
		              : 0.0;
}

DWORD __stdcall RunFit(void* parameter) {
	FitJob* const job = static_cast<FitJob*>(parameter);
	const long long started = ReadPerformanceCounter();
	try {
		ComputeFit(job, started);
	} catch (const std::bad_alloc&) {
		// out of memory: no capsules from this fit, the guessed ones stay
		job->result.count = 0;
		job->result.fields.reset();
		job->result.outOfMemory = true;
	}
	job->done.store(true, std::memory_order_release);
	RunningFits().fetch_sub(1);
	Release(job);
	return 0;
}

}  // namespace

namespace {

// A shape's vertices into the job, each given to the bone with its largest
// weight (in the body's list, `global`) and its worn item's material.
void AddShapeVertices(FitJob& job, const SkinnedShape& s, const std::vector<NiPoint3>& world,
                      const std::vector<UInt16>& global, UInt32 wornId) {
	UInt8 rating = 0;
	const UInt8 material = static_cast<UInt8>(WornMaterial(wornId, rating));
	std::vector<UInt16> dominant(s.vertexCount, kNoBone);
	std::vector<float> dominantWeight(s.vertexCount, 0.0f);
	DominantBones(s.weights.data(), static_cast<UInt32>(s.weights.size()), s.vertexCount, dominant.data(),
	              dominantWeight.data());
	for (UInt32 i = 0; i < s.vertexCount; ++i) {
		if (dominant[i] < global.size()) {
			job.vertices.push_back(FitVertex{world[i], global[dominant[i]], material, rating});
		}
	}
}

}  // namespace

namespace {

// The shapes under `root` read and skinned into `job` (its bones by name, its
// vertices by their strongest bone); the first refused shape's reason in
// `refusal`. May throw std::bad_alloc.
void ReadFitShapes(const NiAVObject* root, FitJob* job, char* refusal, UInt32 refusalSize, UInt32& refused) {
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(root, shapes);
	SkinnedShape s;
	// the shapes hang on one skeleton: its bones' pages asked of the system once
	ReadablePages pages;
	for (const ShapeUnder& under : shapes) {
		if (under.hidden) {
			continue;
		}
		// a decal left out before its skin is read: the engine lays them
		// on a body hit and takes them off again, their skins' bones not
		// always alive (the crash of 2026-10-27 17:05 was on one)
		if (LeftOut(ShapeName(under.geometry))) {
			continue;
		}
		char reason[160];
		const SkinReadResult read = ReadSkinnedShape(under.geometry, s, reason, sizeof(reason), &pages);
		if (read == SkinReadResult::Rigid) {
			continue;
		}
		if (read == SkinReadResult::Refused) {
			if (refused++ == 0) {
				std::snprintf(refusal, refusalSize, "\"%s\": %s", s.name, reason);
			}
			continue;
		}
		std::vector<NiTransform> boneWorld;
		std::vector<NiPoint3> world;
		if (!SkinShapeNow(s, boneWorld, world)) {
			if (refused++ == 0) {
				std::snprintf(refusal, refusalSize, "\"%s\": its vertices' weights do not add up to one", s.name);
			}
			continue;
		}
		// this shape's bones in the body's list, by name
		std::vector<UInt16> global(s.boneName.size());
		for (UInt32 b = 0; b < s.boneName.size(); ++b) {
			UInt32 found = 0;
			while (found < job->names.size() && !FitSameName(job->names[found].c_str(), s.boneName[b])) {
				++found;
			}
			global[b] = static_cast<UInt16>(found);
			if (found < job->names.size()) {
				continue;
			}
			// the world into the bone's own space, now: one node, one
			// transform, whichever skin names it
			job->names.emplace_back(s.boneName[b]);
			job->bones.push_back(FitBone{nullptr, InverseTransform(boneWorld[b])});
			job->boneNodes.push_back(s.boneNode[b]);
		}
		AddShapeVertices(*job, s, world, global, under.wornId);
		++job->result.shapes;
	}
	// the names' strings stay where they are now: point the bones at them
	for (UInt32 i = 0; i < job->bones.size(); ++i) {
		job->bones[i].name = job->names[i].c_str();
	}
}

}  // namespace

UInt32 ActorFitsRunning() { return RunningFits().load(); }

FitJob* StartActorFit(const NiAVObject* root, char* why, UInt32 whySize, float lengthSplitRatio,
                      const char* dumpPath) {
	const long long started = ReadPerformanceCounter();
	char refusal[160] = "";
	UInt32 refused = 0;
	FitJob* job = nullptr;
	try {
		job = new FitJob;
		job->lengthSplitRatio = lengthSplitRatio;
		ReadFitShapes(root, job, refusal, sizeof(refusal), refused);
	} catch (const std::bad_alloc&) {
		// out of memory reading the shapes: no fit, the guessed capsules stay
		delete job;
		std::snprintf(why, whySize, "out of memory reading the shapes - not fitted");
		return nullptr;
	}
	FittedBody& out = job->result;
	out.vertices = static_cast<UInt32>(job->vertices.size());
	const long long frequency = ReadPerformanceFrequency();
	out.readMilliseconds =
		frequency > 0 ? static_cast<double>(ReadPerformanceCounter() - started) * 1000.0 / static_cast<double>(frequency)
		              : 0.0;
	if (refused > 0) {
		std::snprintf(why, whySize, "%u shapes refused, the first %s", refused, refusal);
	} else if (why != nullptr && whySize > 0) {
		why[0] = '\0';
	}
	if (job->vertices.size() < kFitMinVertices) {
		std::snprintf(why, whySize, "%u skinned shapes read, %u vertices, %u bones - nothing to fit%s%s", out.shapes,
		              out.vertices, static_cast<UInt32>(job->bones.size()), refused > 0 ? "; refused " : "", refusal);
		delete job;
		return nullptr;
	}
	if (dumpPath != nullptr) {
		WriteFitDump(dumpPath, job->bones.data(), static_cast<UInt32>(job->bones.size()), job->vertices.data(),
		             static_cast<UInt32>(job->vertices.size()));
	}
	RunningFits().fetch_add(1);
	HANDLE thread = CreateThread(nullptr, 0, &RunFit, job, 0, nullptr);
	if (thread != nullptr) {
		CloseHandle(thread);
	} else {
		RunFit(job);  // no thread to be had: fitted here, done at once
	}
	return job;
}

bool PollActorFit(FitJob*& job, FittedBody& out) {
	if (job == nullptr || !job->done.load(std::memory_order_acquire)) {
		return false;
	}
	out = job->result;
	Release(job);
	job = nullptr;
	return true;
}

void AbandonActorFit(FitJob*& job) {
	if (job != nullptr) {
		Release(job);
		job = nullptr;
	}
}

UInt32 ReadActorMeshNow(const NiAVObject* root, std::vector<NiPoint3>& out, std::vector<BodyPart>* parts) {
	out.clear();
	if (parts != nullptr) {
		parts->clear();
	}
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(root, shapes);
	SkinnedShape s;
	std::vector<NiTransform> boneWorld;
	std::vector<NiPoint3> world;
	std::vector<UInt16> dominant;
	std::vector<float> dominantWeight;
	UInt32 read = 0;
	// the shapes hang on one skeleton: its bones' pages asked of the system once
	ReadablePages pages;
	for (const ShapeUnder& under : shapes) {
		if (under.hidden) {
			continue;
		}
		char reason[160];
		if (LeftOut(ShapeName(under.geometry)) ||
		    ReadSkinnedShape(under.geometry, s, reason, sizeof(reason), &pages) != SkinReadResult::Skinned ||
		    !SkinShapeNow(s, boneWorld, world)) {
			continue;
		}
		out.insert(out.end(), world.begin(), world.end());
		if (parts != nullptr) {
			dominant.assign(s.vertexCount, kNoBone);
			dominantWeight.assign(s.vertexCount, 0.0f);
			DominantBones(s.weights.data(), static_cast<UInt32>(s.weights.size()), s.vertexCount, dominant.data(),
			              dominantWeight.data());
			for (UInt32 i = 0; i < s.vertexCount; ++i) {
				const UInt8 group = dominant[i] < s.boneName.size() ? FitGroupOfBone(s.boneName[dominant[i]]) : kFitNoGroup;
				parts->push_back(group == kFitNoGroup ? BodyPart::Column : FitGroupInfoOf(group).part);
			}
		}
		++read;
	}
	return read;
}

namespace {

// A hand's bone: the hand itself or one of its fingers, on that side.
bool HandBone(const char* name, bool right) {
	const char* const hand = right ? "Bip01 R Hand" : "Bip01 L Hand";
	const char* const finger = right ? "Bip01 R Finger" : "Bip01 L Finger";
	return name != nullptr && (std::strncmp(name, hand, 12) == 0 || std::strncmp(name, finger, 14) == 0);
}

}  // namespace

UInt32 ReadHandVerticesNow(const NiAVObject* root, std::vector<NiPoint3>& right, std::vector<NiPoint3>& left) {
	right.clear();
	left.clear();
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(root, shapes);
	SkinnedShape s;
	std::vector<NiTransform> boneWorld;
	std::vector<NiPoint3> world;
	std::vector<UInt16> dominant;
	std::vector<float> dominantWeight;
	UInt32 read = 0;
	// the shapes hang on one skeleton: its bones' pages asked of the system once
	ReadablePages pages;
	for (const ShapeUnder& under : shapes) {
		if (under.hidden) {
			continue;
		}
		char reason[160];
		if (LeftOut(ShapeName(under.geometry)) ||
		    ReadSkinnedShape(under.geometry, s, reason, sizeof(reason), &pages) != SkinReadResult::Skinned) {
			continue;
		}
		// only a shape skinned to a hand's bones is skinned at all
		bool hands = false;
		for (const char* name : s.boneName) {
			hands = hands || HandBone(name, true) || HandBone(name, false);
		}
		if (!hands || !SkinShapeNow(s, boneWorld, world)) {
			continue;
		}
		dominant.assign(s.vertexCount, kNoBone);
		dominantWeight.assign(s.vertexCount, 0.0f);
		DominantBones(s.weights.data(), static_cast<UInt32>(s.weights.size()), s.vertexCount, dominant.data(),
		              dominantWeight.data());
		for (UInt32 i = 0; i < s.vertexCount; ++i) {
			if (dominant[i] >= s.boneName.size()) {
				continue;
			}
			const char* const bone = s.boneName[dominant[i]];
			if (HandBone(bone, true)) {
				right.push_back(world[i]);
			} else if (HandBone(bone, false)) {
				left.push_back(world[i]);
			}
		}
		++read;
	}
	return read;
}

UInt32 ActorOutfitPrint(const NiAVObject* root) {
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(root, shapes);
	std::vector<UInt32> words;
	for (const ShapeUnder& under : shapes) {
		if (under.hidden) {
			continue;
		}
		// the geometry's data and skin addresses only, not a full read: this
		// runs every second per actor. Rigid shapes are left out as the fit
		// leaves them out: a sword drawn or sheathed is no change of what
		// the body wears (the same actor fitted twice over it,
		// fitted-capsules.txt 2026-10-25 13:33)
		const UInt32 g = reinterpret_cast<UInt32>(under.geometry);
		const UInt32 skin = *reinterpret_cast<const UInt32*>(g + 0xB8);
		if (skin == 0 || LeftOut(ShapeName(under.geometry))) {
			continue;
		}
		words.push_back(g);
		words.push_back(*reinterpret_cast<const UInt32*>(g + 0xB4));
		words.push_back(skin);
	}
	return OutfitPrint(words.data(), static_cast<UInt32>(words.size()));
}

void DescribeFittedBody(const FittedBody& body, char* buf, UInt32 size) {
	if (size == 0) {
		return;
	}
	buf[0] = '\0';
	UInt32 at = 0;
	for (UInt32 i = 0; i < body.count && at + 48 < size; ++i) {
		const FittedCapsule& c = body.cap[i];
		const bool second = i > 0 && body.cap[i - 1].group == c.group;
		// a split group reads "torso 9.3+9.3 (flat 1.41)", any other "thigh
		// 6.5/1.12" - its radius and its flatness (the section's reach along
		// its major direction over its minor one)
		const int n = second ? std::snprintf(buf + at, size - at, "+%.1f (flat %.2f)", static_cast<double>(c.radius),
		                                     static_cast<double>(c.flatness))
		                     : std::snprintf(buf + at, size - at, "%s%s %.1f/%.2f", at > 0 ? ", " : "",
		                                     FitGroupInfoOf(c.group).name, static_cast<double>(c.radius),
		                                     static_cast<double>(c.flatness));
		at += n > 0 ? static_cast<UInt32>(n) : 0;
	}
}

}  // namespace obvr::game
