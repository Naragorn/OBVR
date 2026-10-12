#include "game/BodyView.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/BodyViewLogic.h"
#include "game/FirstPersonHide.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "game/PlayerBody.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// NiGeometry (xOBSE NiObjects.h): its property list's first entry at +0x9C,
// each entry {next, previous, property} (ArmStump.cpp walks it the same
// way); its data at +0xB4; its skin instance at +0xB8.
constexpr UInt32 kGeometryPropertyList = 0x9C;
constexpr UInt32 kGeometryData = 0xB4;
constexpr UInt32 kGeometrySkinInstance = 0xB8;
// NiGeometryData (xOBSE NiGeometry.h; the vertex fields measured by
// NearbyItems.cpp on 2026-09-29): the vertex count (UInt16) at +0x08, the
// vertices at +0x1C, the normals at +0x20. NiTriBasedGeomData's triangle
// count (UInt16) at +0x40; NiTriShapeData's index count at +0x44 and its
// indices at +0x48, three a triangle.
constexpr UInt32 kDataVertexCount = 0x08;
constexpr UInt32 kDataVertices = 0x1C;
constexpr UInt32 kDataNormals = 0x20;
constexpr UInt32 kDataTriangleCount = 0x40;
constexpr UInt32 kDataIndexCount = 0x44;
constexpr UInt32 kDataIndices = 0x48;
// NiSkinInstance: its NiSkinData at +0x08, its bones at +0x14 (ArmStump.cpp).
// NiSkinData: the bone count at +0x40, the bones' data at +0x44, 0x4C each:
// the skin-to-bone NiTransform at +0x00, the weights at +0x44 and their count
// at +0x48; a weight is {UInt16 vertex, two bytes, float weight} (xOBSE
// NiGeometry.h, NiSkinData::Bone and Weight). The count is a UInt16, not the
// UInt32 xOBSE declares: read as words on 2026-10-12, LowerBody's first bone
// held 0xBEA10081 there - 129 weights, and two bytes nobody cleared (Hand
// 0x00FF005A, Foot 0x6666002C, FaceGenFace 0x091B04FB: its 1275 vertices all
// on the head), where UpperBody's happened to be zero. Checked, not trusted:
// every vertex's weights have to add up to one (SkinWeightMisses), or the
// shape is left out and the log says so.
constexpr UInt32 kSkinInstanceData = 0x08;
constexpr UInt32 kSkinInstanceBones = 0x14;
constexpr UInt32 kSkinDataBoneCount = 0x40;
constexpr UInt32 kSkinDataBones = 0x44;
constexpr UInt32 kSkinBoneSize = 0x4C;
constexpr UInt32 kSkinBoneWeights = 0x44;
constexpr UInt32 kSkinBoneWeightCount = 0x48;
constexpr UInt32 kSkinWeightSize = 0x08;

constexpr UInt16 kHiddenBit = 0x0001;
constexpr UInt32 kMaxVertices = 8192;
constexpr UInt32 kMaxBones = 96;
constexpr UInt32 kMaxDepth = 16;

// The trunk: what a look down must not see into - the pelvis, the spine,
// the neck, the head (collapsed, with its share of the neck's vertices) and
// the clavicles, the bones the trunk's triangles of UpperBody, Arms and
// LowerBody hang from (pyffi, 2026-10-12; the neck's ring is Neck1 0.5, Head
// 0.15-0.39, Spine2 and a clavicle).
const char* const kTrunkBones[] = {"Bip01 Pelvis",  "Bip01 Spine", "Bip01 Spine1",     "Bip01 Spine2",     "Bip01 Neck",
                                   "Bip01 Neck1",   "Bip01 Head",  "Bip01 L Clavicle", "Bip01 R Clavicle"};

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
bool LooksLikeObject(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }
UInt16 Read16(UInt32 address) { return *reinterpret_cast<const UInt16*>(address); }

// A block of `bytes` bytes from `address` lies in readable memory, judged by
// its two ends: its start, and the word its last byte lies in (the address
// check wants words, aligned).
bool LooksLikeBlock(UInt32 address, UInt32 bytes) {
	return bytes > 0 && LooksLikeObject(address) && LooksLikeObject((address + bytes - 1) & ~3u);
}

