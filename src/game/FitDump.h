#pragma once

// What a capsule fit is given - its bones and its skinned vertices - as
// text ([Debug] FitDump), so that the fit can be measured and tuned outside
// the game on a real outfit (tests/data/fit-*.txt, capsule_accuracy_test;
// Nadi, 2026-11-07: "wie können wir die genauigkeit verbessern? am ende
// sollten die tests passen").
//
// The format, one record a line:
//   B <name> <rot 9 floats, row by row> <pos 3> <scale>   a bone, the world
//                                                          into its space
//   V <x> <y> <z> <bone>                                   a vertex, world
// Bone names hold no spaces in the skeletons read so far ("Bip01 Spine1"
// does: its spaces are written as '_' and read back as ' ').

#include <cstdio>
#include <string>
#include <vector>

#include "game/CapsuleFitLogic.h"

namespace obvr::game {

inline bool WriteFitDump(const char* path, const FitBone* bones, UInt32 boneCount, const FitVertex* vertices,
                         UInt32 vertexCount) {
	if (path == nullptr || bones == nullptr || vertices == nullptr) {
		return false;
	}
	std::FILE* f = std::fopen(path, "w");
	if (f == nullptr) {
		return false;
	}
	for (UInt32 i = 0; i < boneCount; ++i) {
		std::string name = bones[i].name != nullptr ? bones[i].name : "";
		for (char& c : name) {
			c = c == ' ' ? '_' : c;
		}
		const NiTransform& t = bones[i].skinToBone;
		std::fprintf(f, "B %s", name.empty() ? "_" : name.c_str());
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				std::fprintf(f, " %.6g", static_cast<double>(t.rot.data[r][c]));
			}
		}
		std::fprintf(f, " %.6g %.6g %.6g %.6g\n", static_cast<double>(t.pos.x), static_cast<double>(t.pos.y),
		             static_cast<double>(t.pos.z), static_cast<double>(t.scale));
	}
	for (UInt32 i = 0; i < vertexCount; ++i) {
		std::fprintf(f, "V %.4f %.4f %.4f %u\n", static_cast<double>(vertices[i].bind.x),
		             static_cast<double>(vertices[i].bind.y), static_cast<double>(vertices[i].bind.z),
		             static_cast<unsigned>(vertices[i].bone));
	}
	return std::fclose(f) == 0;
}

// A dump read back: the bones' names live in `names`, the FitBones point at
// them. False when the file cannot be read or a line does not parse.
struct FitDumpData {
	std::vector<std::string> names;
	std::vector<FitBone> bones;
	std::vector<FitVertex> vertices;
};

inline bool ReadFitDump(const char* path, FitDumpData& out) {
	out = FitDumpData{};
	std::FILE* f = path != nullptr ? std::fopen(path, "r") : nullptr;
	if (f == nullptr) {
		return false;
	}
	char line[512];
	bool ok = true;
	while (ok && std::fgets(line, sizeof(line), f) != nullptr) {
		if (line[0] == 'B') {
			char name[128];
			NiTransform t{};
			const int n = std::sscanf(line, "B %127s %f %f %f %f %f %f %f %f %f %f %f %f %f", name, &t.rot.data[0][0],
			                          &t.rot.data[0][1], &t.rot.data[0][2], &t.rot.data[1][0], &t.rot.data[1][1],
			                          &t.rot.data[1][2], &t.rot.data[2][0], &t.rot.data[2][1], &t.rot.data[2][2],
			                          &t.pos.x, &t.pos.y, &t.pos.z, &t.scale);
			if (n != 14) {
				ok = false;
				break;
			}
			std::string s = name;
			for (char& c : s) {
				c = c == '_' ? ' ' : c;
			}
			out.names.push_back(s);
			out.bones.push_back(FitBone{nullptr, t});
		} else if (line[0] == 'V') {
			FitVertex v{};
			unsigned bone = 0;
			if (std::sscanf(line, "V %f %f %f %u", &v.bind.x, &v.bind.y, &v.bind.z, &bone) != 4) {
				ok = false;
				break;
			}
			v.bone = static_cast<UInt16>(bone);
			out.vertices.push_back(v);
		}
	}
	std::fclose(f);
	for (UInt32 i = 0; i < out.bones.size(); ++i) {
		out.bones[i].name = out.names[i].c_str();
	}
	return ok && !out.bones.empty() && !out.vertices.empty();
}

}  // namespace obvr::game
