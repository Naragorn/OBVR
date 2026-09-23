#include "game/VRMenuBridge.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/VRMenuIntentStub.h"
#include "platform/Win32Min.h"

#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#endif

namespace obvr::game::vrbridge {
namespace {

constexpr UInt8 kPushEntry[addr::kPushMenuStackEntryPatchSize] = {
	0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C,
};
constexpr UInt8 kRemoveEntry[addr::kRemoveMenuStackEntryPatchSize] = {
	0x51, 0x53, 0x55, 0x56, 0x8B, 0xF1,
};
constexpr UInt8 kUpdateEntry[kInterfaceManagerUpdatePatchSize] = {
	0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xC0,
};

// 0057D640 is __thiscall(InterfaceManager*, UInt32), ret 4.  The dead EDX
// slot makes __fastcall the byte-compatible spelling used by OBVR hooks.
using PushFn = SInt32(__fastcall*)(void* self, void* unusedEdx, UInt32 menuId);

// 0057CFE0 is __thiscall(InterfaceManager*, UInt32, UInt32), ret 8.  Its
// callers branch on eax < 0, so the signed return must be preserved.
using RemoveFn = SInt32(__fastcall*)(void* self, void* unusedEdx, UInt32 menuId,
	                                  UInt32 removeFlags);
using UpdateFn = void(__fastcall*)(void* self, void* unusedEdx);

PushFn g_pushOriginal = nullptr;
RemoveFn g_removeOriginal = nullptr;
UpdateFn g_updateOriginal = nullptr;
RequestController g_requests;
HookInstallState g_installState{};

struct PreparedHook {
	void* original = nullptr;
	UInt8 patch[addr::kPushMenuStackEntryPatchSize]{};
};

PreparedHook g_preparedPush{};
PreparedHook g_preparedRemove{};
PreparedHook g_preparedUpdate{};

struct UpdateHookState {
	bool installTried = false;
	bool patched = false;
	bool unsafe = false;
};
UpdateHookState g_updateState{};
RequestMailbox g_probeMailbox{};
bool g_runtimeProbeEnabled = false;
bool g_runtimeProbeConfigured = false;
UInt32 g_runtimeProbeThreadId = 0;
bool g_probeOpenDown = false;
bool g_probeCloseDown = false;
UInt32 g_probeActiveToken = 0;

constexpr int kProbeOpenVirtualKey = 0x78;   // F9: diagnostic open request
constexpr int kProbeCloseVirtualKey = 0x79;  // F10: diagnostic close request

bool IsForegroundProcess() {
	void* window = GetForegroundWindow();
	if (window == nullptr) {
		return false;
	}
	DWORD processId = 0;
#if defined(OBVR_NO_WINSDK)
	GetWindowThreadProcessId(window, &processId);
#else
	GetWindowThreadProcessId(reinterpret_cast<HWND>(window), &processId);
#endif
	return processId == GetCurrentProcessId();
}

bool ProbeKeyPressed(int virtualKey, bool& wasDown) {
	const bool isDown = (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
	const bool pressed = isDown && !wasDown;
	wasDown = isDown;
	return pressed;
}

void LogProbeSnapshot(const char* reason, const StackSnapshot& snapshot,
                      UInt32 token = 0) {
	const SessionIdentity current = g_requests.Current();
	const SessionLease lease = g_requests.Lease();
	OBVR_LOG("Native menu probe: %s token=%u phase=%u available=%u focused=%u gameplay=%u "
	         "root=%08X top=%03X stack=%03X,%03X,%03X,%03X,%03X,%03X,%03X,%03X,%03X,%03X "
	         "current=%08X/%u lease=%08X/%u pageRoot=unverified",
	         reason, token, static_cast<UInt32>(g_requests.Phase()), snapshot.available ? 1 : 0,
	         snapshot.focused ? 1 : 0, snapshot.gameplay ? 1 : 0, snapshot.interfaceRoot,
	         snapshot.topVisible, snapshot.entries[0], snapshot.entries[1], snapshot.entries[2],
	         snapshot.entries[3], snapshot.entries[4], snapshot.entries[5], snapshot.entries[6],
	         snapshot.entries[7], snapshot.entries[8], snapshot.entries[9], current.interfaceRoot,
	         current.generation, lease.identity.interfaceRoot, lease.identity.generation);
}

void LogProbeObservation(const char* reason, const Observation& observation,
                         const StackSnapshot& snapshot) {
	if (observation.event != LifecycleEvent::None || observation.foreignStack) {
		LogProbeSnapshot(reason, snapshot);
	}
}

StackSnapshot Capture(void* manager) {
	StackSnapshot snapshot{};
	if (manager == nullptr) {
		return snapshot;
	}

	snapshot.available = true;
	const auto* bytes = static_cast<const UInt8*>(manager);
	snapshot.focused = IsForegroundProcess();
	snapshot.gameplay = bytes[0x08] == 1;
	snapshot.interfaceRoot = *reinterpret_cast<const UInt32*>(
		bytes + addr::kInterfaceMenuRootOffset);
	for (UInt32 i = 0; i < kMenuStackSlots; ++i) {
		snapshot.entries[i] = *reinterpret_cast<const UInt32*>(
			bytes + addr::kInterfaceMenuStackOffset + i * sizeof(UInt32));
	}
	snapshot.topVisible = TopVisible(snapshot.entries);
	return snapshot;
}

void ObserveMutation(const char* operation, SInt32 result, UInt32 menuId,
                     UInt32 mutationReturn, void* manager) {
	const StackSnapshot snapshot = Capture(manager);
	const SessionLease leaseBeforeMutation = g_requests.Lease();
	const SessionIdentity expected{NativeIntentGateState().ExpectedRoot(),
	                              NativeIntentGateState().ExpectedGeneration()};
	const bool leaseMatches = leaseBeforeMutation.Valid() &&
	                          leaseBeforeMutation.identity == expected;
	const bool intentAcknowledged = NativeIntentGateState().ObserveMutation(
		result, menuId, mutationReturn, NativeIntentGateState().ActiveInvocationId(),
		ContainsPersonal(snapshot), NativeIntentActiveUpdateInvocation(), leaseMatches);
	const Observation observation = g_requests.Observe(
		snapshot, intentAcknowledged ? NativeIntentGateState().LastAcknowledgedToken() : 0);
	if (g_runtimeProbeEnabled) {
		LogProbeSnapshot(operation, snapshot,
		                 intentAcknowledged ? NativeIntentGateState().LastAcknowledgedToken() : 0);
	}
	if (result < 0) {
		OBVR_LOG("Menu bridge: %s refused/no-op result %d; stack top=%03X root=%08X",
		         operation, result, snapshot.topVisible, snapshot.interfaceRoot);
		return;
	}
	if (intentAcknowledged) {
		OBVR_LOG("Menu intent probe: acknowledged token=%u mutation=%08X root=%08X",
		         NativeIntentGateState().LastAcknowledgedToken(), mutationReturn,
		         snapshot.interfaceRoot);
	}
	if (observation.event != LifecycleEvent::None) {
		OBVR_LOG("Menu bridge: %s result %d, lifecycle event %u, top=%03X root=%08X "
		         "generation=%u",
		         operation, result, static_cast<UInt32>(observation.event),
		         snapshot.topVisible, snapshot.interfaceRoot, observation.identity.generation);
	}
}

void PrepareProbeFrame(void* manager) {
	if (!g_runtimeProbeEnabled || g_runtimeProbeThreadId == 0 ||
	    GetCurrentThreadId() != g_runtimeProbeThreadId || NativeIntentUpdateDepth() != 1) {
		return;
	}
	const StackSnapshot snapshot = Capture(manager);
	SetNativeIntentContextValid(snapshot.available && snapshot.focused);
	LogProbeObservation("frame", g_requests.Observe(snapshot), snapshot);

	Request requested{};
	const bool openPressed = ProbeKeyPressed(kProbeOpenVirtualKey, g_probeOpenDown);
	const bool closePressed = ProbeKeyPressed(kProbeCloseVirtualKey, g_probeCloseDown);
	if (openPressed) {
		if (!g_requests.RequestOpen(snapshot, requested)) {
			LogProbeSnapshot("open-refused", snapshot);
		} else if (!g_probeMailbox.Publish(requested)) {
			g_requests.CancelPending();
			LogProbeSnapshot("open-mailbox-refused", snapshot, requested.token);
		}
	} else if (closePressed) {
		if (!g_requests.RequestClose(snapshot, requested)) {
			LogProbeSnapshot("close-refused", snapshot);
		} else if (!g_probeMailbox.Publish(requested)) {
			g_requests.CancelPending();
			LogProbeSnapshot("close-mailbox-refused", snapshot, requested.token);
		}
	}

	Request command{};
	if (!g_probeMailbox.Consume(command)) {
		return;
	}
	if (!g_requests.Dispatch(snapshot, command)) {
		g_requests.ClearActiveFailure();
		LogProbeSnapshot("dispatch-refused", snapshot, command.token);
		return;
	}
	if (!ArmNativeRequest(command)) {
		g_requests.ClearActiveFailure();
		LogProbeSnapshot("native-arm-refused", snapshot, command.token);
		return;
	}
	g_probeActiveToken = command.token;
	OBVR_LOG("Native menu probe: armed operation=%u token=%u expected=%08X/%u update=%u",
	         static_cast<UInt32>(command.operation), command.token, command.expected.interfaceRoot,
	         command.expected.generation, NativeIntentActiveUpdateInvocation());
}

void FinishProbeFrame(void* manager) {
	if (!g_runtimeProbeEnabled || g_runtimeProbeThreadId == 0 ||
	    GetCurrentThreadId() != g_runtimeProbeThreadId || NativeIntentUpdateDepth() != 1) {
		return;
	}
	const StackSnapshot snapshot = Capture(manager);
	LogProbeObservation("frame-end", g_requests.Observe(snapshot), snapshot);
	const IntentPhase phase = NativeIntentGateState().Phase();
	if (g_probeActiveToken != 0 &&
	    (phase == IntentPhase::Rejected || phase == IntentPhase::Cancelled)) {
		OBVR_LOG("Native menu probe: token=%u cleared after native refusal/no-op/update exit "
		         "phase=%u", g_probeActiveToken, static_cast<UInt32>(phase));
		g_requests.ClearActiveFailure();
		g_probeActiveToken = 0;
	} else if (g_probeActiveToken != 0 && phase == IntentPhase::Acknowledged) {
		OBVR_LOG("Native menu probe: token=%u acknowledged; lease=%08X/%u", g_probeActiveToken,
		         g_requests.Lease().identity.interfaceRoot,
		         g_requests.Lease().identity.generation);
		g_probeActiveToken = 0;
	}
}

SInt32 __fastcall HookedPush(void* self, void* unusedEdx, UInt32 menuId) {
	const SInt32 result = g_pushOriginal(self, unusedEdx, menuId);
	const UInt32 mutationReturn =
#if defined(_MSC_VER)
		reinterpret_cast<UInt32>(_ReturnAddress());
#else
		0;
#endif
	// Read only after the engine has completed its stack mutation.  This makes
	// a same-frame remove/reinsert of the same pointer two lifecycle events.
	ObserveMutation("push", result, menuId, mutationReturn, self);
	return result;
}

SInt32 __fastcall HookedRemove(void* self, void* unusedEdx, UInt32 menuId,
	                             UInt32 removeFlags) {
	const SInt32 result = g_removeOriginal(self, unusedEdx, menuId, removeFlags);
	const UInt32 mutationReturn =
#if defined(_MSC_VER)
		reinterpret_cast<UInt32>(_ReturnAddress());
#else
		0;
#endif
	ObserveMutation("remove", result, menuId, mutationReturn, self);
	return result;
}

void __fastcall HookedUpdate(void* self, void* unusedEdx) {
	if (g_runtimeProbeEnabled && !g_runtimeProbeConfigured) {
		// This is the verified InterfaceManager::Update entry, so its first
		// invocation establishes the actual game-loop thread rather than the
		// plugin load thread that installed the detour.
		g_runtimeProbeThreadId = GetCurrentThreadId();
		ConfigureNativeIntentProbe(g_runtimeProbeThreadId, kOpenIntentQueryContinuation,
		                           kCloseIntentQueryContinuation);
		SetNativeIntentContextValid(false);
		g_runtimeProbeConfigured = true;
		OBVR_LOG("Native menu probe: game Update thread established as %u", g_runtimeProbeThreadId);
	}
	struct UpdateCall {
		UpdateFn original;
		void* self;
		void* unusedEdx;
	};
	const auto invoke = [](void* context) {
		auto* call = static_cast<UpdateCall*>(context);
		PrepareProbeFrame(call->self);
		call->original(call->self, call->unusedEdx);
		FinishProbeFrame(call->self);
	};
	UpdateCall call{g_updateOriginal, self, unusedEdx};
	RunNativeIntentUpdate(invoke, &call);
}

PreparedHook& Prepared(HookSite site) {
	if (site == HookSite::Push) {
		return g_preparedPush;
	}
	return g_preparedRemove;
}

const UInt8* Expected(HookSite site) {
	return site == HookSite::Push ? kPushEntry : kRemoveEntry;
}

UInt32 Address(HookSite site) {
	return site == HookSite::Push ? addr::kPushMenuStackEntry
	                             : addr::kRemoveMenuStackEntry;
}

UInt32 Length(HookSite site) {
	return site == HookSite::Push ? sizeof(kPushEntry) : sizeof(kRemoveEntry);
}

bool VerifySite(HookSite site, void*) {
	return mem::Verify(Address(site), Expected(site), Length(site));
}

bool PrepareSite(HookSite site, void** original, void*) {
	PreparedHook& prepared = Prepared(site);
	constexpr UInt32 kCapacity = 32;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kCapacity));
	if (trampoline == nullptr) {
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 size = mem::BuildEntryTrampoline(trampoline, kCapacity, trampolineAddress,
	                                              Address(site), Expected(site), Length(site));
	if (size == 0) {
		return false;
	}

	void* replacement = site == HookSite::Push ? reinterpret_cast<void*>(&HookedPush)
	                                           : reinterpret_cast<void*>(&HookedRemove);
	if (mem::BuildEntryPatch(prepared.patch, sizeof(prepared.patch), Address(site),
	                         reinterpret_cast<UInt32>(replacement), Length(site)) !=
	    Length(site)) {
		return false;
	}
	prepared.original = trampoline;
	*original = trampoline;
	return true;
}

bool WriteSite(HookSite site, bool restore, void*) {
	const UInt8* bytes = restore ? Expected(site) : Prepared(site).patch;
	return mem::SafeWrite(Address(site), bytes, Length(site));
}

bool InstallUpdateHook() {
	if (g_updateState.unsafe || g_updateState.installTried) {
		return g_updateState.patched;
	}
	g_updateState.installTried = true;
	char observedHash[65]{};
	if (!ReadNativeIntentExecutableHash(observedHash, sizeof(observedHash))) {
		OBVR_LOG("Menu bridge: Update executable hash could not be read; expected %s",
		         kExpectedOblivionSha256);
		return false;
	}
	if (!NativeIntentHashMatches(observedHash)) {
		OBVR_LOG("Menu bridge: Update executable hash mismatch; expected %s observed %s",
		         kExpectedOblivionSha256, observedHash);
		return false;
	}
	if (!mem::Verify(kInterfaceManagerUpdate, kUpdateEntry,
	                 kInterfaceManagerUpdatePatchSize)) {
		mem::ReportForeignCode("Menu bridge Update", kInterfaceManagerUpdate);
		return false;
	}
	constexpr UInt32 kCapacity = 32;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kCapacity));
	if (trampoline == nullptr) {
		return false;
	}
	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	if (mem::BuildEntryTrampoline(trampoline, kCapacity, trampolineAddress,
	                              kInterfaceManagerUpdate, kUpdateEntry,
	                              kInterfaceManagerUpdatePatchSize) == 0 ||
	    mem::BuildEntryPatch(g_preparedUpdate.patch, sizeof(g_preparedUpdate.patch),
	                         kInterfaceManagerUpdate,
	                         reinterpret_cast<UInt32>(&HookedUpdate),
	                         kInterfaceManagerUpdatePatchSize) == 0) {
		VirtualFree(trampoline, 0, MEM_RELEASE);
		return false;
	}
	if (!mem::SafeWrite(kInterfaceManagerUpdate, g_preparedUpdate.patch,
	                    kInterfaceManagerUpdatePatchSize)) {
		VirtualFree(trampoline, 0, MEM_RELEASE);
		return false;
	}
	g_preparedUpdate.original = trampoline;
	g_updateOriginal = reinterpret_cast<UpdateFn>(trampoline);
	g_updateState.patched = true;
	return true;
}