const char* ReadableName(const void* object) {
	if (!LooksLikeObject(object)) {
		return "?";
	}
	const char* const name = *reinterpret_cast<const char* const*>(reinterpret_cast<const UInt8*>(object) + 0x08);
	if (!LooksLikeObject(name)) {
		return "?";
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const unsigned char c = static_cast<unsigned char>(name[at]);
		if (c == '\0') {
			return at > 0 ? name : "?";
		}
		if (c < 0x20 || c > 0x7E) {
			return "?";
		}
	}
	return "?";
}

bool SameName(const char* a, const char* b) {
	for (; *a != '\0' && *b != '\0'; ++a, ++b) {
		char x = *a;
		char y = *b;
		x = (x >= 'A' && x <= 'Z') ? static_cast<char>(x - 'A' + 'a') : x;
		y = (y >= 'A' && y <= 'Z') ? static_cast<char>(y - 'A' + 'a') : y;
		if (x != y) {
			return false;
		}
	}
	return *a == *b;
}

bool StartsWith(const char* text, const char* prefix) {
	for (; *prefix != '\0'; ++text, ++prefix) {
		if (*text != *prefix) {
			return false;
		}
	}
	return true;
}

bool IsTrunkBone(const char* name) {
	for (const char* trunk : kTrunkBones) {
		if (SameName(name, trunk)) {
			return true;
		}
	}
	return false;
}

// Whether a shape carries a NiStencilProperty: in the vanilla body only the
// loincloth does, and it is drawn from both sides (pyffi: draw mode 3). Its
// draw mode is not read here - a stencil property drawn from one side would
// be taken as two-sided, which can only hide an inside, not invent one.
bool HasStencilProperty(UInt32 geometry) {
	UInt32 entry = Read(geometry + kGeometryPropertyList);
	for (UInt32 guard = 0; guard < 16 && LooksLikeObject(entry); ++guard) {
		const UInt32 property = Read(entry + 8);
		if (LooksLikeObject(property) && StartsWith(NiClassNameOf(reinterpret_cast<const void*>(property)), "NiStencilProperty")) {
			return true;
		}
		entry = Read(entry);
	}
	return false;
}

struct ShapeRead {
	const char* name = "?";
	UInt32 vertices = 0;
	UInt32 triangles = 0;
	UInt32 bones = 0;  // 0 for a rigid shape
	UInt32 agree = 0;
	UInt32 against = 0;
	UInt32 trunkTriangles = 0;
	bool twoSided = false;
	const char* refused = nullptr;  // why it was left out
	// The geometry data's fields as read, for the log: what a refusal saw.
	char fields[256] = "";
};

struct Collected {
	std::vector<ViewTriangle> triangles;
	std::vector<ShapeRead> shapes;
	UInt32 strips = 0;   // NiTriStrips shapes, not read
	UInt32 tooMany = 0;  // shapes past kViewMaxShapes
};

