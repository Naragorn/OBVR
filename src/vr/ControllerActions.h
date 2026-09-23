#pragma once

#include <cmath>
#include <cstdio>

#include "vr/HandInput.h"

namespace obvr::vr {

namespace input {

// These are the natural Windows layouts from openvr_capi.h's IVRInput_011
// interface. The action API is loaded dynamically because OBVR must still
// start when SteamVR is absent.
struct DigitalData {
	bool active;
	UInt64 origin;
	bool state;
	bool changed;
	float time;
};

struct AnalogData {
	bool active;
	UInt64 origin;
	float x;
	float y;
	float z;
	float dx;
	float dy;
	float dz;
	float time;
};

static_assert(sizeof(DigitalData) == 24, "OpenVR digital action ABI");
static_assert(sizeof(AnalogData) == 48, "OpenVR analog action ABI");

// Skeletal hand tracking structures from IVRInput_011 (openvr_capi.h).
struct InputSkeletalActionData {
	bool active;
	UInt64 origin;
};

static_assert(sizeof(InputSkeletalActionData) == 16, "OpenVR skeletal action data ABI");

// A bone's transform in its parent's space. Position is x,y,z,w (w unused),
// orientation is a quaternion w,x,y,z.
struct VRBoneTransform {
	float position[4];
	float qw;
	float qx;
	float qy;
	float qz;
};

static_assert(sizeof(VRBoneTransform) == 32, "OpenVR bone transform ABI");

enum SkeletalTransformSpace : int {
	SkeletalModel = 0,    // relative to the skeleton root (controller grip)
	SkeletalParent = 1    // relative to each bone's parent in the chain
};

enum SkeletalMotionRange : int {
	SkeletalWithController = 0,      // hand pose includes controller offset
	SkeletalWithoutController = 1    // pure hand pose, no controller offset
};

// Standard SteamVR hand skeleton bone indices (25 bones per hand).
enum HandSkeletonBone : unsigned {
	HandRoot = 0,
	HandWrist,
	HandThumbMetacarpal,
	HandThumbProximal,
	HandThumbIntermediate,
	HandThumbDistal,
	HandIndexFingerMetacarpal,
	HandIndexFingerProximal,
	HandIndexFingerIntermediate,
	HandIndexFingerDistal,
	HandMiddleFingerMetacarpal,
	HandMiddleFingerProximal,
	HandMiddleFingerIntermediate,
	HandMiddleFingerDistal,
	HandRingFingerMetacarpal,
	HandRingFingerProximal,
	HandRingFingerIntermediate,
	HandRingFingerDistal,
	HandPinkyFingerMetacarpal,
	HandPinkyFingerProximal,
	HandPinkyFingerIntermediate,
	HandPinkyFingerDistal,
	HandBoneCount
};

static_assert(HandBoneCount == 22, "SteamVR hand skeleton has 22 bones (root+wrist+4*5 fingers)");

struct ActionSet {
	UInt64 set;
	UInt64 device;
	UInt64 secondary;
	UInt32 padding;
	SInt32 priority;
};

static_assert(sizeof(ActionSet) == 32, "OpenVR active action set ABI");

struct Table {
	int(__stdcall* SetManifest)(const char* path);
	int(__stdcall* GetSet)(const char* path, UInt64* handle);
	int(__stdcall* GetAction)(const char* path, UInt64* handle);
	int(__stdcall* GetSource)(const char* path, UInt64* handle);
	int(__stdcall* Update)(ActionSet* sets, UInt32 size, UInt32 count);
	int(__stdcall* Digital)(UInt64 action, DigitalData* data, UInt32 size, UInt64 origin);
	int(__stdcall* Analog)(UInt64 action, AnalogData* data, UInt32 size, UInt64 origin);
	void* pose;
	void* nextFramePose;
	// GetSkeletalActionData(action, &data, sizeof(data)) - check if skeletal action is active.
	int(__stdcall* GetSkeletalActionData)(UInt64 action, InputSkeletalActionData* data, UInt32 size);
	void* unused[9];
	// GetSkeletalBoneData(action, space, range, transforms[], count) - read bone poses.
	int(__stdcall* GetSkeletalBoneData)(UInt64 action, int space, int range, VRBoneTransform* out,
	                                    UInt32 count);
};

enum Action : unsigned {
	StickClick,
	A,
	B,
	Grip,
	Trackpad,
	Trigger,
	Stick,
	Count
};

inline const char* ActionName(unsigned action) {
	static const char* names[] = {
		"stick_click", "a", "b", "grip", "trackpad", "trigger", "stick"};
	return action < Count ? names[action] : "";
}

// Skeletal hand tracking handles (one per hand).
struct SkeletonHandles {
	UInt64 left = 0;   // /actions/obvr/in/left_hand_skeleton
	UInt64 right = 0;  // /actions/obvr/in/right_hand_skeleton
};

inline void ClearSkeletonSetup(SkeletonHandles& handles) {
	handles.left = 0;
	handles.right = 0;
}

inline void ClearActionSetup(UInt64& actionSet, UInt64 handles[2][Count]) {
	actionSet = 0;
	for (unsigned hand = 0; hand < 2; ++hand) {
		for (unsigned action = 0; action < Count; ++action) {
			handles[hand][action] = 0;
		}
	}
}

inline bool HasSetupMethods(const Table* table) {
	return table != nullptr && table->SetManifest != nullptr && table->GetSet != nullptr &&
	       table->GetAction != nullptr && table->Update != nullptr && table->Digital != nullptr &&
	       table->Analog != nullptr;
}

// Check whether the skeletal API functions are present in this IVRInput version.
inline bool HasSkeletalMethods(const Table* table) {
	return table != nullptr && table->GetSkeletalActionData != nullptr &&
	       table->GetSkeletalBoneData != nullptr;
}

// Register one complete manifest and resolve every action handle. A partial
// setup is unusable: clear all outputs before returning so a failed reopen
// cannot retain handles from an earlier runtime.
inline int ConfigureActions(Table* table, const char* manifest, UInt64& actionSet,
                            UInt64 handles[2][Count]) {
	ClearActionSetup(actionSet, handles);
	if (!HasSetupMethods(table) || manifest == nullptr || manifest[0] == '\0') {
		return -1;
	}

	int error = table->SetManifest(manifest);
	if (error == 0) {
		error = table->GetSet("/actions/obvr", &actionSet);
		if (error == 0 && actionSet == 0) {
			error = -1;
		}
	}
	for (unsigned hand = 0; hand < 2 && error == 0; ++hand) {
		for (unsigned action = 0; action < Count && error == 0; ++action) {
			char path[128]{};
			const int written = std::snprintf(path, sizeof(path), "/actions/obvr/in/%s_%s",
			                                  hand == 0 ? "right" : "left",
			                                  ActionName(action));
			if (written < 0 || static_cast<size_t>(written) >= sizeof(path)) {
				error = -1;
				break;
			}
			error = table->GetAction(path, &handles[hand][action]);
			if (error == 0 && handles[hand][action] == 0) {
				error = -1;
			}
		}
	}
	if (error != 0) {
		ClearActionSetup(actionSet, handles);
	}
	return error;
}

// Resolve skeletal action handles from the manifest. These are optional: if
// they fail to resolve, finger tracking is unavailable but controller actions
// still work. Returns 0 on success or an OpenVR error code.
inline int ConfigureSkeletonActions(Table* table, SkeletonHandles& out) {
	ClearSkeletonSetup(out);
	if (!HasSkeletalMethods(table)) {
		return -1;
	}

	int error = table->GetAction("/actions/obvr/in/left_hand_skeleton", &out.left);
	if (error == 0 && out.left == 0) {
		error = -2;
	}
	if (error == 0) {
		error = table->GetAction("/actions/obvr/in/right_hand_skeleton", &out.right);
		if (error == 0 && out.right == 0) {
			error = -3;
		}
	}
	if (error != 0) {
		ClearSkeletonSetup(out);
	}
	return error;
}

// UpdateActionState's third argument is the number of entries; the second is
// the byte size of each VRActiveActionSet_t entry.
inline int UpdateActionState(Table* table, UInt64 actionSet, ActionSet& active) {
	active = ActionSet{};
	active.set = actionSet;
	if (table == nullptr || table->Update == nullptr || actionSet == 0) {
		return -1;
	}
	return table->Update(&active, sizeof(active), 1);
}

// Reads the controller actions into the existing HandPose value. Each action
// is independently fail-closed: an inactive or failed action contributes its
// neutral value, while the pose is read by OpenVRBackend through the system
// interface and remains usable.
inline int ReadControls(Table* table, const UInt64* handles, bool updated, HandPose& out,
                       UInt32* activeMask = nullptr) {
	if (activeMask != nullptr) {
		*activeMask = 0;
	}
	out.buttonsPressed = 0;
	out.trigger = 0.0f;
	out.thumbX = 0.0f;
	out.thumbY = 0.0f;
	out.thumbFromJoystickAxis = false;
	out.gripForce = 0.0f;
	if (table == nullptr || handles == nullptr || !updated || table->Digital == nullptr ||
	    table->Analog == nullptr) {
		return -1;
	}

	int firstError = 0;
	const unsigned buttonBits[] = {
		openvr::kButtonIndexJoystick,
		openvr::kButtonA,
		openvr::kButtonIndexB,
		openvr::kButtonIndexGrip,
		openvr::kButtonIndexTrackpad,
	};
	for (unsigned action = StickClick; action < Trigger; ++action) {
		DigitalData data{};
		const int error = table->Digital(handles[action], &data, sizeof(data), 0);
		if (error != 0 && firstError == 0) {
			firstError = error;
		}
		if (error == 0 && data.active && activeMask != nullptr) {
			*activeMask |= 1u << action;
		}
		if (error == 0 && data.active && data.state) {
			out.buttonsPressed |= 1ull << buttonBits[action];
		}
	}

	for (unsigned action = Trigger; action < Count; ++action) {
		AnalogData data{};
		const int error = table->Analog(handles[action], &data, sizeof(data), 0);
		if (error != 0 && firstError == 0) {
			firstError = error;
		}
		if (error != 0 || !data.active) {
			continue;
		}
		if (action == Trigger) {
			if (!std::isfinite(data.x) || data.x < 0.0f || data.x > 1.0f) {
				if (firstError == 0) {
					firstError = -2;
				}
				continue;
			}
			out.trigger = data.x;
		} else {
			if (!std::isfinite(data.x) || !std::isfinite(data.y) || data.x < -1.0f ||
			    data.x > 1.0f || data.y < -1.0f || data.y > 1.0f) {
				if (firstError == 0) {
					firstError = -2;
				}
				continue;
			}
			out.thumbX = data.x;
			out.thumbY = data.y;
			out.thumbFromJoystickAxis = true;
		}
		if (activeMask != nullptr) {
			*activeMask |= 1u << action;
		}
	}
	return firstError;
}

// Copy only normalized controls and their provenance. The tracked pose and
// validity stay untouched so action failures cannot erase a usable hand.
inline void ApplyActionControls(HandPose& pose, const HandPose& controls, UInt32 activeMask,
                                SInt32 error) {
	pose.buttonsPressed = controls.buttonsPressed;
	pose.trigger = controls.trigger;
	pose.thumbX = controls.thumbX;
	pose.thumbY = controls.thumbY;
	pose.thumbFromJoystickAxis = controls.thumbFromJoystickAxis;
	pose.actionInput = true;
	pose.actionActiveMask = activeMask;
	pose.actionError = error;
}

// Read skeletal bone data for one hand. Returns the number of bones read (up
// to HandBoneCount), or 0 if the action is inactive, unbound, or unavailable.
// Transforms are in parent space by default so each bone's pose is relative to
// its predecessor in the chain; use SkeletalModel to get poses relative to the
// skeleton root instead. The motion range controls whether the controller-to-
// hand offset is included (WithController) or excluded (WithoutController).
inline unsigned ReadSkeleton(Table* table, UInt64 actionHandle, VRBoneTransform* out,
                             unsigned maxBones, int space = SkeletalParent,
                             int range = SkeletalWithoutController) {
	if (!HasSkeletalMethods(table) || actionHandle == 0 || out == nullptr || maxBones == 0) {
		return 0;
	}

	InputSkeletalActionData data{};
	const int checkError = table->GetSkeletalActionData(actionHandle, &data, sizeof(data));
	if (checkError != 0 || !data.active) {
		return 0;
	}

	const unsigned count = maxBones < HandBoneCount ? maxBones : HandBoneCount;
	const int readError = table->GetSkeletalBoneData(actionHandle, space, range, out, count);
	if (readError != 0) {
		return 0;
	}
	return count;
}

}  // namespace input
}  // namespace obvr::vr
