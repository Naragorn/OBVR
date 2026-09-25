#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "render/EyeGeometry.h"
#include "vr/OpenVRTypes.h"

namespace obvr::render {

// The arithmetic of drawing a controller's render model into an eye's
// picture, on the CPU: every vertex carried from the model through the
// controller's pose into the tracking space, from there into the eye, and
// projected onto the eye texture's pixels. The draw then needs no transform
// state at all - pre-transformed vertices, the way the menu shade is drawn.
//
// All in OpenVR's own conventions: matrices are 3x4, row-major, the
// translation in the last column; the eye looks down -z with y up.

inline vr::openvr::HmdMatrix34 ComposeRigid(const vr::openvr::HmdMatrix34& a,
                                            const vr::openvr::HmdMatrix34& b) {
	vr::openvr::HmdMatrix34 out{};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 4; ++c) {
			float sum = 0.0f;
			for (int k = 0; k < 3; ++k) {
				sum += a.m[r][k] * b.m[k][c];
			}
			out.m[r][c] = sum + (c == 3 ? a.m[r][3] : 0.0f);
		}
	}
	return out;
}

// The inverse of a rotation and translation: the transposed rotation, and
// the translation taken back through it.
inline vr::openvr::HmdMatrix34 InvertRigid(const vr::openvr::HmdMatrix34& a) {
	vr::openvr::HmdMatrix34 out{};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			out.m[r][c] = a.m[c][r];
		}
	}
	for (int r = 0; r < 3; ++r) {
		out.m[r][3] = -(out.m[r][0] * a.m[0][3] + out.m[r][1] * a.m[1][3] +
		                out.m[r][2] * a.m[2][3]);
	}
	return out;
}

inline NiPoint3 TransformPoint(const vr::openvr::HmdMatrix34& a, const NiPoint3& p) {
	return NiPoint3{a.m[0][0] * p.x + a.m[0][1] * p.y + a.m[0][2] * p.z + a.m[0][3],
	                a.m[1][0] * p.x + a.m[1][1] * p.y + a.m[1][2] * p.z + a.m[1][3],
	                a.m[2][0] * p.x + a.m[2][1] * p.y + a.m[2][2] * p.z + a.m[2][3]};
}

// Where a point in the eye's space lands on the eye's texture, in pixels,
// with the depth for a depth test (0 at the near plane, 1 at the far) and
// the 1/w a pre-transformed vertex carries. False for a point nearer than
// the near plane - behind the eye, or too close to project.
//
// The frustum as OpenVR's GetProjectionRaw gives it. The vertical pair is
// named backwards (docs/vr-modding/rendering-and-stereo.md: "top" is the
// lower edge's tangent, negative; confirmed in a headset for the picture
// placement, OpticalCentreV's topIsNegative): the texture's top row is at
// the tangent called "bottom".
struct EyePixel {
	float x = 0.0f;
	float y = 0.0f;
	float depth = 0.0f;
	float rhw = 0.0f;
};

inline bool ProjectToEyePixel(const NiPoint3& eyeSpace, const EyeProjection& frustum,
                              float width, float height, float nearMetres, float farMetres,
                              EyePixel& out) {
	const float forward = -eyeSpace.z;
	const float across = frustum.right - frustum.left;
	const float down = frustum.bottom - frustum.top;
	if (!(forward >= nearMetres) || across == 0.0f || down == 0.0f || !(farMetres > nearMetres)) {
		return false;
	}
	const float tanRight = eyeSpace.x / forward;
	const float tanUp = eyeSpace.y / forward;
	out.x = (tanRight - frustum.left) / across * width;
	out.y = (frustum.bottom - tanUp) / down * height;
	float depth = (forward - nearMetres) / (farMetres - nearMetres);
	if (depth > 1.0f) {
		depth = 1.0f;
	}
	out.depth = depth;
	out.rhw = 1.0f / forward;
	return true;
}