void ReadShape(const NiAVObject* geometry, Collected& out) {
	ShapeRead s;
	s.name = ReadableName(geometry);
	if (out.shapes.size() >= kViewMaxShapes) {
		++out.tooMany;
		return;
	}
	const UInt8 shapeIndex = static_cast<UInt8>(out.shapes.size());
	const UInt32 g = reinterpret_cast<UInt32>(geometry);
	const UInt32 data = Read(g + kGeometryData);
	if (!LooksLikeObject(data)) {
		s.refused = "its geometry data does not read";
		out.shapes.push_back(s);
		return;
	}
	s.vertices = Read16(data + kDataVertexCount);
	s.triangles = Read16(data + kDataTriangleCount);
	const UInt32 vertexAt = Read(data + kDataVertices);
	const UInt32 normalAt = Read(data + kDataNormals);
	const UInt32 indexCount = Read(data + kDataIndexCount);
	const UInt32 indexAt = Read(data + kDataIndices);
	std::snprintf(s.fields, sizeof(s.fields),
	              "%s: %u vertices at %08X, normals at %08X, %u triangles, %u indices at %08X, keep flags %02X, compress "
	              "flags %02X",
	              NiClassNameOf(reinterpret_cast<const void*>(data)), s.vertices, vertexAt, normalAt, s.triangles,
	              indexCount, indexAt, *reinterpret_cast<const UInt8*>(data + 0x30),
	              *reinterpret_cast<const UInt8*>(data + 0x31));
	if (s.vertices == 0 || s.vertices > kMaxVertices || !LooksLikeBlock(vertexAt, s.vertices * 12)) {
		s.refused = "its vertices do not read";
		out.shapes.push_back(s);
		return;
	}
	if (s.triangles == 0 || indexCount != s.triangles * 3 || !LooksLikeBlock(indexAt, indexCount * 2)) {
		s.refused = "its triangles do not read (the index count is not three a triangle)";
		out.shapes.push_back(s);
		return;
	}
	const auto* bind = reinterpret_cast<const NiPoint3*>(vertexAt);
	const auto* indices = reinterpret_cast<const UInt16*>(indexAt);
	for (UInt32 i = 0; i < indexCount; ++i) {
		if (indices[i] >= s.vertices) {
			s.refused = "a triangle names a vertex past the count";
			out.shapes.push_back(s);
			return;
		}
	}
	const NiPoint3* normals =
		LooksLikeBlock(normalAt, s.vertices * 12) ? reinterpret_cast<const NiPoint3*>(normalAt) : nullptr;
	const int winding = OutwardWinding(bind, normals, s.vertices, indices, s.triangles, s.agree, s.against);
	if (winding == 0) {
		s.refused = normals == nullptr ? "it has no normals to tell its outside by"
		                               : "its normals do not tell its outside (as many triangles each way)";
		out.shapes.push_back(s);
		return;
	}
	s.twoSided = HasStencilProperty(g);

	std::vector<NiPoint3> world(s.vertices);
	std::vector<UInt16> dominant;  // empty for a rigid shape
	std::vector<float> dominantWeight;
	std::vector<bool> trunkBone;
	const UInt32 skin = Read(g + kGeometrySkinInstance);
	if (skin == 0) {
		for (UInt32 i = 0; i < s.vertices; ++i) {
			world[i] = TransformPoint(geometry->worldTransform, bind[i]);
		}
	} else {
		const UInt32 skinData = LooksLikeObject(skin) ? Read(skin + kSkinInstanceData) : 0;
		const UInt32 boneAt = LooksLikeObject(skin) ? Read(skin + kSkinInstanceBones) : 0;
		if (!LooksLikeObject(skinData)) {
			s.refused = "its skin data does not read";
			out.shapes.push_back(s);
			return;
		}
		s.bones = Read(skinData + kSkinDataBoneCount);
		const UInt32 boneDataAt = Read(skinData + kSkinDataBones);
		if (s.bones == 0 || s.bones > kMaxBones || !LooksLikeBlock(boneAt, s.bones * 4) ||
		    !LooksLikeBlock(boneDataAt, s.bones * kSkinBoneSize)) {
			s.refused = "its skin's bones do not read";
			out.shapes.push_back(s);
			return;
		}
		std::vector<NiTransform> boneWorld(s.bones);
		std::vector<NiTransform> skinToBone(s.bones);
		std::vector<SkinWeight> weights;
		trunkBone.assign(s.bones, false);
		for (UInt32 b = 0; b < s.bones; ++b) {
			const NiAVObject* const bone = reinterpret_cast<const NiAVObject*>(Read(boneAt + b * 4));
			if (!LooksLikeObject(bone)) {
				s.refused = "a bone of its skin does not read";
				out.shapes.push_back(s);
				return;
			}
			boneWorld[b] = bone->worldTransform;
			trunkBone[b] = IsTrunkBone(ReadableName(bone));
			const UInt32 entry = boneDataAt + b * kSkinBoneSize;
			skinToBone[b] = *reinterpret_cast<const NiTransform*>(entry);
			const UInt32 weightAt = Read(entry + kSkinBoneWeights);
			const UInt32 weightCount = Read16(entry + kSkinBoneWeightCount);
			if (weightCount > s.vertices || (weightCount > 0 && !LooksLikeBlock(weightAt, weightCount * kSkinWeightSize))) {
				const UInt32 used = static_cast<UInt32>(std::strlen(s.fields));
				std::snprintf(s.fields + used, sizeof(s.fields) - used, "; bone %u \"%s\" of %u: %u weights at %08X", b,
				              ReadableName(bone), s.bones, weightCount, weightAt);
				s.refused = "a bone's weights do not read";
				out.shapes.push_back(s);
				return;
			}
			for (UInt32 k = 0; k < weightCount; ++k) {
				SkinWeight w;
				w.vertex = Read16(weightAt + k * kSkinWeightSize);
				w.bone = static_cast<UInt16>(b);
				w.weight = *reinterpret_cast<const float*>(weightAt + k * kSkinWeightSize + 4);
				weights.push_back(w);
			}
		}
		std::vector<float> weightSum(s.vertices);
		SkinVertices(bind, s.vertices, boneWorld.data(), skinToBone.data(), s.bones, weights.data(),
		             static_cast<UInt32>(weights.size()), world.data(), weightSum.data());
		if (SkinWeightMisses(weightSum.data(), s.vertices) != 0) {
			s.refused = "its vertices' weights do not add up to one - the skin data did not read as expected";
			out.shapes.push_back(s);
			return;
		}
		dominant.assign(s.vertices, kNoBone);
		dominantWeight.assign(s.vertices, 0.0f);
		DominantBones(weights.data(), static_cast<UInt32>(weights.size()), s.vertices, dominant.data(),
		              dominantWeight.data());
	}
	for (UInt32 t = 0; t < s.triangles; ++t) {
		const UInt16 ia = indices[t * 3];
		const UInt16 ib = winding > 0 ? indices[t * 3 + 1] : indices[t * 3 + 2];
		const UInt16 ic = winding > 0 ? indices[t * 3 + 2] : indices[t * 3 + 1];
		ViewTriangle tri;
		tri.a = world[ia];
		tri.b = world[ib];
		tri.c = world[ic];
		tri.shape = shapeIndex;
		tri.twoSided = s.twoSided;
		if (!dominant.empty()) {
			const UInt16 bone = TriangleBone(dominant[ia], dominant[ib], dominant[ic], dominantWeight[ia],
			                                 dominantWeight[ib], dominantWeight[ic]);
			tri.trunk = bone != kNoBone && bone < trunkBone.size() && trunkBone[bone];
		}
		if (tri.trunk) {
			++s.trunkTriangles;
		}
		out.triangles.push_back(tri);
	}
	out.shapes.push_back(s);
}

