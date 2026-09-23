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
	void* skeleton;
	void* unused[9];
	void* bones;
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

}  // namespace input
}  // namespace obvr::vr
