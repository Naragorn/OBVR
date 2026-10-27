#include "game/FittedCapsules.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "game/BodyViewLogic.h"
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

}  // namespace

struct FitJob {
	std::vector<std::string> names;  // the bones' names, copied: the worker outlives no engine string
	std::vector<FitBone> bones;
	std::vector<const NiAVObject*> boneNodes;
	std::vector<FitVertex> vertices;
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

DWORD __stdcall RunFit(void* parameter) {
	FitJob* const job = static_cast<FitJob*>(parameter);
	const long long started = ReadPerformanceCounter();
	FittedBody& out = job->result;
	out.count = FitBodyCapsules(job->bones.data(), static_cast<UInt32>(job->bones.size()), job->vertices.data(),
	                            static_cast<UInt32>(job->vertices.size()), out.cap, kFitCapsulesMax);
	for (UInt32 i = 0; i < out.count; ++i) {
		out.frame[i] = job->boneNodes[out.cap[i].frameBone];
	}
	const long long frequency = ReadPerformanceFrequency();
	out.fitMilliseconds =
		frequency > 0 ? static_cast<double>(ReadPerformanceCounter() - started) * 1000.0 / static_cast<double>(frequency)
		              : 0.0;
	job->done.store(true, std::memory_order_release);
	Release(job);
	return 0;
}

}  // namespace

FitJob* StartActorFit(const NiAVObject* root, char* why, UInt32 whySize) {
	const long long started = ReadPerformanceCounter();
	auto* job = new FitJob;
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(root, shapes);
	SkinnedShape s;
	char refusal[160] = "";
	UInt32 refused = 0;
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
		const SkinReadResult read = ReadSkinnedShape(under.geometry, s, reason, sizeof(reason));
		if (read == SkinReadResult::Rigid) {
			continue;
		}
		if (read == SkinReadResult::Refused) {
			if (refused++ == 0) {
				std::snprintf(refusal, sizeof(refusal), "\"%s\": %s", s.name, reason);
			}
			continue;
		}
		std::vector<NiTransform> boneWorld;
		std::vector<NiPoint3> world;
		if (!SkinShapeNow(s, boneWorld, world)) {
			if (refused++ == 0) {
				std::snprintf(refusal, sizeof(refusal), "\"%s\": its vertices' weights do not add up to one", s.name);
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
		std::vector<UInt16> dominant(s.vertexCount, kNoBone);
		std::vector<float> dominantWeight(s.vertexCount, 0.0f);
		DominantBones(s.weights.data(), static_cast<UInt32>(s.weights.size()), s.vertexCount, dominant.data(),
		              dominantWeight.data());
		for (UInt32 i = 0; i < s.vertexCount; ++i) {
			if (dominant[i] < global.size()) {
				job->vertices.push_back(FitVertex{world[i], global[dominant[i]]});
			}
		}
		++job->result.shapes;
	}
	// the names' strings stay where they are now: point the bones at them
	for (UInt32 i = 0; i < job->bones.size(); ++i) {
		job->bones[i].name = job->names[i].c_str();
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

UInt32 ReadActorMeshNow(const NiAVObject* root, std::vector<NiPoint3>& out) {
	out.clear();
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(root, shapes);
	SkinnedShape s;
	std::vector<NiTransform> boneWorld;
	std::vector<NiPoint3> world;
	UInt32 read = 0;
	for (const ShapeUnder& under : shapes) {
		if (under.hidden) {
			continue;
		}
		char reason[160];
		if (LeftOut(ShapeName(under.geometry)) ||
		    ReadSkinnedShape(under.geometry, s, reason, sizeof(reason)) != SkinReadResult::Skinned ||
		    !SkinShapeNow(s, boneWorld, world)) {
			continue;
		}
		out.insert(out.end(), world.begin(), world.end());
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
	for (const ShapeUnder& under : shapes) {
		if (under.hidden) {
			continue;
		}
		char reason[160];
		if (LeftOut(ShapeName(under.geometry)) ||
		    ReadSkinnedShape(under.geometry, s, reason, sizeof(reason)) != SkinReadResult::Skinned) {
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