// The shapes the draw will show: not hidden, nor under anything hidden.
void Collect(const NiAVObject* node, UInt32 depth, Collected& out) {
	if (!LooksLikeObject(node) || depth > kMaxDepth || (node->flags & kHiddenBit) != 0) {
		return;
	}
	const char* const className = NiClassNameOf(node);
	if (NiClassIsNode(className)) {
		UInt32 count = 0;
		NiAVObject* const* const children = NodeChildren(node, count);
		for (UInt32 i = 0; children != nullptr && i < count; ++i) {
			if (children[i] != nullptr) {
				Collect(children[i], depth + 1, out);
			}
		}
		return;
	}
	if (StartsWith(className, "NiTriShape@")) {
		ReadShape(node, out);
	} else if (StartsWith(className, "NiTriStrips@")) {
		++out.strips;
	}
}

// ------------------------------------------------------------------ the picture

constexpr UInt32 kPictureScale = 3;
constexpr UInt32 kPictureGap = 6;

void PutU32(UInt8* at, UInt32 value) {
	at[0] = static_cast<UInt8>(value);
	at[1] = static_cast<UInt8>(value >> 8);
	at[2] = static_cast<UInt8>(value >> 16);
	at[3] = static_cast<UInt8>(value >> 24);
}

// The colour of one ray, red-green-blue: green the trunk from outside, blue
// a limb, red the trunk's inside, orange a limb's, grey nothing.
void ColourOf(UInt8 seen, UInt8 rgb[3]) {
	static const UInt8 kColours[kViewSeenKinds][3] = {
		{40, 40, 40}, {70, 130, 230}, {60, 200, 80}, {240, 160, 40}, {235, 40, 40},
	};
	const UInt8* c = kColours[seen < kViewSeenKinds ? seen : 0];
	rgb[0] = c[0];
	rgb[1] = c[1];
	rgb[2] = c[2];
}

