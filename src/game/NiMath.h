#pragma once

#include "core/Types.h"

// The arithmetic types from Oblivion's Gamebryo scene graph.
//
// Deliberately separate from GameTypes.h: these types consist of floats only
// and have the same layout on every architecture. That makes rotation and
// quaternion maths testable natively on the development machine, while
// GameTypes.h with its pointer-bearing structs requires 32 bit.

namespace obvr {

struct NiPoint3 {
	float x;
	float y;
	float z;

	NiPoint3 operator+(const NiPoint3& rhs) const {
		return NiPoint3{x + rhs.x, y + rhs.y, z + rhs.z};
	}

	NiPoint3 operator-(const NiPoint3& rhs) const {
		return NiPoint3{x - rhs.x, y - rhs.y, z - rhs.z};
	}

	NiPoint3 operator*(float scale) const {
		return NiPoint3{x * scale, y * scale, z * scale};
	}

	float LengthSquared() const { return x * x + y * y + z * z; }
};

static_assert(sizeof(NiPoint3) == 0x0C, "NiPoint3 must be 0x0C bytes");

// Row major: data[row][column].
struct NiMatrix33 {
	float data[3][3];

	static NiMatrix33 Identity() {
		NiMatrix33 result{};
		result.data[0][0] = 1.0f;
		result.data[1][1] = 1.0f;
		result.data[2][2] = 1.0f;
		return result;
	}

	NiMatrix33 operator*(const NiMatrix33& rhs) const {
		NiMatrix33 result{};
		for (int row = 0; row < 3; ++row) {
			for (int col = 0; col < 3; ++col) {
				result.data[row][col] =
					data[row][0] * rhs.data[0][col] +
					data[row][1] * rhs.data[1][col] +
					data[row][2] * rhs.data[2][col];
			}
		}
		return result;
	}

	// Transforms a vector out of the space this matrix describes into its
	// parent space. Row major, so row i of the matrix meets the whole vector.
	//
	// This is what carries a head offset measured in camera space over into
	// the space the CameraNode's position lives in - the counterpart to the
	// rotation being multiplied on as rot * head.
	NiPoint3 operator*(const NiPoint3& v) const {
		return NiPoint3{
			data[0][0] * v.x + data[0][1] * v.y + data[0][2] * v.z,
			data[1][0] * v.x + data[1][1] * v.y + data[1][2] * v.z,
			data[2][0] * v.x + data[2][1] * v.y + data[2][2] * v.z,
		};
	}
};

static_assert(sizeof(NiMatrix33) == 0x24, "NiMatrix33 must be 0x24 bytes");

// The inverse of a 3x3 matrix: the adjugate over the determinant. A rotation's
// inverse is its transpose, but Oblivion's actor nodes do not always hold a
// rotation: a race's height is a scale laid into the player node's rotation
// matrix (an Altmer's third column reads 1.10 long, 2026-10-16, where an
// Imperial's reads 1.00), and a transpose taken as the inverse of that
// leaves the scale in every local derived from a world target - squared at
// each bone down the chain, which tore a tester's skeleton 2170 times apart
// within one frame and pushed his hands off the controllers. A matrix too
// close to singular to invert (a node collapsed to scale zero) gets its
// transpose, as before.
inline constexpr float kMatrixSingularDeterminant = 1.0e-12f;

inline NiMatrix33 InverseMatrix(const NiMatrix33& m) {
	const float a = m.data[0][0], b = m.data[0][1], c = m.data[0][2];
	const float d = m.data[1][0], e = m.data[1][1], f = m.data[1][2];
	const float g = m.data[2][0], h = m.data[2][1], i = m.data[2][2];
	const float cofA = e * i - f * h;
	const float cofB = f * g - d * i;
	const float cofC = d * h - e * g;
	const float det = a * cofA + b * cofB + c * cofC;
	if (!(det == det) || (det < kMatrixSingularDeterminant && det > -kMatrixSingularDeterminant)) {
		NiMatrix33 t{};
		for (int row = 0; row < 3; ++row) {
			for (int col = 0; col < 3; ++col) {
				t.data[row][col] = m.data[col][row];
			}
		}
		return t;
	}
	const float inv = 1.0f / det;
	NiMatrix33 r{};
	r.data[0][0] = cofA * inv;
	r.data[0][1] = (c * h - b * i) * inv;
	r.data[0][2] = (b * f - c * e) * inv;
	r.data[1][0] = cofB * inv;
	r.data[1][1] = (a * i - c * g) * inv;
	r.data[1][2] = (c * d - a * f) * inv;
	r.data[2][0] = cofC * inv;
	r.data[2][1] = (b * g - a * h) * inv;
	r.data[2][2] = (a * e - b * d) * inv;
	return r;
}

struct NiTransform {
	NiMatrix33 rot;    // 0x00
	NiPoint3 pos;      // 0x24
	float scale;       // 0x30
};

static_assert(sizeof(NiTransform) == 0x34, "NiTransform must be 0x34 bytes");

struct NiBound {
	NiPoint3 center;
	float radius;
};

static_assert(sizeof(NiBound) == 0x10, "NiBound must be 0x10 bytes");

}  // namespace obvr