bool RestoreUpdateHook() {
	if (!g_updateState.patched) {
		return true;
	}
	if (!mem::SafeWrite(kInterfaceManagerUpdate, kUpdateEntry,
	                    kInterfaceManagerUpdatePatchSize)) {
		g_updateState.unsafe = true;
		OBVR_LOG("Menu bridge: UNSAFE Update rollback failure; trampoline remains reachable");
		return false;
	}
	VirtualFree(g_preparedUpdate.original, 0, MEM_RELEASE);
	g_preparedUpdate.original = nullptr;
	g_updateOriginal = nullptr;
	g_updateState.patched = false;
	return true;
}

}  // namespace

bool InstallVRMenuBridge() {
	if (!InstallUpdateHook()) {
		OBVR_LOG("Menu bridge: Update wrapper not installed; lifecycle hooks remain disabled");
		return false;
	}
	const HookInstallOperations operations = {&VerifySite, &PrepareSite, &WriteSite, nullptr};
	const HookInstallResult result = InstallHookPair(operations, g_installState);
	if (result == HookInstallResult::Installed ||
	    result == HookInstallResult::AlreadyInstalled) {
		g_pushOriginal = reinterpret_cast<PushFn>(g_preparedPush.original);
		g_removeOriginal = reinterpret_cast<RemoveFn>(g_preparedRemove.original);
		// The close query is allowed to override only while this lifecycle
		// observer can prove the current id1 anchor belongs to its session.
		SetNativeIntentOwnershipProbe(&IsOwnedPersonalSession);
		OBVR_LOG("Menu bridge: stack lifecycle hooks installed at %08X/%08X; "
		         "Update boundary and post-mutation generation tracking are active",
		         addr::kPushMenuStackEntry, addr::kRemoveMenuStackEntry);
		return true;
	}

	if (result == HookInstallResult::PushSignatureMismatch) {
		mem::ReportForeignCode("Menu bridge push", addr::kPushMenuStackEntry);
	} else if (result == HookInstallResult::RemoveSignatureMismatch) {
		mem::ReportForeignCode("Menu bridge remove", addr::kRemoveMenuStackEntry);
	}
	if (result == HookInstallResult::UnsafeRollbackFailure) {
		// Keep the prepared push trampoline reachable if restoration failed.  A
		// blind retry would overwrite a live handler and is forbidden.
		g_pushOriginal = reinterpret_cast<PushFn>(g_preparedPush.original);
		OBVR_LOG("Menu bridge: UNSAFE rollback failure; push handler remains owned, "
		         "remove hook is not installed and retry is refused");
	} else {
		OBVR_LOG("Menu bridge: install result %u; lifecycle hook disabled",
		         static_cast<UInt32>(result));
	}
	RestoreUpdateHook();
	return false;
}