bool WritePicture(const char* path, const ViewGrid& grid, const std::vector<UInt8> (&cells)[2]) {
	const UInt32 allRows = grid.rows + grid.marginRows;
	const UInt32 eyeWidth = grid.columns * kPictureScale;
	const UInt32 width = eyeWidth * 2 + kPictureGap;
	const UInt32 height = allRows * kPictureScale;
	std::vector<UInt8> pixels(width * height * 4, 0);
	for (UInt32 eye = 0; eye < 2; ++eye) {
		const UInt32 left = eye == 0 ? 0 : eyeWidth + kPictureGap;
		for (UInt32 r = 0; r < allRows; ++r) {
			for (UInt32 c = 0; c < grid.columns; ++c) {
				UInt8 rgb[3];
				ColourOf(cells[eye][r * grid.columns + c], rgb);
				if (r >= grid.rows) {
					for (UInt8& v : rgb) {
						v = static_cast<UInt8>(v * 11 / 20);
					}
				}
				for (UInt32 dy = 0; dy < kPictureScale; ++dy) {
					for (UInt32 dx = 0; dx < kPictureScale; ++dx) {
						UInt8* const p = &pixels[((r * kPictureScale + dy) * width + left + c * kPictureScale + dx) * 4];
						const bool edge = r == grid.rows && dy == 0;
						p[0] = edge ? 200 : rgb[2];
						p[1] = edge ? 200 : rgb[1];
						p[2] = edge ? 200 : rgb[0];
						p[3] = 255;
					}
				}
			}
		}
	}
	FILE* file = nullptr;
	if (fopen_s(&file, path, "wb") != 0 || file == nullptr) {
		return false;
	}
	const UInt32 imageBytes = width * height * 4;
	UInt8 header[54] = {};
	header[0] = 'B';
	header[1] = 'M';
	PutU32(header + 2, 54 + imageBytes);
	PutU32(header + 10, 54);
	PutU32(header + 14, 40);
	PutU32(header + 18, width);
	PutU32(header + 22, height);
	header[26] = 1;
	header[28] = 32;
	PutU32(header + 34, imageBytes);
	bool ok = std::fwrite(header, 1, sizeof(header), file) == sizeof(header);
	for (UInt32 row = 0; ok && row < height; ++row) {
		ok = std::fwrite(&pixels[(height - 1 - row) * width * 4], 4, width, file) == width;
	}
	std::fclose(file);
	return ok;
}

// ------------------------------------------------------------------ the log

const NiAVObject* g_shapesReportedFor = nullptr;
UInt32 g_noBodyLines = 2;

float Percent(const ViewTally& t, UInt8 kind) { return 100.0f * ViewShare(t, kind); }

void LogEye(const char* mark, const char* side, const ViewEye& eye, const ViewTally& t, const Collected& c) {
	char inside[256] = "";
	UInt32 used = 0;
	for (UInt32 i = 0; i < c.shapes.size() && i < kViewMaxShapes; ++i) {
		if (t.trunkInsideOf[i] == 0 || used + 48 >= sizeof(inside)) {
			continue;
		}
		const int n = std::snprintf(inside + used, sizeof(inside) - used, "%s\"%s\" %.2f%%", used == 0 ? " (" : ", ",
		                            c.shapes[i].name,
		                            static_cast<double>(100.0f * static_cast<float>(t.trunkInsideOf[i]) /
		                                                static_cast<float>(t.rays > 0 ? t.rays : 1)));
		if (n > 0) {
			used += static_cast<UInt32>(n);
		}
	}
	if (used > 0 && used + 1 < sizeof(inside)) {
		inside[used] = ')';
		inside[used + 1] = '\0';
	}
	char below[96];
	if (t.trunkInsideBelowDegrees >= 0.0f) {
		std::snprintf(below, sizeof(below), "the trunk's inside %.1f degrees below the view's bottom edge",
		              static_cast<double>(t.trunkInsideBelowDegrees));
	} else {
		std::snprintf(below, sizeof(below), "no trunk's inside within the margin below the view");
	}
	OBVR_LOG("Body view: at \"%s\", the %s eye at %.1f %.1f %.1f - of %u rays the trunk from outside %.2f%%, from inside "
	         "%.2f%%%s; the limbs from outside %.2f%%, from inside %.2f%%; nothing %.2f%%; %s",
	         mark, side, static_cast<double>(eye.pos.x), static_cast<double>(eye.pos.y), static_cast<double>(eye.pos.z),
	         t.rays, static_cast<double>(Percent(t, kViewSeenTrunkOutside)),
	         static_cast<double>(Percent(t, kViewSeenTrunkInside)), inside,
	         static_cast<double>(Percent(t, kViewSeenLimbOutside)), static_cast<double>(Percent(t, kViewSeenLimbInside)),
	         static_cast<double>(Percent(t, kViewSeenNothing)), below);
}

