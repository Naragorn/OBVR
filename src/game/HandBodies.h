#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "game/HandBodyLogic.h"

namespace obvr::game {

// The hands and the drawn melee weapon as keyframed Havok bodies
// (docs/hand-weapon-collision-spec.md; the tester, 2026-09-28: "setze die mal
// um"). Each is a capsule in the player's Havok world, driven every frame to
// where the controller puts the hand or the blade, and Havok's solver pushes
// what they meet: a cup tips, a plate slides, a hand held still keeps
// pressing. Like HIGGS in Skyrim VR, a keyframed body does not stop at walls.
//
// Made the way the engine makes one from a NIF (Oblivion.exe 1.2.0.416,
// read 2026-09-28):
// - the shape: bhkCapsuleShape, 0x14 bytes from 0x00401F00, its data
//   (0x30 bytes, default 0x00564030: +0x04 radius, +0x10 and +0x20 the two
//   ends, Havok units) handed to its constructor 0x00563BB0, which makes the
//   hkCapsuleShape at once (0x008B6B90); the hkShape is at shape+8;
// - the body: the bhkRigidBody factory 0x008A41F0 (the NIF loader's), its
//   creation block from vtable +0x74 (0x008A5980: 0xF0 bytes, 16-aligned;
//   the hkRigidBodyCinfo embedded at +0x20), filled - filter, shape, pose,
//   mass 1, motion type 6 (keyframed) - and handed to vtable +0x70
//   (0x008A4260), which builds the hkRigidBody (0x008A9F50) and keeps it at
//   wrapper+8;
// - into the world, and between worlds: 0x0089F470 thiscall(wrapper,
//   bhkWorld*) - out of the old, bhkWorld::AddObject into the new, nothing
//   when it is already there; out: vtable +0x60 (0x008B0020);
// - both wrappers get a reference of OBVR's (NiRefObject, +4) so nothing
//   frees them under it. A weapon body replaced for another weapon's blade
//   is taken out of the world and kept, not freed: the release path of a
//   bhkRigidBody has not been read.
//
// Driven with the body's motion's setLinearVelocity / setAngularVelocity
// (vtable +0x54 / +0x58) and activated (0x008A6410) each frame; placed
// through the wrapper's SetTranslationAndRotation (vtable +0xA0) when too
// far. All inside the Havok critical section (0x00BA7B00).

enum class HandBodySlot : UInt8 { RightHand, LeftHand, Weapon, Count };

struct HandBodyFrame {
	// [Hands] BodyCollision, and the moment allows it: Full VR in the world,
	// first person, no menu (a load is a menu: the bodies are out of the
	// world before it is torn down).
	bool enabled = false;
	bool valid[static_cast<int>(HandBodySlot::Count)] = {};
	NiMatrix33 rot[static_cast<int>(HandBodySlot::Count)];  // world, the hand's
	NiPoint3 pos[static_cast<int>(HandBodySlot::Count)];    // the grip, game units
	float bladeUnits = 0.0f;   // the weapon's length, for its capsule
	// Whether each body pushes people (HandBodyPushesActors): not in combat,
	// not a hand made a fist.
	bool pushesActors[static_cast<int>(HandBodySlot::Count)] = {true, true, true};
	// Each body's span from the drawn hand or blade, in the body's own frame
	// (HandBodyLogic.h); invalid, the constants are used.
	BodySpan span[static_cast<int>(HandBodySlot::Count)];
	float physicsRate = 0.0f;  // [Hands] PhysicsRate, 0 the game's own
	float dtSeconds = 0.0f;
};

// Which bodies stood in the world and were driven this frame: what
// game::WorldPush leaves alone.
struct HandBodyReport {
	bool live[static_cast<int>(HandBodySlot::Count)] = {};
};

// Once at start-up: the first bytes of every function used, and the bhk
// vtable's slots, against what was read. False (and every body refused)
// when one differs.
bool VerifyHandBodyAddresses();

HandBodyReport StepHandBodies(const HandBodyFrame& frame);

// For a hand script's marks: where each body stands (Havok's own pose, in
// game units), one line.
void LogHandBodies();

// How many physics steps this frame runs (the planner, 0x00BA7914).
UInt32 HandBodyPhysicsSteps();

// The axis-aligned box an object's Havok shape covers in the world, game
// units, the convex radius included: from the node's collision object. False
// when it has none, or it is no rigid body of the hands' own class.
bool HavokWorldBoxOf(UInt32 node, NiPoint3& low, NiPoint3& high);

}  // namespace obvr::game