bool IsVRMenuBridgeInstalled() {
	return g_pushOriginal != nullptr && g_removeOriginal != nullptr &&
	       g_updateOriginal != nullptr;
}

bool InstallNativeMenuLifecycleProbe() {
	if (g_runtimeProbeEnabled) {
		return true;
	}
	if (!InstallVRMenuBridge()) {
		OBVR_LOG("Native menu probe: lifecycle/update hooks refused; query sites untouched");
		return false;
	}
	if (!InstallNativeIntentProbe()) {
		OBVR_LOG("Native menu probe: query seam refused after lifecycle hooks installed; "
		         "probe remains disabled");
		return false;
	}
	g_runtimeProbeEnabled = true;
	OBVR_LOG("Native menu probe: guarded lifecycle and query seams installed; "
	         "diagnostic F9=open F10=close");
	return true;
}

bool IsNativeMenuLifecycleProbeEnabled() { return g_runtimeProbeEnabled; }

bool IsOwnedPersonalSession(UInt32 manager, UInt32 expectedRoot,
	                        UInt32 expectedGeneration) {
	const StackSnapshot snapshot = Capture(reinterpret_cast<void*>(manager));
	const SessionLease lease = g_requests.Lease();
	const SessionIdentity expected{expectedRoot, expectedGeneration};
	return lease.Valid() && lease.identity == expected && snapshot.available &&
	       snapshot.interfaceRoot == expected.interfaceRoot && ContainsPersonal(snapshot) &&
	       g_requests.Current() == expected;
}

bool ArmNativeRequest(const Request& request) {
	return ::obvr::game::vrbridge::ArmNativeRequest(
		request.token, static_cast<UInt32>(request.operation), request.expected.interfaceRoot,
		request.expected.generation);
}

}  // namespace obvr::game::vrbridge