void LogShapes(const Collected& c) {
	for (const ShapeRead& s : c.shapes) {
		if (s.refused != nullptr) {
			OBVR_LOG("Body view: left out, shape \"%s\" - %s (%s)", s.name, s.refused, s.fields);
			continue;
		}
		OBVR_LOG("Body view: shape \"%s\" - %u vertices, %u triangles (%u of them the trunk's), %s, wound %s by its "
		         "normals (%u of %u triangles), %s (%s)",
		         s.name, s.vertices, s.triangles, s.trunkTriangles, s.bones > 0 ? "skinned, every vertex weighing one" : "rigid",
		         s.agree >= s.against ? "outward" : "inward", s.agree >= s.against ? s.agree : s.against,
		         s.agree + s.against, s.twoSided ? "drawn from both sides (a NiStencilProperty)" : "drawn from one side",
		         s.fields);
	}
	if (c.strips > 0 || c.tooMany > 0) {
		OBVR_LOG("Body view: %u strip shape(s) not read (NiTriStrips), %u shape(s) past the first %u not cast", c.strips,
		         c.tooMany, kViewMaxShapes);
	}
}

void SafeFileName(const char* mark, char* out, UInt32 size) {
	UInt32 at = 0;
	for (; mark[at] != '\0' && at + 1 < size; ++at) {
		const char ch = mark[at];
		const bool plain = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
		                   ch == '-' || ch == '_';
		out[at] = plain ? ch : '_';
	}
	out[at] = '\0';
}

}  // namespace

