#include "game/HandBodies.h"

#include <intrin.h>

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GrabPhysics.h"
#include "game/HandBodyLogic.h"
#include "game/PlayerTeleport.h"

namespace obvr::game {
namespace {

// The functions and tables used (HandBodies.h, the spec's tables).
constexpr UInt32 kGameAlloc = 0x00401F00;            // cdecl(size)
constexpr UInt32 kCapsuleDataCtor = 0x00564030;      // thiscall(data)
constexpr UInt32 kCapsuleShapeCtor = 0x00563BB0;     // thiscall(this, data*), ret 4
constexpr UInt32 kRigidBodyFactory = 0x008A41F0;     // cdecl(), the NIF loader's
constexpr UInt32 kMoveToWorld = 0x0089F470;          // thiscall(wrapper, bhkWorld*), ret 4
constexpr UInt32 kRigidBodyVtable = 0x00A5605C;
constexpr UInt32 kVtblRemoveFromWorld = 0x60;
constexpr UInt32 kVtblCreateHavok = 0x70;
constexpr UInt32 kVtblCreateHavokData = 0x74;
constexpr UInt32 kKeyframedMotionVtable = 0x00A9AE10;
constexpr UInt32 kPlannerStepSeconds = 0x00BA790C;   // the step length this frame's steps use
constexpr UInt32 kPlannerStepCount = 0x00BA7914;     // and how many there are
constexpr UInt32 kPhysicsStepRuntime = 0x00B2E2E8;   // fMaxTime:HAVOK's runtime copy
constexpr UInt32 kShapeSize = 0x14;
constexpr UInt32 kWorldBhkWorldOffset = 0x2B0;       // hkWorld -> its bhkWorld
constexpr UInt32 kBhkWorldHkWorldOffset = 0x08;      // bhkWorld -> its hkWorld
constexpr UInt32 kWrapperObjOffset = 0x08;           // bhk wrapper -> hk object
constexpr UInt32 kRefCountOffset = 0x04;             // NiRefObject
constexpr UInt32 kBodyWorldOffset = 0x08;            // hkEntity -> hkWorld
constexpr UInt32 kVtblRefreshFilter = 0x80;         // thiscall(wrapper): the world takes the new filter
constexpr UInt32 kLayerMatrix = 0x00BA7DB0;          // 32 rows of 32 bits, one per layer
constexpr UInt32 kLayerMatrixSetter = 0x008A7F20;    // cdecl(layerA, layerB, bool), both ways
// For measuring what an object's Havok shape covers (2026-09-29): the node's
// bhkCollisionObject (0x0047FAC0 cdecl(node), NiAVObject+0xA8 behind an RTTI
// check), its body wrapper at +0x10; in the hkRigidBody the shape at +0x14
// and the transform pointer at +0x1C (the collidable's); hkShape's getAabb
// at vtable +0x0C, thiscall(shape, const hkTransform*, float tolerance,
// hkAabb* out), ret 0Ch, min at +0, max at +0x10 (read on the box and the
// sphere shape, 0x008CE060, 0x008ED4A0; the convex radius included).
constexpr UInt32 kCollisionObjectOf = 0x0047FAC0;
constexpr UInt32 kCollisionObjectBodyOffset = 0x10;
constexpr UInt32 kBodyShapeOffset = 0x14;
constexpr UInt32 kBodyTransformOffset = 0x1C;
constexpr UInt32 kShapeGetAabbSlot = 0x0C;

// The creation block (0x008A5790's layout): the filter and the shape, and
// the hkRigidBodyCinfo at +0x20 (the spec's table, offsets from the block).
constexpr UInt32 kBlockFilter = 0x00;
constexpr UInt32 kBlockShape = 0x04;
constexpr UInt32 kBlockCinfo = 0x20;
constexpr UInt32 kCinfoPosition = 0x10;
constexpr UInt32 kCinfoRotation = 0x20;
constexpr UInt32 kCinfoMass = 0x90;
constexpr UInt32 kCinfoMotionType = 0xB0;

struct Expected {
	UInt32 address;
	UInt8 bytes[8];
	UInt32 size;
	const char* name;
};

const Expected kExpected[] = {
	{kGameAlloc, {0x8B, 0x44, 0x24, 0x04, 0x6A, 0x01, 0x50}, 7, "the game's allocator"},
	{kCapsuleDataCtor, {0x8B, 0xC1, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, "the capsule data"},
	{kCapsuleShapeCtor, {0x6A, 0xFF, 0x68, 0x58, 0xD4, 0x9B, 0x00}, 7, "bhkCapsuleShape"},
	{kRigidBodyFactory, {0x6A, 0xFF, 0x68, 0xDB, 0x74, 0x9C, 0x00}, 7, "the bhkRigidBody factory"},
	{0x008A5980, {0x6A, 0xFF, 0x68, 0x9B, 0x70, 0x9D, 0x00}, 7, "CreateHavokData"},
	{0x008A4260, {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x6A, 0xFF}, 8, "CreateHavok"},
	{kMoveToWorld, {0x53, 0x55, 0x56, 0x8B, 0xF1, 0x85, 0xF6}, 7, "the move between worlds"},
	{0x008B0020, {0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x8B, 0x50}, 8, "RemoveFromWorld"},
	{kActivateBody, {0x8B, 0x41, 0x54, 0x85, 0xC0}, 5, "activate"},
	{0x008A2FB0, {0x56, 0x8B, 0xF1, 0x85, 0xF6}, 5, "SetTranslationAndRotation"},
	{kHavokLockEnter, {0x56, 0x8B, 0xF1, 0x56}, 4, "the Havok lock"},
	{kHavokLockLeave, {0x83, 0x41, 0x7C, 0xFF}, 4, "the Havok unlock"},
	{0x0089DB90, {0x8B, 0x44, 0x24, 0x04, 0x0F, 0x28, 0x00}, 7, "setLinearVelocity"},
	{0x0089DBB0, {0x8B, 0x44, 0x24, 0x04, 0x0F, 0x28, 0x00}, 7, "setAngularVelocity"},
	{kCollisionObjectOf, {0x8B, 0x44, 0x24, 0x04, 0x56, 0x8B, 0xB0, 0xA8}, 8, "the collision object getter"},
	{kLayerMatrixSetter, {0x8B, 0x54, 0x24, 0x08, 0x8B, 0x44, 0x24, 0x04}, 8, "the layer matrix setter"},
};

struct Slot {
	UInt32 offset;
	UInt32 value;
};
// bhkRigidBody's vtable: SetObj, GetWorld, AddToWorld, RemoveFromWorld,
// CreateHavok, CreateHavokData, the filter's refresh (0x008B0060:
// hkWorld::updateCollisionFilterOnEntity, 0x0089B630), SetTranslationAndRotation.
const Slot kVtableSlots[] = {{0x4C, 0x0089D730}, {0x58, 0x0089D940}, {0x5C, 0x008A48C0},
                             {0x60, 0x008B0020}, {0x70, 0x008A4260}, {0x74, 0x008A5980},
                             {0x80, 0x008B0060}, {0xA0, 0x008A2FB0}};
// The keyframed motion's velocity setters.
const Slot kMotionSlots[] = {{kMotionSetLinearVelocitySlot, 0x0089DB90},
                             {kMotionSetAngularVelocitySlot, 0x0089DBB0}};

bool g_verified = false;

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

const char* SlotName(int slot) {
	switch (static_cast<HandBodySlot>(slot)) {
	case HandBodySlot::RightHand:
		return "the right hand";
	case HandBodySlot::LeftHand:
		return "the left hand";
	default:
		return "the weapon";
	}
}

struct Body {
	UInt32 wrapper = 0;  // bhkRigidBody
	UInt32 body = 0;     // hkRigidBody
	float bladeUnits = 0.0f;
	// The span it was built from (invalid: the constants), and the frames the
	// drawn span has stood away from it.
	BodySpan span;
	UInt32 spanFrames = 0;
	bool reportedDrive = false;
	// The world it was last sent into, and the frames since: an add the
	// world defers (while it steps) is not asked for twice.
	UInt32 enteredBhk = 0;
	UInt32 enterWait = 0;
};
constexpr UInt32 kEnterWaitFrames = 5;
Body g_bodies[static_cast<int>(HandBodySlot::Count)];
UInt32 g_failLinesLeft = 6;
UInt32 g_worldLinesLeft = 12;
UInt32 g_placeLinesLeft = 6;
float g_originalStep = 0.0f;
bool g_stepChanged = false;

struct Lock {
	Lock() {
		using LockFn = void(__thiscall*)(void* lock);
		reinterpret_cast<LockFn>(kHavokLockEnter)(reinterpret_cast<void*>(kHavokLock));
	}
	~Lock() {
		using LockFn = void(__thiscall*)(void* lock);
		reinterpret_cast<LockFn>(kHavokLockLeave)(reinterpret_cast<void*>(kHavokLock));
	}
};

void TakeReference(UInt32 object) {
	_InterlockedIncrement(reinterpret_cast<volatile long*>(object + kRefCountOffset));
}

UInt32 VtableSlot(UInt32 object, UInt32 offset) { return Read(Read(object) + offset); }

// The body in the world it stands in, and whether that world is still one:
// its bhkWorld points back at it.
UInt32 BodyWorld(const Body& b) {
	return LooksLikeObject(b.body) ? Read(b.body + kBodyWorldOffset) : 0;
}

bool WorldSound(UInt32 hkWorld) {
	if (!LooksLikeObject(hkWorld)) {
		return false;
	}
	const UInt32 bhk = Read(hkWorld + kWorldBhkWorldOffset);
	return LooksLikeObject(bhk) && Read(bhk + kBhkWorldHkWorldOffset) == hkWorld;
}

void RotationQuaternion(const NiMatrix33& rot, float out[4]) { QuaternionFromRotation(rot, out); }

// A capsule body at the pose, keyframed, in the player's group; not yet in
// a world. False, and nothing kept, when a step fails.
bool Create(Body& b, const CapsuleSpec& capsule, const NiMatrix33& rot, const NiPoint3& pos, int slot,
            bool pushesActors) {
	using AllocFn = void*(__cdecl*)(UInt32 size);
	using DataCtorFn = void(__thiscall*)(void* data);
	using ShapeCtorFn = void*(__thiscall*)(void* self, void* data);
	using FactoryFn = void*(__cdecl*)();
	using BlockFn = UInt8*(__thiscall*)(void* self, bool* created);
	using CreateFn = void(__thiscall*)(void* self, void* block);

	void* const shapeMemory = reinterpret_cast<AllocFn>(kGameAlloc)(kShapeSize);
	if (shapeMemory == nullptr) {
		return false;
	}
	alignas(16) float data[12];
	reinterpret_cast<DataCtorFn>(kCapsuleDataCtor)(data);
	data[1] = capsule.radius;
	data[4] = capsule.a.x;
	data[5] = capsule.a.y;
	data[6] = capsule.a.z;
	data[7] = 0.0f;
	data[8] = capsule.b.x;
	data[9] = capsule.b.y;
	data[10] = capsule.b.z;
	data[11] = 0.0f;
	const UInt32 shape =
		reinterpret_cast<UInt32>(reinterpret_cast<ShapeCtorFn>(kCapsuleShapeCtor)(shapeMemory, data));
	const UInt32 hkShape = LooksLikeObject(shape) ? Read(shape + kWrapperObjOffset) : 0;
	if (!LooksLikeObject(hkShape)) {
		if (g_failLinesLeft > 0) {
			--g_failLinesLeft;
			OBVR_LOG("Hands: %s's capsule could not be made (shape %08X, hkShape %08X)", SlotName(slot),
			         shape, hkShape);
		}
		return false;
	}
	TakeReference(shape);

	const UInt32 wrapper = reinterpret_cast<UInt32>(reinterpret_cast<FactoryFn>(kRigidBodyFactory)());
	if (!LooksLikeObject(wrapper) || Read(wrapper) != kRigidBodyVtable) {
		if (g_failLinesLeft > 0) {
			--g_failLinesLeft;
			OBVR_LOG("Hands: %s's bhkRigidBody could not be made (%08X)", SlotName(slot), wrapper);
		}
		return false;
	}
	TakeReference(wrapper);
	bool created = false;
	UInt8* const block =
		reinterpret_cast<BlockFn>(VtableSlot(wrapper, kVtblCreateHavokData))(reinterpret_cast<void*>(wrapper), &created);
	if (!LooksLikeObject(reinterpret_cast<UInt32>(block))) {
		if (g_failLinesLeft > 0) {
			--g_failLinesLeft;
			OBVR_LOG("Hands: %s's creation block could not be had", SlotName(slot));
		}
		return false;
	}
	const UInt32 filter = HandBodyFilter(PlayerCollisionGroup(), true, pushesActors);
	*reinterpret_cast<UInt32*>(block + kBlockFilter) = filter;
	*reinterpret_cast<UInt32*>(block + kBlockShape) = hkShape;
	UInt8* const cinfo = block + kBlockCinfo;
	*reinterpret_cast<UInt32*>(cinfo + 0x00) = filter;
	*reinterpret_cast<UInt32*>(cinfo + 0x04) = hkShape;
	float* const position = reinterpret_cast<float*>(cinfo + kCinfoPosition);
	position[0] = pos.x * kHavokPerUnit;
	position[1] = pos.y * kHavokPerUnit;
	position[2] = pos.z * kHavokPerUnit;
	position[3] = 0.0f;
	RotationQuaternion(rot, reinterpret_cast<float*>(cinfo + kCinfoRotation));
	*reinterpret_cast<float*>(cinfo + kCinfoMass) = 1.0f;
	*(cinfo + kCinfoMotionType) = static_cast<UInt8>(kMotionTypeKeyframed);
	reinterpret_cast<CreateFn>(VtableSlot(wrapper, kVtblCreateHavok))(reinterpret_cast<void*>(wrapper), block);

	const UInt32 body = Read(wrapper + kWrapperObjOffset);
	const UInt32 motion = LooksLikeObject(body) ? Read(body + kBodyMotionOffset) : 0;
	const UInt32 motionVtable = LooksLikeObject(motion) ? Read(motion) : 0;
	OBVR_LOG("Hands: %s's body made - bhkRigidBody %08X, hkRigidBody %08X, motion vtable %08X (%s), "
	         "filter %08X, capsule %.1f units round, %.1f long, from %.1f %.1f %.1f to %.1f %.1f %.1f (the grip's frame)",
	         SlotName(slot), wrapper, body, motionVtable,
	         motionVtable == kKeyframedMotionVtable ? "keyframed" : "NOT keyframed", filter,
	         static_cast<double>(capsule.radius / kHavokPerUnit),
	         static_cast<double>(math::Sqrt((capsule.b - capsule.a).LengthSquared()) / kHavokPerUnit),
	         static_cast<double>(capsule.a.x / kHavokPerUnit), static_cast<double>(capsule.a.y / kHavokPerUnit),
	         static_cast<double>(capsule.a.z / kHavokPerUnit), static_cast<double>(capsule.b.x / kHavokPerUnit),
	         static_cast<double>(capsule.b.y / kHavokPerUnit), static_cast<double>(capsule.b.z / kHavokPerUnit));
	if (!LooksLikeObject(body) || motionVtable != kKeyframedMotionVtable) {
		return false;
	}
	b.wrapper = wrapper;
	b.body = body;
	return true;
}

void Enter(Body& b, UInt32 bhkWorld, int slot) {
	if (b.enteredBhk == bhkWorld && b.enterWait < kEnterWaitFrames) {
		++b.enterWait;
		return;
	}
	b.enteredBhk = bhkWorld;
	b.enterWait = 0;
	using MoveFn = bool(__thiscall*)(void* self, void* world);
	reinterpret_cast<MoveFn>(kMoveToWorld)(reinterpret_cast<void*>(b.wrapper), reinterpret_cast<void*>(bhkWorld));
	if (g_worldLinesLeft > 0) {
		--g_worldLinesLeft;
		OBVR_LOG("Hands: %s's body into the player's world (bhkWorld %08X) - its hkWorld now %08X",
		         SlotName(slot), bhkWorld, BodyWorld(b));
	}
}

void Leave(Body& b, int slot) {
	b.enteredBhk = 0;
	using RemoveFn = bool(__thiscall*)(void* self);
	reinterpret_cast<RemoveFn>(VtableSlot(b.wrapper, kVtblRemoveFromWorld))(reinterpret_cast<void*>(b.wrapper));
	if (g_worldLinesLeft > 0) {
		--g_worldLinesLeft;
		OBVR_LOG("Hands: %s's body out of the world (its hkWorld now %08X)", SlotName(slot), BodyWorld(b));
	}
}

// Velocities to the tracked pose, or placed there when too far.
void Drive(Body& b, const NiMatrix33& rot, const NiPoint3& pos, float stepSeconds, int slot) {
	const UInt32 motion = Read(b.body + kBodyMotionOffset);
	if (!LooksLikeObject(motion) || Read(motion) != kKeyframedMotionVtable) {
		return;
	}
	const float* const columns = reinterpret_cast<const float*>(motion + kMotionRotationOffset);
	const float* const at = reinterpret_cast<const float*>(motion + kMotionTranslationOffset);
	const NiPoint3 current{at[0], at[1], at[2]};
	const NiPoint3 target{pos.x * kHavokPerUnit, pos.y * kHavokPerUnit, pos.z * kHavokPerUnit};
	const float gap = math::Sqrt((target - current).LengthSquared()) / kHavokPerUnit;
	using SetVectorFn = void(__thiscall*)(void* motion, const float* v);
	const UInt32 linearSlot = VtableSlot(motion, kMotionSetLinearVelocitySlot);
	const UInt32 angularSlot = VtableSlot(motion, kMotionSetAngularVelocitySlot);
	if (HandBodyPlaceInstead(gap)) {
		alignas(16) float place[4] = {target.x, target.y, target.z, 0.0f};
		alignas(16) float turn[4];
		RotationQuaternion(rot, turn);
		using PlaceFn = void(__thiscall*)(void* body, const float* pos, const float* rot);
		reinterpret_cast<PlaceFn>(VtableSlot(b.wrapper, kBodySetTranslationAndRotationSlot))(
			reinterpret_cast<void*>(b.wrapper), place, turn);
		alignas(16) const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		reinterpret_cast<SetVectorFn>(linearSlot)(reinterpret_cast<void*>(motion), zero);
		reinterpret_cast<SetVectorFn>(angularSlot)(reinterpret_cast<void*>(motion), zero);
		if (g_placeLinesLeft > 0) {
			--g_placeLinesLeft;
			OBVR_LOG("Hands: %s's body placed at the hand (%.0f units away)", SlotName(slot),
			         static_cast<double>(gap));
		}
		return;
	}
	const NiPoint3 v = HardKeyframeLinear(current, target, stepSeconds);
	const NiPoint3 w = HardKeyframeAngular(RotationFromColumns(columns), rot, stepSeconds);
	using ActivateFn = void(__thiscall*)(void* body);
	reinterpret_cast<ActivateFn>(kActivateBody)(reinterpret_cast<void*>(b.body));
	alignas(16) float linear[4] = {v.x, v.y, v.z, 0.0f};
	alignas(16) float angular[4] = {w.x, w.y, w.z, 0.0f};
	reinterpret_cast<SetVectorFn>(linearSlot)(reinterpret_cast<void*>(motion), linear);
	reinterpret_cast<SetVectorFn>(angularSlot)(reinterpret_cast<void*>(motion), angular);
	if (!b.reportedDrive) {
		b.reportedDrive = true;
		OBVR_LOG("Hands: %s's body driven - %.1f units from the hand, %.2f m/s, %.1f rad/s, physics %.4f s this frame (%u steps of %.4f)",
		         SlotName(slot), static_cast<double>(gap), static_cast<double>(math::Sqrt(v.LengthSquared()) / 10.0f),
		         static_cast<double>(math::Sqrt(w.LengthSquared())), static_cast<double>(stepSeconds),
		         Read(kPlannerStepCount), static_cast<double>(*reinterpret_cast<const float*>(kPlannerStepSeconds)));
	}
}

void StepPhysicsRate(float rateHz) {
	float& step = *reinterpret_cast<float*>(kPhysicsStepRuntime);
	const float want = PhysicsStepFor(rateHz);
	if (want > 0.0f) {
		if (!g_stepChanged) {
			g_originalStep = step;
			g_stepChanged = true;
			OBVR_LOG("Hands: the physics step %.5f s -> %.5f s (%.0f Hz)", static_cast<double>(step),
			         static_cast<double>(want), static_cast<double>(1.0f / want));
		}
		if (step != want) {
			step = want;  // a new world writes the INI's value again
		}
	} else if (g_stepChanged) {
		step = g_originalStep;
		g_stepChanged = false;
		OBVR_LOG("Hands: the physics step back to %.5f s", static_cast<double>(g_originalStep));
	}
}

// Layer 23's row without the character controllers (HandBodyLogic.h,
// pushing the people): 0x008A83C0 fills the whole matrix with every bit,
// so this is looked at every step, not once.
void KeepQuietLayerOffPeople() {
	const UInt32 row = Read(kLayerMatrix + kHandBodyQuietLayer * 4);
	if ((row & (1u << kCharControllerLayer)) == 0) {
		return;
	}
	using SetterFn = void(__cdecl*)(UInt32 layerA, UInt32 layerB, bool collide);
	reinterpret_cast<SetterFn>(kLayerMatrixSetter)(kHandBodyQuietLayer, kCharControllerLayer, false);
	static UInt32 linesLeft = 3;
	if (linesLeft > 0) {
		--linesLeft;
		OBVR_LOG("Hands: layer %u no longer collides with the character controllers (layer %u)",
		         kHandBodyQuietLayer, kCharControllerLayer);
	}
}

// A body in the world whose filter is to change (pushing people or not):
// written where the engine's own setters write it, then handed to the world.
void Refilter(Body& b, bool pushesActors, int slot) {
	if (!LooksLikeObject(b.body)) {
		return;
	}
	UInt32* const filter = reinterpret_cast<UInt32*>(b.body + kBodyFilterOffset);
	const UInt32 wanted = HandBodyFilter(FilterGroup(*filter), (*filter & kFilterNoCollision) == 0, pushesActors);
	if (!HandBodyRefilterNeeded(*filter, wanted)) {
		return;
	}
	*filter = wanted;
	using RefreshFn = void(__thiscall*)(void* wrapper);
	reinterpret_cast<RefreshFn>(VtableSlot(b.wrapper, kVtblRefreshFilter))(reinterpret_cast<void*>(b.wrapper));
	static UInt32 linesLeft = 12;
	if (linesLeft > 0) {
		--linesLeft;
		OBVR_LOG("Hands: %s's body %s people now (layer %u)", SlotName(slot),
		         pushesActors ? "pushes" : "no longer pushes", FilterLayer(wanted));
	}
}

}  // namespace

UInt32 HandBodyPhysicsSteps() { return Read(kPlannerStepCount); }

bool HavokWorldBoxOf(UInt32 node, NiPoint3& low, NiPoint3& high) {
	if (!g_verified || !LooksLikeObject(node)) {
		return false;
	}
	// Only bodies of the class the hands' own are: without one of ours to
	// compare with, nothing is read.
	UInt32 knownBody = 0;
	for (const Body& b : g_bodies) {
		if (LooksLikeObject(b.body)) {
			knownBody = b.body;
			break;
		}
	}
	if (knownBody == 0) {
		return false;
	}
	using CollisionOfFn = UInt32(__cdecl*)(UInt32 node);
	const UInt32 collision = reinterpret_cast<CollisionOfFn>(kCollisionObjectOf)(node);
	const UInt32 wrapper = LooksLikeObject(collision) ? Read(collision + kCollisionObjectBodyOffset) : 0;
	const UInt32 body = LooksLikeObject(wrapper) ? Read(wrapper + kWrapperObjOffset) : 0;
	if (!LooksLikeObject(body) || Read(body) != Read(knownBody)) {
		return false;
	}
	const UInt32 shape = Read(body + kBodyShapeOffset);
	const UInt32 transform = Read(body + kBodyTransformOffset);
	if (!LooksLikeObject(shape) || !LooksLikeObject(transform) || !LooksLikeObject(Read(shape))) {
		return false;
	}
	const UInt32 getAabb = VtableSlot(shape, kShapeGetAabbSlot);
	if (!LooksLikeObject(getAabb)) {
		return false;
	}
	alignas(16) float aabb[8] = {};
	{
		Lock lock;
		using GetAabbFn = void(__thiscall*)(void* shape, const void* transform, float tolerance, float* out);
		reinterpret_cast<GetAabbFn>(getAabb)(reinterpret_cast<void*>(shape), reinterpret_cast<const void*>(transform),
		                                     0.0f, aabb);
	}
	low = NiPoint3{aabb[0] / kHavokPerUnit, aabb[1] / kHavokPerUnit, aabb[2] / kHavokPerUnit};
	high = NiPoint3{aabb[4] / kHavokPerUnit, aabb[5] / kHavokPerUnit, aabb[6] / kHavokPerUnit};
	return true;
}

void LogHandBodies() {
	char text[320];
	int at = 0;
	for (int i = 0; i < static_cast<int>(HandBodySlot::Count) && at < 250; ++i) {
		const Body& b = g_bodies[i];
		const UInt32 motion = LooksLikeObject(b.body) ? Read(b.body + kBodyMotionOffset) : 0;
		if (!LooksLikeObject(motion)) {
			at += std::snprintf(text + at, sizeof(text) - at, "%s%s none", at > 0 ? "; " : "", SlotName(i));
			continue;
		}
		const float* p = reinterpret_cast<const float*>(motion + kMotionTranslationOffset);
		at += std::snprintf(text + at, sizeof(text) - at, "%s%s at %.1f %.1f %.1f%s", at > 0 ? "; " : "", SlotName(i),
		                    static_cast<double>(p[0] / kHavokPerUnit), static_cast<double>(p[1] / kHavokPerUnit),
		                    static_cast<double>(p[2] / kHavokPerUnit), BodyWorld(b) != 0 ? "" : " (out of the world)");
	}
	OBVR_LOG("Hands: bodies - %s", at > 0 ? text : "none");
}

bool VerifyHandBodyAddresses() {
	bool ok = true;
	for (const Expected& e : kExpected) {
		if (!mem::Verify(e.address, e.bytes, e.size)) {
			OBVR_LOG("Hands: body collision - %s at %08X is not the bytes read; the bodies stay off", e.name,
			         e.address);
			ok = false;
		}
	}
	for (const Slot& s : kVtableSlots) {
		if (Read(kRigidBodyVtable + s.offset) != s.value) {
			OBVR_LOG("Hands: body collision - bhkRigidBody's vtable +%02X is %08X, not %08X; the bodies stay off",
			         s.offset, Read(kRigidBodyVtable + s.offset), s.value);
			ok = false;
		}
	}
	for (const Slot& s : kMotionSlots) {
		if (Read(kKeyframedMotionVtable + s.offset) != s.value) {
			OBVR_LOG("Hands: body collision - the keyframed motion's vtable +%02X is %08X, not %08X; the bodies "
			         "stay off",
			         s.offset, Read(kKeyframedMotionVtable + s.offset), s.value);
			ok = false;
		}
	}
	g_verified = ok;
	OBVR_LOG("Hands: body collision - %u functions and %u table slots %s", static_cast<UInt32>(sizeof(kExpected) / sizeof(kExpected[0])),
	         static_cast<UInt32>(sizeof(kVtableSlots) / sizeof(kVtableSlots[0]) + sizeof(kMotionSlots) / sizeof(kMotionSlots[0])),
	         ok ? "match what was read" : "do NOT all match - off");
	return ok;
}

HandBodyReport StepHandBodies(const HandBodyFrame& frame) {
	HandBodyReport report;
	if (!g_verified) {
		return report;
	}
	Lock lock;
	StepPhysicsRate(frame.enabled ? frame.physicsRate : 0.0f);
	if (frame.enabled) {
		KeepQuietLayerOffPeople();
	}
	const UInt32 bhkWorld = frame.enabled ? PlayerBhkWorld() : 0;
	const UInt32 playerWorld = LooksLikeObject(bhkWorld) ? Read(bhkWorld + kBhkWorldHkWorldOffset) : 0;
	const UInt32 plannerSteps = Read(kPlannerStepCount);
	const float stepSeconds =
		HandBodyDriveSeconds(plannerSteps, *reinterpret_cast<const float*>(kPlannerStepSeconds));
	const int weapon = static_cast<int>(HandBodySlot::Weapon);

	// Another weapon: its blade's body is made anew.
	Body& blade = g_bodies[weapon];
	if (blade.wrapper != 0 && frame.valid[weapon] && BladeNeedsRebuild(blade.bladeUnits, frame.bladeUnits)) {
		if (BodyWorld(blade) != 0 && WorldSound(BodyWorld(blade))) {
			Leave(blade, weapon);
		}
		OBVR_LOG("Hands: the weapon's blade is %.0f units now (its body was %.0f) - a new body",
		         static_cast<double>(frame.bladeUnits), static_cast<double>(blade.bladeUnits));
		blade = Body{};
	}

	// A body whose drawn hand or blade has moved away from its span for a
	// while (HandBodyLogic.h, the shapes from what is drawn): made anew.
	for (int i = 0; i < static_cast<int>(HandBodySlot::Count); ++i) {
		Body& b = g_bodies[i];
		if (b.wrapper == 0 || !frame.valid[i] || !frame.span[i].valid) {
			b.spanFrames = 0;
			continue;
		}
		const bool moved = b.span.valid ? SpanMoved(b.span, frame.span[i]) : true;
		if (!StepSpanWatch(b.spanFrames, moved)) {
			continue;
		}
		if (BodyWorld(b) != 0 && WorldSound(BodyWorld(b))) {
			Leave(b, i);
		}
		OBVR_LOG("Hands: %s's drawn span is %.1f %.1f %.1f to %.1f %.1f %.1f now (the body's %s) - a new body",
		         SlotName(i), static_cast<double>(frame.span[i].a.x), static_cast<double>(frame.span[i].a.y),
		         static_cast<double>(frame.span[i].a.z), static_cast<double>(frame.span[i].b.x),
		         static_cast<double>(frame.span[i].b.y), static_cast<double>(frame.span[i].b.z),
		         b.span.valid ? "was another" : "was the constants");
		b = Body{};
	}

	for (int i = 0; i < static_cast<int>(HandBodySlot::Count); ++i) {
		Body& b = g_bodies[i];
		HandBodyState s;
		s.wanted = frame.enabled && frame.valid[i];
		s.made = b.wrapper != 0;
		s.bodyWorld = BodyWorld(b);
		s.bodyWorldSound = s.bodyWorld == 0 || WorldSound(s.bodyWorld);
		s.playerWorld = playerWorld;
		switch (DecideHandBody(s)) {
		case HandBodyAction::None:
			break;
		case HandBodyAction::Abandon:
			OBVR_LOG("Hands: %s's body stands in a world that is gone (%08X) - left, a new one made", SlotName(i),
			         s.bodyWorld);
			b = Body{};
			break;
		case HandBodyAction::Leave:
			Leave(b, i);
			break;
		case HandBodyAction::Create: {
			// From the drawn hand or blade when it could be read, else the
			// constants (HandCapsule, BladeCapsule).
			const float radius = i == weapon ? kBladeBodyRadiusUnits : kHandBodyRadiusUnits;
			CapsuleSpec capsule = HandCapsule(kHandBodyRadiusUnits);
			if (frame.span[i].valid) {
				capsule = CapsuleFromSpan(frame.span[i], radius);
			} else if (i == weapon && !BladeCapsule(frame.bladeUnits, kBladeBodyRadiusUnits, capsule)) {
				break;
			}
			if (!Create(b, capsule, frame.rot[i], frame.pos[i], i, frame.pushesActors[i])) {
				b = Body{};
				break;
			}
			b.bladeUnits = i == weapon ? frame.bladeUnits : 0.0f;
			b.span = frame.span[i];
			Enter(b, bhkWorld, i);
			// Made at the world's origin whatever the block said: to the hand
			// at once, before a step can run it through what stands there.
			Drive(b, frame.rot[i], frame.pos[i], stepSeconds, i);
			report.live[i] = BodyWorld(b) == playerWorld;
			break;
		}
		case HandBodyAction::Enter:
			Enter(b, bhkWorld, i);
			report.live[i] = BodyWorld(b) == playerWorld;
			break;
		case HandBodyAction::Drive:
			Refilter(b, frame.pushesActors[i], i);
			Drive(b, frame.rot[i], frame.pos[i], stepSeconds, i);
			report.live[i] = true;
			break;
		}
	}
	return report;
}

}  // namespace obvr::game
