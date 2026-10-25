#include "game/FittedCapsules.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "game/BodyViewLogic.h"
#include "game/SkinRead.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// Shapes left out of the fit by name: the blood decals the engine lays on a
// body (skinned, a few dozen vertices each; worn-shapes.txt counted 16 after
// one fight).
bool LeftOut(const char* name) { return std::strncmp(name, "Decal:", 6) == 0; }

}  // namespace

bool FitActorBody(const NiAVObject* root, FittedBody& out, char* why, UInt32 whySize) {
	out = FittedBody{};
	const long long started = ReadPerformanceCounter();
	std::vector<ShapeUnder> shapes;
	CollectShapesUnder(root, shapes);
	std::vector<FitBone> bones;
	std::vector<const NiAVObject*> boneNodes;
	std::vector<FitVertex> vertices;
	SkinnedShape s;
	char refusal[160] = "";
	UInt32 refused = 0;
	for (const ShapeUnder& under : shapes) {
		if (under.hidden) {
			continue;
		}
		char reason[160];
		const SkinReadResult read = ReadSkinnedShape(under.geometry, s, reason, sizeof(reason));
		if (read == SkinReadResult::Rigid || LeftOut(s.name)) {
			continue;
		}
		if (read == SkinReadResult::Refused) {
			if (refused++ == 0) {
				std::snprintf(refusal, sizeof(refusal), "\"%s\": %s", s.name, reason);
			}
			continue;
		}
		std::vector<float> weightSum(s.vertexCount, 0.0f);
		for (const SkinWeight& w : s.weights) {
			weightSum[w.vertex] += w.weight;
		}
		if (SkinWeightMisses(weightSum.data(), s.vertexCount) != 0) {
			if (refused++ == 0) {
				std::snprintf(refusal, sizeof(refusal), "\"%s\": its vertices' weights do not add up to one", s.name);
			}
			continue;
		}
		// this shape's bones in the body's list, by name
		std::vector<UInt16> global(s.boneName.size());
		for (UInt32 b = 0; b < s.boneName.size(); ++b) {
			const int found = FitFindBone(bones.data(), static_cast<UInt32>(bones.size()), s.boneName[b]);
			if (found >= 0) {
				global[b] = static_cast<UInt16>(found);
				continue;
			}
			global[b] = static_cast<UInt16>(bones.size());
			bones.push_back(FitBone{s.boneName[b], s.skinToBone[b]});
			boneNodes.push_back(s.boneNode[b]);
		}
		std::vector<UInt16> dominant(s.vertexCount, kNoBone);
		std::vector<float> dominantWeight(s.vertexCount, 0.0f);
		DominantBones(s.weights.data(), static_cast<UInt32>(s.weights.size()), s.vertexCount, dominant.data(),
		              dominantWeight.data());
		for (UInt32 i = 0; i < s.vertexCount; ++i) {
			if (dominant[i] < global.size()) {
				vertices.push_back(FitVertex{s.bind[i], global[dominant[i]]});
			}
		}
		++out.shapes;
	}
	out.vertices = static_cast<UInt32>(vertices.size());
	out.count = FitBodyCapsules(bones.data(), static_cast<UInt32>(bones.size()), vertices.data(),
	                            static_cast<UInt32>(vertices.size()), out.cap, kFitCapsulesMax);
	for (UInt32 i = 0; i < out.count; ++i) {
		out.frame[i] = boneNodes[out.cap[i].frameBone];
	}
	const long long frequency = ReadPerformanceFrequency();
	out.milliseconds =
		frequency > 0 ? static_cast<double>(ReadPerformanceCounter() - started) * 1000.0 / static_cast<double>(frequency)
		              : 0.0;
	if (out.count == 0) {
		std::snprintf(why, whySize, "%u skinned shapes read, %u vertices, %u bones, no group fitted%s%s",
		              out.shapes, out.vertices, static_cast<UInt32>(bones.size()), refused > 0 ? "; refused " : "",
		              refusal);
		return false;
	}
	if (refused > 0) {
		std::snprintf(why, whySize, "%u shapes refused, the first %s", refused, refusal);
	} else if (why != nullptr && whySize > 0) {
		why[0] = '\0';
	}
	return true;
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
		// runs every second per actor
		const UInt32 g = reinterpret_cast<UInt32>(under.geometry);
		const char* const name = ShapeName(under.geometry);
		if (LeftOut(name)) {
			continue;
		}
		words.push_back(g);
		words.push_back(*reinterpret_cast<const UInt32*>(g + 0xB4));
		words.push_back(*reinterpret_cast<const UInt32*>(g + 0xB8));
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
		// a split group reads "torso 9.3+9.3 (flat 1.41)"
		const int n = second ? std::snprintf(buf + at, size - at, "+%.1f (flat %.2f)", static_cast<double>(c.radius),
		                                     static_cast<double>(c.flatness))
		                     : std::snprintf(buf + at, size - at, "%s%s %.1f", at > 0 ? ", " : "",
		                                     FitGroupInfoOf(c.group).name, static_cast<double>(c.radius));
		at += n > 0 ? static_cast<UInt32>(n) : 0;
	}
}

}  // namespace obvr::game