// Drawing into the world instead, while an eye's picture is still in the back
// buffer with the game's depth beside it: the point goes through the game's
// own projection, so the model is hidden by what stands in front of it
// exactly as the hands are. The point is in the camera node's frame as OBVR
// uses it everywhere (x right, y forward, z up, game units); Direct3D's view
// space is x right, y up, z forward, row vectors times the matrix.
inline bool ProjectThroughD3D(const NiPoint3& cameraSpace, const float (&p)[4][4], float viewX,
                              float viewY, float viewWidth, float viewHeight, float minZ,
                              float maxZ, EyePixel& out) {
	const float vx = cameraSpace.x;
	const float vy = cameraSpace.z;
	const float vz = cameraSpace.y;
	const float cx = vx * p[0][0] + vy * p[1][0] + vz * p[2][0] + p[3][0];
	const float cy = vx * p[0][1] + vy * p[1][1] + vz * p[2][1] + p[3][1];
	const float cz = vx * p[0][2] + vy * p[1][2] + vz * p[2][2] + p[3][2];
	const float cw = vx * p[0][3] + vy * p[1][3] + vz * p[2][3] + p[3][3];
	if (!(cw > 1.0e-4f)) {
		return false;
	}
	const float nx = cx / cw;
	const float ny = cy / cw;
	float nz = cz / cw;
	// Nearer than the near plane - with a hair of slack for a point on it,
	// which float rounding can leave a millionth short.
	if (!(nz >= -1.0e-4f)) {
		return false;
	}
	if (nz < 0.0f) {
		nz = 0.0f;
	}
	out.x = viewX + (nx + 1.0f) * 0.5f * viewWidth;
	out.y = viewY + (1.0f - ny) * 0.5f * viewHeight;
	out.depth = minZ + (nz > 1.0f ? 1.0f : nz) * (maxZ - minZ);
	out.rhw = 1.0f / cw;
	return true;
}

// Whether a matrix read off the device is the world camera's perspective
// projection: w taken from the forward distance, and a width that matches
// the camera's own tangent within a tenth (the device's matrix is the last
// one set, which after the world may belong to something else entirely).
inline bool PerspectiveMatchesCamera(const float (&p)[4][4], float tanHalfWidth) {
	const float wFromZ = p[2][3];
	const float wConstant = p[3][3];
	if (!(wFromZ > 0.99f && wFromZ < 1.01f) || !(wConstant > -0.01f && wConstant < 0.01f) ||
	    !(p[0][0] > 0.0f) || !(p[1][1] > 0.0f) || !(p[2][2] > 0.0f)) {
		return false;
	}
	if (!(tanHalfWidth > 0.0f)) {
		return false;
	}
	const float tangent = 1.0f / p[0][0];
	const float difference = tangent - tanHalfWidth;
	return difference < tanHalfWidth * 0.1f && -difference < tanHalfWidth * 0.1f;
}

// A render model's vertex (OpenVR controller frame: x right, y up, z back,
// metres) in the game's controller axes (x right, y forward, z up) - the
// axes HandMode's relative rotations are written in.
inline NiPoint3 ControllerVertexToGame(const vr::openvr::HmdVector3& v) {
	return NiPoint3{v.v[0], -v.v[2], v.v[1]};
}

// A model drawn in one colour, shaded by a light fixed to the model: lit
// from above and in front of the controller, never darker than a third, so
// every face reads.
inline UInt32 ShadedModelColour(const NiPoint3& normal) {
	// Up (+y) and towards the user (+z, the controller's back) in the
	// render model's frame, normalised: (0, 0.8, 0.6).
	float lit = normal.y * 0.8f + normal.z * 0.6f;
	if (!(lit > 0.0f)) {
		lit = 0.0f;
	}
	if (lit > 1.0f) {
		lit = 1.0f;
	}
	const float level = 0.35f + 0.65f * lit;
	const UInt32 red = static_cast<UInt32>(level * 200.0f);
	const UInt32 green = static_cast<UInt32>(level * 215.0f);
	const UInt32 blue = static_cast<UInt32>(level * 235.0f);
	return 0xFF000000u | (red << 16) | (green << 8) | blue;
}

}  // namespace obvr::render