bool MeasureBodyView(const BodyViewFrame& frame) {
	const long long started = ReadPerformanceCounter();
	const char* const mark = frame.mark != nullptr ? frame.mark : "";
	NiAVObject* const root = PlayerBodyRoot();
	if (root == nullptr) {
		if (g_noBodyLines > 0) {
			--g_noBodyLines;
			OBVR_LOG("Body view: not measured at \"%s\" - the body's root does not read", mark);
		}
		return false;
	}
	NiFrustum frustum{};
	if (!ReadGameCameraFrustum(frustum) || !(frustum.r > 0.0f) || !(frustum.l < 0.0f) || !(frustum.t > 0.0f) ||
	    !(frustum.b < 0.0f) || !(frustum.n > 0.0f)) {
		OBVR_LOG("Body view: not measured at \"%s\" - the camera's frustum does not read as a view (l %.3f r %.3f t "
		         "%.3f b %.3f n %.2f)",
		         mark, static_cast<double>(frustum.l), static_cast<double>(frustum.r), static_cast<double>(frustum.t),
		         static_cast<double>(frustum.b), static_cast<double>(frustum.n));
		return false;
	}

	Collected collected;
	Collect(root, 0, collected);
	if (root != g_shapesReportedFor) {
		g_shapesReportedFor = root;
		LogShapes(collected);
	}

	ViewEye eyes[2];
	const NiMatrix33& m = frame.rotation;
	const NiPoint3 first = frame.cyclopean + frame.firstEyeStep;
	const NiPoint3 second = frame.cyclopean - frame.firstEyeStep;
	for (UInt32 e = 0; e < 2; ++e) {
		ViewEye& eye = eyes[e];
		eye.right = NiPoint3{m.data[0][0], m.data[1][0], m.data[2][0]};
		eye.forward = NiPoint3{m.data[0][1], m.data[1][1], m.data[2][1]};
		eye.up = NiPoint3{m.data[0][2], m.data[1][2], m.data[2][2]};
		eye.tanLeft = -frustum.l;
		eye.tanRight = frustum.r;
		eye.tanUp = frustum.t;
		eye.tanDown = -frustum.b;
		eye.nearUnits = frustum.n;
		const bool left = e == 0;
		eye.pos = left == frame.firstIsLeft ? first : second;
	}
	const ViewGrid grid{};
	const UInt32 cellCount = grid.columns * (grid.rows + grid.marginRows);
	std::vector<UInt8> cells[2] = {std::vector<UInt8>(cellCount), std::vector<UInt8>(cellCount)};
	const UInt32 triangleCount = static_cast<UInt32>(collected.triangles.size());
	const ViewTally tally[2] = {
		CastBodyView(eyes[0], grid, collected.triangles.data(), triangleCount, cells[0].data()),
		CastBodyView(eyes[1], grid, collected.triangles.data(), triangleCount, cells[1].data()),
	};
	UInt32 trunkTriangles = 0;
	UInt32 cast = 0;
	for (const ShapeRead& s : collected.shapes) {
		if (s.refused == nullptr) {
			++cast;
			trunkTriangles += s.trunkTriangles;
		}
	}
	const long long perSecond = ReadPerformanceFrequency();
	const double ms = perSecond > 0 ? 1000.0 * static_cast<double>(ReadPerformanceCounter() - started) /
	                                      static_cast<double>(perSecond)
	                                : 0.0;
	OBVR_LOG("Body view: at \"%s\" - %u shape(s) cast, %u triangles (%u the trunk's), %u left out; the view %.1f "
	         "degrees left to %.1f right, %.1f up to %.1f down, the near plane %.1f units; %ux%u rays an eye, %u rows "
	         "%.0f degrees below it; %.1f ms",
	         mark, cast, triangleCount, trunkTriangles, static_cast<UInt32>(collected.shapes.size()) - cast,
	         static_cast<double>(math::Atan(eyes[0].tanLeft) * math::kRadiansToDegrees),
	         static_cast<double>(math::Atan(eyes[0].tanRight) * math::kRadiansToDegrees),
	         static_cast<double>(math::Atan(eyes[0].tanUp) * math::kRadiansToDegrees),
	         static_cast<double>(math::Atan(eyes[0].tanDown) * math::kRadiansToDegrees),
	         static_cast<double>(eyes[0].nearUnits), grid.columns, grid.rows, grid.marginRows,
	         static_cast<double>(grid.marginDegrees), ms);
	LogEye(mark, "left", eyes[0], tally[0], collected);
	LogEye(mark, "right", eyes[1], tally[1], collected);

	const ViewVerdict verdict = BodyViewVerdict(tally[0], tally[1]);
	if (verdict == ViewVerdict::IntoTheBody) {
		OBVR_LOG("Body view: into the body at \"%s\" - the trunk's inside on %.2f%% of the left eye's view and %.2f%% "
		         "of the right's",
		         mark, static_cast<double>(Percent(tally[0], kViewSeenTrunkInside)),
		         static_cast<double>(Percent(tally[1], kViewSeenTrunkInside)));
	} else if (verdict == ViewVerdict::TrunkOutOfView) {
		OBVR_LOG("Body view: the trunk nowhere in view at \"%s\" - from outside on %.2f%% of the left eye's view and "
		         "%.2f%% of the right's (%.1f%% wanted), its inside nowhere",
		         mark, static_cast<double>(Percent(tally[0], kViewSeenTrunkOutside)),
		         static_cast<double>(Percent(tally[1], kViewSeenTrunkOutside)),
		         static_cast<double>(100.0f * kViewTrunkSeenShare));
	} else {
		float below = -1.0f;
		for (const ViewTally& t : tally) {
			if (t.trunkInsideBelowDegrees >= 0.0f && (below < 0.0f || t.trunkInsideBelowDegrees < below)) {
				below = t.trunkInsideBelowDegrees;
			}
		}
		char margin[96];
		if (below >= 0.0f) {
			std::snprintf(margin, sizeof(margin), "the trunk's inside %.1f degrees below their bottom edge",
			              static_cast<double>(below));
		} else {
			std::snprintf(margin, sizeof(margin), "no inside within %.0f degrees below their bottom edge",
			              static_cast<double>(grid.marginDegrees));
		}
		OBVR_LOG("Body view: down the body at \"%s\" - the trunk from outside on %.1f%% of the left eye's view and "
		         "%.1f%% of the right's, its inside nowhere in them; %s",
		         mark, static_cast<double>(Percent(tally[0], kViewSeenTrunkOutside)),
		         static_cast<double>(Percent(tally[1], kViewSeenTrunkOutside)), margin);
	}

	char name[96];
	char safe[64];
	char path[512];
	SafeFileName(mark, safe, sizeof(safe));
	std::snprintf(name, sizeof(name), "OBVR-BodyView-%s.bmp", safe);
	if (platform::BuildGamePath(name, path, sizeof(path)) && WritePicture(path, grid, cells)) {
		OBVR_LOG("Body view: the rays' picture saved as %s (left eye left; green the trunk from outside, blue a limb, red "
		         "the trunk's inside, orange a limb's; below the grey line the rows under the view)",
		         name);
	}
	return true;
}

}  // namespace obvr::game
