#include "game/HandControls.h"

#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// Which keys OBVR itself put down, by virtual-key code. Bounded: the map has
// fifteen entries and nothing else is ever pressed from here.
constexpr UInt32 kHeldSlots = 16;
UInt32 g_heldKeys[kHeldSlots] = {};
UInt32 g_heldCount = 0;

bool IsHeld(UInt32 key) {
	for (UInt32 i = 0; i < g_heldCount; ++i) {
		if (g_heldKeys[i] == key) {
			return true;
		}
	}
	return false;
}

void RememberHeld(UInt32 key) {
	if (g_heldCount < kHeldSlots && !IsHeld(key)) {
		g_heldKeys[g_heldCount++] = key;
	}
}

void ForgetHeld(UInt32 key) {
	for (UInt32 i = 0; i < g_heldCount; ++i) {
		if (g_heldKeys[i] == key) {
			g_heldKeys[i] = g_heldKeys[g_heldCount - 1];
			--g_heldCount;
			return;
		}
	}
}

void SendKey(UInt32 key, bool down) {
	if (key == 0x01) {
		mouse_event(down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
		return;
	}
	if (key == 0x02) {
		mouse_event(down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
		return;
	}
	const UInt32 scan = MapVirtualKeyA(key, MAPVK_VK_TO_VSC);
	keybd_event(static_cast<UInt8>(key), static_cast<UInt8>(scan),
	            KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP), 0);
}

// Brings one key to the wanted state, sending nothing when it is there.
void SetKey(UInt32 key, bool wanted) {
	if (key == 0) {
		return;
	}
	const bool held = IsHeld(key);
	if (wanted && !held) {
		SendKey(key, true);
		RememberHeld(key);
	} else if (!wanted && held) {
		SendKey(key, false);
		ForgetHeld(key);
	}
}

}  // namespace

void ApplyHandControls(const vr::HandControlsWanted& wanted, const HandKeyMap& keys,
                       float turnSpeed) {
	SetKey(keys.attack, wanted.attack || wanted.menuClick);
	SetKey(keys.block, wanted.block);
	SetKey(keys.cast, wanted.cast);
	SetKey(keys.activate, wanted.activate);
	SetKey(keys.grab, wanted.grab);
	SetKey(keys.jump, wanted.jump);
	SetKey(keys.sneak, wanted.sneak);
	SetKey(keys.readyWeapon, wanted.readyWeapon);
	SetKey(keys.menu, wanted.menu);
	SetKey(keys.escape, wanted.escape);
	SetKey(keys.quickMenu, wanted.quickMenu);
	SetKey(keys.forward, wanted.move.forward);
	SetKey(keys.back, wanted.move.back);
	SetKey(keys.left, wanted.move.left);
	SetKey(keys.right, wanted.move.right);

	if (wanted.turn > 0.1f || wanted.turn < -0.1f) {
		const int dx = static_cast<int>(wanted.turn * turnSpeed);
		if (dx != 0) {
			MoveMouseBy(dx, 0);
		}
	}
}

void ReleaseHandControls(const HandKeyMap&) {
	while (g_heldCount > 0) {
		const UInt32 key = g_heldKeys[g_heldCount - 1];
		SendKey(key, false);
		--g_heldCount;
	}
}

void MoveMouseBy(int dx, int dy) {
	mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(dx), static_cast<DWORD>(dy), 0, 0);
}

void ScrollMouseWheel(int notches) {
	if (notches == 0) {
		return;
	}
	mouse_event(MOUSEEVENTF_WHEEL, 0, 0, static_cast<DWORD>(notches * WHEEL_DELTA), 0);
}

}  // namespace obvr::game
