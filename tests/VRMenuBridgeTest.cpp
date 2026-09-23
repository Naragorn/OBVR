#include <cstdio>
#include <algorithm>
#include <atomic>
#include <array>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "core/EntryDetour.h"
#include "game/VRMenuBridge.h"
#include "game/VRMenuIntentStub.h"

using namespace obvr::game::vrbridge;
using namespace obvr::game;

namespace {

int checks = 0;
int failures = 0;

void Check(bool condition, const char* label) {
	++checks;
	if (!condition) {
		++failures;
		std::printf("FAIL: %s\n", label);
	}
}

StackSnapshot Gameplay() {
	StackSnapshot snapshot{};
	snapshot.available = true;
	snapshot.focused = true;
	snapshot.gameplay = true;
	return snapshot;
}

StackSnapshot Personal(UInt32 root, UInt32 top = kPersonalStackId) {
	StackSnapshot snapshot = Gameplay();
	snapshot.interfaceRoot = root;
	snapshot.entries[0] = kPersonalStackId;
	snapshot.topVisible = top;
	return snapshot;
}

void TestStackReads() {
	UInt32 entries[kMenuStackSlots] = {kPersonalStackId, kMenuIdInventory, 0, 99, 0,
	                                   0, 0, 0, 0, 0};
	Check(TopVisible(entries) == kMenuIdInventory, "top scan stops at the first empty slot");
	StackSnapshot empty = Gameplay();
	Check(Empty(empty) && !ContainsPersonal(empty), "empty gameplay has no native menu anchor");
	empty.entries[0] = 99;
	Check(!Empty(empty) && !ContainsPersonal(empty), "foreign stack is not personal");
	empty.entries[0] = kPersonalStackId;
	Check(ContainsPersonal(empty), "personal anchor is read from the active stack");
	Check(!EligibleForOpen(empty), "personal stack refuses a new open request");
}

void TestLifecycle() {
	Lifecycle lifecycle;
	StackSnapshot empty = Gameplay();
	auto observation = lifecycle.Observe(empty);
	Check(observation.event == LifecycleEvent::None && lifecycle.Known(),
	      "initial empty snapshot establishes known closed world");

	auto first = lifecycle.Observe(Personal(0x1000));
	Check(first.event == LifecycleEvent::Opened && first.identity.interfaceRoot == 0x1000 &&
	          first.identity.generation == 1,
	      "personal insertion creates generation one");
	const SessionIdentity identity = first.identity;

	auto page = Personal(0x1000, kMenuIdStats);
	page.entries[1] = kMenuIdStats;
	page.topVisible = kMenuIdStats;
	auto pageChange = lifecycle.Observe(page);
	Check(pageChange.event == LifecycleEvent::None && lifecycle.Current() == identity,
	      "four tab page and child changes preserve one owned generation");

	page.entries[1] = 0;
	page.topVisible = kPersonalStackId;
	Check(lifecycle.Observe(page).event == LifecycleEvent::None,
	      "owned child removal does not close the personal session");

	auto closed = lifecycle.Observe(empty);
	Check(closed.event == LifecycleEvent::Closed && closed.previous == identity &&
	          !lifecycle.Present(),
	      "actual anchor removal closes the owned generation");

	auto reused = lifecycle.Observe(Personal(0x1000));
	Check(reused.event == LifecycleEvent::Opened && reused.identity.interfaceRoot == 0x1000 &&
	          reused.identity.generation == 2 && reused.identity != identity,
	      "same root pointer after close receives a new generation");

	auto replaced = lifecycle.Observe(Personal(0x2000));
	Check(replaced.event == LifecycleEvent::Replaced && replaced.previous == reused.identity &&
	          replaced.identity.generation == 3,
	      "root replacement invalidates the old session without sampling only the top id");

	StackSnapshot unavailable = Personal(0x2000);
	unavailable.available = false;
	auto lost = lifecycle.Observe(unavailable);
	Check(lost.event == LifecycleEvent::Unavailable && lifecycle.Present() &&
	          lifecycle.Current() == replaced.identity,
	      "unavailable bridge preserves identity instead of inventing a close");

	StackSnapshot foreign = Gameplay();
	foreign.entries[0] = kMenuIdInventory;
	foreign.topVisible = kMenuIdInventory;
	auto foreignObservation = lifecycle.Observe(foreign);
	Check(foreignObservation.event == LifecycleEvent::Closed &&
	          foreignObservation.foreignStack,
	      "personal removal records a foreign replacement separately");
}

void TestRequests() {
	const StackSnapshot empty = Gameplay();
	RequestController controller;
	Request open{};
	Check(controller.RequestOpen(empty, open) && open.operation == Operation::OpenPersonalMenu,
	      "eligible gameplay queues one explicit open request");
	Check(controller.Phase() == DispatchPhase::Queued && !controller.RequestOpen(empty, open),
	      "second open is rejected while the first is pending");
	Check(controller.CancelPending() == CancelResult::CancelledBeforeDispatch &&
	          controller.Phase() == DispatchPhase::None,
	      "pre dispatch cancellation removes a queued request");
	Check(controller.RequestOpen(empty, open) &&
	          controller.ExpirePending() == CancelResult::CancelledBeforeDispatch,
	      "pre dispatch timeout removes a queued request");

	Check(controller.RequestOpen(empty, open) && controller.Dispatch(empty, open) &&
	          controller.Phase() == DispatchPhase::Dispatched,
	      "dispatch records a command without calling it an acknowledgement");
	Check(controller.CancelPending() == CancelResult::CannotRevokeDispatchedInput &&
	          controller.Phase() == DispatchPhase::CancelledAfterDispatch,
	      "post dispatch cancellation reports that queued input cannot be revoked");
	const auto late = controller.Observe(Personal(0x3100));
	Check(late.event == LifecycleEvent::Opened &&
	          controller.Phase() == DispatchPhase::AmbiguousObservation,
	      "late personal insertion remains ambiguous after cancelled input");
	Check(!MatchesSynchronousOpenAcknowledgement(open, late, false) &&
	          MatchesSynchronousOpenAcknowledgement(open, late, true),
	      "open acknowledgement requires explicit synchronous engine evidence");

	RequestController foreignController;
	Check(foreignController.RequestOpen(empty, open) && foreignController.Dispatch(empty, open),
	      "second controller dispatches a fresh open request");
	StackSnapshot foreign = Gameplay();
	foreign.entries[0] = kMenuIdInventory;
	foreign.topVisible = kMenuIdInventory;
	Check(foreignController.Observe(foreign).foreignStack &&
	          foreignController.Phase() == DispatchPhase::ForeignObservation,
	      "foreign insertion cannot be attributed to the open command");

	RequestController closeController;
	Request explicitOpen{};
	Check(closeController.RequestOpen(empty, explicitOpen) &&
	          closeController.Dispatch(empty, explicitOpen),
	      "close lease begins with an explicitly dispatched open");
	const auto opened = closeController.Observe(Personal(0x4100), explicitOpen.token);
	Check(opened.event == LifecycleEvent::Opened && closeController.Lease().Valid(),
	      "only the causally acknowledged open creates a close lease");
	Request close{};
	StackSnapshot owned = Personal(0x4100, kMenuIdStats);
	owned.entries[1] = kMenuIdStats;
	Check(closeController.RequestClose(owned, close) &&
	          close.expected == opened.identity,
	      "owned session queues close even with a child page on top");
	Check(closeController.Dispatch(owned, close), "owned close is revalidated at dispatch");
	const auto closeAck = closeController.Observe(Gameplay());
	Check(MatchesCloseAcknowledgement(close, closeAck) &&
	          closeController.Phase() == DispatchPhase::None && !closeController.Lease().Valid(),
	      "observed anchor removal acknowledges matching close");

	RequestController legacy;
	legacy.Observe(Personal(0x6000));
	Check(!legacy.RequestClose(Personal(0x6000), close),
	      "a legacy or externally opened personal menu has no close lease");
	RequestController reusedLease;
	Request leasedOpen{};
	Check(reusedLease.RequestOpen(empty, leasedOpen) &&
	          reusedLease.Dispatch(empty, leasedOpen),
	      "same-pointer reuse test dispatches its explicit open");
	const auto firstLeaseOpen = reusedLease.Observe(Personal(0x6100), leasedOpen.token);
	Check(firstLeaseOpen.event == LifecycleEvent::Opened && reusedLease.Lease().identity == firstLeaseOpen.identity,
	      "same-pointer reuse test records the first open lease");
	const auto firstLease = reusedLease.Lease();
	Check(reusedLease.Observe(empty).event == LifecycleEvent::Closed &&
	          !reusedLease.Lease().Valid(),
	      "external close clears the open lease");
	Check(reusedLease.Observe(Personal(0x6100)).event == LifecycleEvent::Opened &&
	          !reusedLease.Lease().Valid() &&
	          reusedLease.Current() != firstLease.identity,
	      "same-root reopen receives a new generation without inheriting the old lease");
	Check(!EligibleForClose(Personal(0x6100), reusedLease.Current(), firstLease.identity),
	      "a stale same-root generation cannot authorize close");

	RequestController staleController;
	staleController.Observe(Personal(0x5100));
	Request stale{};
	StackSnapshot replacement = Personal(0x5200);
	Check(!staleController.RequestClose(replacement, stale),
	      "foreign root cannot close the previous owned session");
	RequestController closeGates;
	closeGates.Observe(Personal(0x5300));
	StackSnapshot closeNoFocus = Personal(0x5300);
	closeNoFocus.focused = false;
	StackSnapshot closeUnavailable = Personal(0x5300);
	closeUnavailable.available = false;
	Check(!closeGates.RequestClose(closeNoFocus, stale) &&
	          !closeGates.RequestClose(closeUnavailable, stale),
	      "focus or bridge loss refuses an owned close request");
	Check(!Valid(SessionIdentity{0x5300, 0}) &&
	          !EligibleForClose(Personal(0x5300), {0x5300, 0}, {0x5300, 0}),
	      "partial lifecycle identity cannot authorize close");

	StackSnapshot noFocus = empty;
	noFocus.focused = false;
	StackSnapshot noGame = empty;
	noGame.gameplay = false;
	StackSnapshot unavailable = empty;
	unavailable.available = false;
	RequestController gates;
	Check(!gates.RequestOpen(noFocus, open) && !gates.RequestOpen(noGame, open) &&
	          !gates.RequestOpen(unavailable, open),
	      "focus, gameplay, and bridge loss refuse opening");
}

void TestMailbox() {
	RequestMailbox mailbox;
	Request request{7, Operation::OpenPersonalMenu, {}};
	Request received{};
	Check(mailbox.Publish(request) && mailbox.Pending(), "producer publishes one request");
	Check(!mailbox.Publish(request), "second producer cannot overwrite pending work");
	Check(mailbox.Consume(received) && received.token == request.token && !mailbox.Pending(),
	      "game thread consumes the published request");
	Check(!mailbox.Consume(received), "empty mailbox has no fabricated request");
	Check(mailbox.Publish(request) && mailbox.Cancel(received) && !mailbox.Pending(),
	      "producer cancellation clears undispatched work");
	Request invalid{0, Operation::None, {}};
	Check(!mailbox.Publish(invalid), "zero or no op requests are refused");
}

struct InstallerFake {
	bool signatures[2] = {true, true};
	bool preparations[2] = {true, true};
	bool pushWrite = true;
	bool removeWrite = true;
	bool rollbackWrite = true;
	int verifyCalls = 0;
	int prepareCalls = 0;
	int writeCalls = 0;
	HookSite writes[8]{};
	bool restores[8]{};

	static int Index(HookSite site) { return site == HookSite::Push ? 0 : 1; }
	static bool Verify(HookSite site, void* context) {
		auto& fake = *static_cast<InstallerFake*>(context);
		++fake.verifyCalls;
		return fake.signatures[Index(site)];
	}
	static bool Prepare(HookSite site, void** original, void* context) {
		auto& fake = *static_cast<InstallerFake*>(context);
		++fake.prepareCalls;
		if (!fake.preparations[Index(site)]) {
			return false;
		}
		*original = reinterpret_cast<void*>(static_cast<UInt32>(Index(site) + 1));
		return true;
	}
	static bool Write(HookSite site, bool restore, void* context) {
		auto& fake = *static_cast<InstallerFake*>(context);
		if (fake.writeCalls < 8) {
			fake.writes[fake.writeCalls] = site;
			fake.restores[fake.writeCalls] = restore;
		}
		++fake.writeCalls;
		if (restore) {
			return fake.rollbackWrite;
		}
		return site == HookSite::Push ? fake.pushWrite : fake.removeWrite;
	}

	HookInstallOperations Operations() {
		return {&Verify, &Prepare, &Write, this};
	}
};

void TestInstaller() {
	Check(EvaluateUpdateHookInstall(false, true, true, true, true, true, true) ==
	          UpdateHookInstallCheck::Ready,
	      "Update wrapper installer admits matching hash, signature, and write");
	Check(EvaluateUpdateHookInstall(true, false, false, false, false, false, false) ==
	          UpdateHookInstallCheck::AlreadyInstalled,
	      "Update wrapper installed guard prevents a second patch");
	Check(EvaluateUpdateHookInstall(false, false, false, true, true, true, true) ==
	          UpdateHookInstallCheck::HashReadFailed &&
	          EvaluateUpdateHookInstall(false, true, false, true, true, true, true) ==
	              UpdateHookInstallCheck::HashMismatch,
	      "Update wrapper hash read and mismatch refuse before signature or write");
	Check(EvaluateUpdateHookInstall(false, true, true, false, true, true, true) ==
	          UpdateHookInstallCheck::SignatureMismatch &&
	          EvaluateUpdateHookInstall(false, true, true, true, false, true, true) ==
	              UpdateHookInstallCheck::PrepareFailed,
	      "Update wrapper signature and trampoline failures refuse before writes");
	Check(EvaluateUpdateHookInstall(false, true, true, true, true, false, true) ==
	          UpdateHookInstallCheck::WriteFailed &&
	          EvaluateUpdateHookInstall(false, true, true, true, true, false, false) ==
	              UpdateHookInstallCheck::RollbackFailed,
	      "Update wrapper write failure distinguishes safe restore from unsafe rollback");
	Check(EvaluateNativeIntentInstall(false, true, true, true, true) ==
	          NativeIntentInstallCheck::Ready,
	      "intent installer admits only a verified executable and both call signatures");
	Check(EvaluateNativeIntentInstall(true, false, false, false, false) ==
	          NativeIntentInstallCheck::AlreadyInstalled,
	      "installed guard wins before any further hash or signature work");
	Check(EvaluateNativeIntentInstall(false, false, false, true, true) ==
	          NativeIntentInstallCheck::HashReadFailed,
	      "unreadable executable hash refuses before either patch");
	Check(EvaluateNativeIntentInstall(false, true, false, true, true) ==
	          NativeIntentInstallCheck::HashMismatch,
	      "mismatched executable hash refuses before either patch");
	Check(EvaluateNativeIntentInstall(false, true, true, false, true) ==
	          NativeIntentInstallCheck::OpenSignatureMismatch &&
	          EvaluateNativeIntentInstall(false, true, true, true, false) ==
	              NativeIntentInstallCheck::CloseSignatureMismatch,
	      "both callsite signatures are checked only after a matching executable hash");
	{
		InstallerFake fake;
		HookInstallState state;
		Check(InstallHookPair({}, state) == HookInstallResult::InvalidPort &&
		          fake.verifyCalls == 0,
		      "invalid installer port refuses without touching either site");
	}
	{
		InstallerFake fake;
		fake.signatures[0] = false;
		HookInstallState state;
		Check(InstallHookPair(fake.Operations(), state) ==
		          HookInstallResult::PushSignatureMismatch && fake.writeCalls == 0 &&
		          fake.prepareCalls == 0,
		      "foreign push prologue refuses before any patch operation");
	}
	{
		InstallerFake fake;
		fake.signatures[1] = false;
		HookInstallState state;
		Check(InstallHookPair(fake.Operations(), state) ==
		          HookInstallResult::RemoveSignatureMismatch && fake.writeCalls == 0 &&
		          fake.prepareCalls == 0,
		      "foreign remove prologue refuses before either site is touched");
	}
	{
		InstallerFake fake;
		fake.preparations[0] = false;
		HookInstallState state;
		Check(InstallHookPair(fake.Operations(), state) == HookInstallResult::PushPrepareFailed &&
		          fake.writeCalls == 0 && !state.resourcesRetained,
		      "push trampoline failure leaves both sites untouched");
	}
	{
		InstallerFake fake;
		fake.preparations[1] = false;
		HookInstallState state;
		Check(InstallHookPair(fake.Operations(), state) ==
		          HookInstallResult::RemovePrepareFailed && fake.writeCalls == 0 &&
		          state.resourcesRetained,
		      "remove trampoline failure retains prepared push resources without patching");
	}
	{
		InstallerFake fake;
		fake.pushWrite = false;
		HookInstallState state;
		Check(InstallHookPair(fake.Operations(), state) == HookInstallResult::PushWriteFailed &&
		          fake.writeCalls == 1 && !state.pushPatched && !state.removePatched,
		      "first site write failure does not attempt the second site");
	}
	{
		InstallerFake fake;
		fake.removeWrite = false;
		HookInstallState state;
		Check(InstallHookPair(fake.Operations(), state) ==
		          HookInstallResult::RemoveWriteRolledBack && fake.writeCalls == 3 &&
		          fake.writes[0] == HookSite::Push && !fake.restores[0] &&
		          fake.writes[1] == HookSite::Remove && !fake.restores[1] &&
		          fake.writes[2] == HookSite::Push && fake.restores[2] &&
		          !state.pushPatched && !state.removePatched && state.resourcesRetained,
		      "second site failure restores the first and leaves no patch state");
	}
	{
		InstallerFake fake;
		fake.removeWrite = false;
		fake.rollbackWrite = false;
		HookInstallState state;
		Check(InstallHookPair(fake.Operations(), state) ==
		          HookInstallResult::UnsafeRollbackFailure && state.unsafe &&
		          state.pushPatched && !state.removePatched && state.resourcesRetained,
		      "rollback failure retains live resources and surfaces unsafe partial state");
		const int writes = fake.writeCalls;
		Check(InstallHookPair(fake.Operations(), state) == HookInstallResult::UnsafePartialState &&
		          fake.writeCalls == writes,
		      "unsafe partial state refuses a blind retry");
	}
	{
		InstallerFake fake;
		HookInstallState state;
		Check(InstallHookPair(fake.Operations(), state) == HookInstallResult::Installed &&
		          state.pushPatched && state.removePatched && fake.writeCalls == 2,
		      "both verified sites install in push then remove order");
		const int calls = fake.writeCalls;
		Check(InstallHookPair(fake.Operations(), state) == HookInstallResult::AlreadyInstalled &&
		          fake.writeCalls == calls,
		      "repeated install does not double patch");
	}
	{
		InstallerFake fake;
		HookInstallState state;
		state.pushPatched = true;
		Check(InstallHookPair(fake.Operations(), state) == HookInstallResult::UnsafePartialState &&
		          state.unsafe && fake.verifyCalls == 0,
		      "inconsistent pre-existing patch state refuses installation");
	}
}

void TestUpdateEntryRelocation() {
	constexpr UInt32 entry = kInterfaceManagerUpdate;
	constexpr UInt32 trampolineAddress = 0x20000000;
	constexpr UInt32 replacementAddress = 0x30000000;
	constexpr UInt8 entryBytes[kInterfaceManagerUpdatePatchSize] = {
		0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xC0};
	UInt8 trampoline[32]{};
	const UInt32 trampolineSize = obvr::mem::BuildEntryTrampoline(
		trampoline, sizeof(trampoline), trampolineAddress, entry, entryBytes,
		kInterfaceManagerUpdatePatchSize);
	const UInt32 returnDisplacement = *reinterpret_cast<const UInt32*>(trampoline + 7);
	const UInt32 returnTarget = trampolineAddress + trampolineSize + returnDisplacement;
	Check(trampolineSize == kInterfaceManagerUpdatePatchSize + 5 &&
	          std::equal(trampoline, trampoline + kInterfaceManagerUpdatePatchSize,
	                     entryBytes) && trampoline[6] == 0xE9 &&
	          returnTarget == entry + kInterfaceManagerUpdatePatchSize,
	      "Update trampoline copies push/ebp/and esp,-64 and returns past all six bytes");
	UInt8 patch[16]{};
	const UInt32 patchSize = obvr::mem::BuildEntryPatch(
		patch, sizeof(patch), entry, replacementAddress,
		kInterfaceManagerUpdatePatchSize);
	const UInt32 patchDisplacement = *reinterpret_cast<const UInt32*>(patch + 1);
	const UInt32 patchTarget = entry + 5 + patchDisplacement;
	Check(patchSize == kInterfaceManagerUpdatePatchSize && patch[0] == 0xE9 &&
	          patch[5] == 0x90 && patchTarget == replacementAddress,
	      "Update entry patch preserves a whole-instruction six-byte prologue");
}

void TestMailboxContention() {
	RequestMailbox mailbox;
	std::atomic<bool> start{false};
	std::atomic<int> accepted{0};
	Request first{21, Operation::OpenPersonalMenu, {}};
	Request second{22, Operation::CloseOwnedMenu, {0x1000, 1}};
	std::thread producerA([&] {
		while (!start.load(std::memory_order_acquire)) {
		}
		if (mailbox.Publish(first)) {
			accepted.fetch_add(1, std::memory_order_relaxed);
		}
	});
	std::thread producerB([&] {
		while (!start.load(std::memory_order_acquire)) {
		}
		if (mailbox.Publish(second)) {
			accepted.fetch_add(1, std::memory_order_relaxed);
		}
	});
	start.store(true, std::memory_order_release);
	producerA.join();
	producerB.join();
	Request consumed{};
	Check(accepted.load(std::memory_order_relaxed) == 1 && mailbox.Consume(consumed) &&
	          (consumed.token == first.token || consumed.token == second.token),
	      "contending producers admit exactly one complete request");
	Check(mailbox.Publish(first) && mailbox.Cancel(consumed) && consumed.token == first.token &&
	          !mailbox.Pending() && !mailbox.Consume(consumed),
	      "cancel returns the exact unconsumed request without overwrite or release loss");
	Check(mailbox.Publish(second) && mailbox.Consume(consumed) &&
	          consumed.token == second.token,
	      "a later request is admitted only after cancellation clears the slot");
}

using IntentStubFn = UInt32(__fastcall*)(void* self, void* unusedEdx, UInt32 control,
                                         UInt32 argument);

struct AbiCapture {
	IntentStubFn stub = nullptr;
	UInt32 result = 0;
	UInt32 calls = 0;
	UInt32 nestedCalls = 0;
	UInt32 self = 0;
	UInt32 unusedEdx = 0;
	UInt32 control = 0;
	UInt32 argument = 0;
	bool recurse = false;
	bool inTarget = false;
};

AbiCapture g_abi{};
bool g_ownedSessionForAbi = false;
bool g_holdUpdateScope = false;
UInt32 g_ownedRootForAbi = 0x4100;
UInt32 g_ownedGenerationForAbi = 1;

bool SyntheticOwnership(UInt32, UInt32 expectedRoot, UInt32 expectedGeneration) {
	return g_ownedSessionForAbi && expectedRoot == g_ownedRootForAbi &&
	       expectedGeneration == g_ownedGenerationForAbi;
}

extern "C" UInt32 __fastcall SyntheticQuery(void* self, void* unusedEdx,
                                             UInt32 control, UInt32 argument) {
	++g_abi.calls;
	g_abi.self = reinterpret_cast<UInt32>(self);
	g_abi.unusedEdx = reinterpret_cast<UInt32>(unusedEdx);
	g_abi.control = control;
	g_abi.argument = argument;
	if (g_abi.recurse && !g_abi.inTarget) {
		g_abi.inTarget = true;
		++g_abi.nestedCalls;
		g_abi.result = g_abi.stub(self, unusedEdx, control, argument);
		g_abi.inTarget = false;
	}
	return g_abi.result;
}

#if defined(_MSC_VER) && defined(_M_IX86)
UInt32 CallIntentWithCanaries(IntentStubFn stub, void* self, void* manager,
	                              UInt32 pageSelection, UInt32* ebxOut, UInt32* esiOut,
                              UInt32* ediOut, UInt32* edxOut, bool* stackOk,
                              bool* carryOut, bool enterUpdateScope = true) {
	const bool scoped = enterUpdateScope && NativeIntentUpdateEnter();
	UInt32 result = 0;
	UInt32 stackMarker = 0;
	UInt32 carryMarker = 0;
	__asm {
		mov ebx, 013572468h
		mov esi, manager
		mov edi, pageSelection
		mov edx, 0DEADBEEfh
		mov ecx, self
		stc
		push 0CAFEBABEh
		push 1
		push 0Fh
		call stub
		mov result, eax
		pushfd
		pop eax
		test eax, 1
		setnz al
		movzx eax, al
		mov carryMarker, eax
		cmp dword ptr [esp], 0CAFEBABEh
		jne stack_failed
		mov stackMarker, 1
	stack_failed:
		mov eax, ebxOut
		mov dword ptr [eax], ebx
		mov eax, esiOut
		mov dword ptr [eax], esi
		mov eax, ediOut
		mov dword ptr [eax], edi
		mov eax, edxOut
		mov dword ptr [eax], edx
		add esp, 4
	}
	*stackOk = stackMarker != 0;
	*carryOut = carryMarker != 0;
	if (scoped && !g_holdUpdateScope) {
		NativeIntentUpdateLeave();
	}
	return result;
}

struct UpdateHarness {
	IntentStubFn stub = nullptr;
	void* self = nullptr;
	void* manager = nullptr;
	UInt32 result = 0;
	UInt32 depthAtBody = 0;
	UInt32 depthAfterQuery = 0;
	UInt32 nestedDepth = 0;
	UInt32 nestedResult = 0;
	bool runNested = false;
	bool acknowledged = false;
};

void SyntheticNestedUpdateBody(void* context) {
	auto& harness = *static_cast<UpdateHarness*>(context);
	harness.nestedDepth = NativeIntentUpdateDepth();
	UInt32 ebx = 0, esi = 0, edi = 0, edx = 0;
	bool stackOk = false, carry = false;
	harness.nestedResult = CallIntentWithCanaries(
		harness.stub, harness.self, harness.manager, 0, &ebx, &esi, &edi, &edx,
		&stackOk, &carry, false);
}

void SyntheticUpdateBody(void* context) {
	auto& harness = *static_cast<UpdateHarness*>(context);
	harness.depthAtBody = NativeIntentUpdateDepth();
	UInt32 ebx = 0, esi = 0, edi = 0, edx = 0;
	bool stackOk = false, carry = false;
	harness.result = CallIntentWithCanaries(
		harness.stub, harness.self, harness.manager, 0, &ebx, &esi, &edi, &edx,
		&stackOk, &carry, false);
	harness.depthAfterQuery = NativeIntentUpdateDepth();
	if (harness.runNested) {
		RunNativeIntentUpdate(&SyntheticNestedUpdateBody, &harness);
	}
	const UInt32 invocation = NativeIntentGateState().ActiveInvocationId();
	harness.acknowledged = NativeIntentGateState().ObserveMutation(
		0, kMenuIdBigFour, kOpenIntentMutationReturn, invocation, true,
		NativeIntentActiveUpdateInvocation(), true);
}

UInt32 CallIntentInUpdate(IntentStubFn stub, void* self, void* manager,
                          UInt32 pageSelection, UInt32* ebxOut, UInt32* esiOut,
                          UInt32* ediOut, UInt32* edxOut, bool* stackOk,
                          bool* carryOut) {
	const UInt32 result = CallIntentWithCanaries(stub, self, manager, pageSelection,
	                                             ebxOut, esiOut, ediOut, edxOut,
	                                             stackOk, carryOut);
	return result;
}
#endif

void TestNativeIntentStub() {
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86)
	std::array<UInt8, 0x200> manager{};
	manager[0x08] = 1;
	*reinterpret_cast<UInt32*>(manager.data() + 0xE0) = 0;

	constexpr UInt32 kStubCapacity = 128;
	auto* executable = static_cast<UInt8*>(VirtualAlloc(
		nullptr, kStubCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	Check(executable != nullptr, "ABI harness allocates executable stub memory");
	if (executable == nullptr) {
		return;
	}
	const UInt32 stubAddress = reinterpret_cast<UInt32>(executable);
	const NativeIntentStubSpec spec{
		stubAddress,
		reinterpret_cast<UInt32>(&SyntheticQuery),
		reinterpret_cast<UInt32>(&NativeIntentStubEnter),
		reinterpret_cast<UInt32>(&NativeIntentStubResolve),
		reinterpret_cast<UInt32>(&NativeIntentStubLeave),
	};
	Check(BuildNativeIntentStub(executable, kStubCapacity, spec) != 0,
	      "ABI harness executes the generated per-invocation stub shape");
	Check(BuildNativeIntentStub(executable, 8, spec) == 0,
	      "stub generation refuses a too-small buffer");
	Check(!InstallNativeIntentProbe() && !IsNativeIntentProbeInstalled(),
	      "a non-Oblivion host hash refuses before either native query patch");
	g_abi = {};
	g_abi.stub = reinterpret_cast<IntentStubFn>(executable);
	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	Check(ArmNativeOpenIntent(101), "open token arms on the explicit probe seam");
	UInt32 ebx = 0, esi = 0, edi = 0, edx = 0;
	bool stackOk = false, carry = false;
	g_holdUpdateScope = true;
	const UInt32 first = CallIntentWithCanaries(
		g_abi.stub, reinterpret_cast<void*>(0x12345678), manager.data(), 0,
		&ebx, &esi, &edi, &edx, &stackOk, &carry);
	Check(first == 1 && g_abi.calls == 1 && g_abi.self == 0x12345678 &&
	          g_abi.control == 0x0F && g_abi.argument == 1,
	      "native target runs exactly once with this and both original arguments");
	Check(ebx == 0x13572468 && esi == reinterpret_cast<UInt32>(manager.data()) &&
	          edi == 0 && edx == 0xDEADBEEF && stackOk && carry,
	      "generated stub preserves register, stack, and flags canaries");
	Check(NativeIntentGateState().Phase() == IntentPhase::Admitted,
	      "override admits a bounded mutation scope rather than acknowledging early");
	const UInt32 invocation = NativeIntentGateState().ActiveInvocationId();
	Check(NativeIntentGateState().ObserveMutation(0, kMenuIdBigFour,
	                                               kOpenIntentMutationReturn, invocation, true,
                                               NativeIntentActiveUpdateInvocation(), true) &&
	          NativeIntentGateState().Phase() == IntentPhase::Acknowledged,
	      "only the expected id1 mutation and post-mutation anchor acknowledge");
	g_holdUpdateScope = false;
	NativeIntentUpdateLeave();

	// Exercise the same bounded wrapper used by HookedUpdate, with the
	// generated query stub running inside its body.  The body acknowledges the
	// post-query mutation before the wrapper expires the Update scope.
	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	g_abi.calls = 0;
	g_abi.result = 0;
	Check(ArmNativeOpenIntent(119), "Update wrapper case arms a fresh open token");
	UpdateHarness updateHarness{g_abi.stub, reinterpret_cast<void*>(0x12340001),
	                            manager.data()};
	RunNativeIntentUpdate(&SyntheticUpdateBody, &updateHarness);
	Check(updateHarness.depthAtBody == 1 && updateHarness.depthAfterQuery == 1 &&
	          updateHarness.result == 1 && g_abi.calls == 1,
	      "Update wrapper runs the generated query once inside a top-level scope");
	Check(updateHarness.acknowledged &&
	          NativeIntentGateState().Phase() == IntentPhase::Acknowledged &&
	          NativeIntentUpdateDepth() == 0,
	      "Update wrapper acknowledges before exit and expires its scope afterward");

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	g_abi.calls = 0;
	g_abi.result = 0;
	Check(ArmNativeOpenIntent(120), "nested Update wrapper case arms a token");
	UpdateHarness nestedHarness{g_abi.stub, reinterpret_cast<void*>(0x12340002),
	                            manager.data(), 0, 0, 0, 0, 0, true, false};
	RunNativeIntentUpdate(&SyntheticUpdateBody, &nestedHarness);
	Check(nestedHarness.depthAtBody == 1 && nestedHarness.nestedDepth == 2 &&
	          nestedHarness.nestedResult == 0 && g_abi.calls == 2,
	      "nested Update after the outer query passes through the original stub");
	Check(nestedHarness.acknowledged &&
	          NativeIntentGateState().Phase() == IntentPhase::Acknowledged &&
	          NativeIntentUpdateDepth() == 0,
	      "outer Update token remains attributable after nested wrapper exit");

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	g_abi.calls = 0;
	g_abi.result = 7;
	const UInt32 passthrough = CallIntentWithCanaries(
		g_abi.stub, reinterpret_cast<void*>(0x22222222), manager.data(), 0,
		&ebx, &esi, &edi, &edx, &stackOk, &carry);
	Check(passthrough == 7 && g_abi.calls == 1,
	      "without a token the original EAX and single native call pass through");

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	g_abi.result = 9;
	Check(ArmNativeOpenIntent(102), "native-request case arms independently");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0x33333333), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 9 &&
	          NativeIntentGateState().Phase() == IntentPhase::Rejected,
	      "native request wins and rejects the synthetic intent");

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	manager[0x08] = 1;
	*reinterpret_cast<UInt32*>(manager.data() + 0xE0) = 0;
	g_abi.result = 0;
	Check(ArmNativeOpenIntent(103), "page-selection case arms independently");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0x44444444), manager.data(),
	                             0x3EA, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 0 &&
	          NativeIntentGateState().Phase() == IntentPhase::Rejected,
	      "page selection wins and the open intent is not attributed");

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	Check(ArmNativeOpenIntent(104), "nested case arms independently");
	g_abi = {};
	g_abi.stub = reinterpret_cast<IntentStubFn>(executable);
	g_abi.result = 0;
	g_abi.recurse = true;
	g_holdUpdateScope = true;
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0x55555555), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 1,
	      "outer nested invocation overrides once");
	Check(g_abi.calls == 2 && g_abi.nestedCalls == 1,
	      "nested invocation passes through the original exactly once");
	Check(NativeIntentGateState().Phase() == IntentPhase::Admitted,
	      "outer invocation alone consumes the pending intent");
	g_holdUpdateScope = false;
	NativeIntentUpdateLeave();

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	Check(ArmNativeOpenIntent(105), "wrong-thread case arms independently");
	std::atomic<UInt32> workerResult{0};
	std::thread worker([&] {
		UInt32 a = 0, b = 0, c = 0, d = 0;
		bool s = false, f = false;
		workerResult.store(CallIntentWithCanaries(g_abi.stub,
		                                           reinterpret_cast<void*>(0x66666666),
		                                           manager.data(), 0, &a, &b, &c, &d, &s, &f),
		                  std::memory_order_release);
	});
	worker.join();
	Check(workerResult.load(std::memory_order_acquire) == 0 &&
	          NativeIntentGateState().Phase() == IntentPhase::Pending,
	      "wrong-thread invocation calls through without consuming the token");
	g_abi.result = 0;
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0x77777777), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 1,
	      "the owning thread can still consume the preserved token");

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0xDEAD);
	SetNativeIntentContextValid(true);
	g_abi.result = 0;
	Check(ArmNativeOpenIntent(106), "wrong-origin case arms independently");
	g_holdUpdateScope = true;
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0x88888888), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 0,
	      "wrong callsite continuation preserves the original result");
	Check(NativeIntentGateState().Phase() == IntentPhase::Pending,
	      "wrong callsite continuation passes through without consuming intent");
	g_holdUpdateScope = false;
	NativeIntentUpdateLeave();

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(false);
	g_abi.result = 0;
	Check(ArmNativeOpenIntent(107), "context-loss case arms independently");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0x99999999), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 0 &&
	          NativeIntentGateState().Phase() == IntentPhase::Cancelled,
	      "context loss consumes and cancels without overriding EAX");

	// The second verified query is the close branch.  It requires the native
	// state byte, an adapter-provided owned-session proof, and no native/page
	// request; its removal acknowledgement requires the anchor to be absent.
	SetNativeIntentOwnershipProbe(&SyntheticOwnership);
	manager[0x08] = 2;
	*reinterpret_cast<UInt32*>(manager.data() + 0xE0) = kMenuIdBigFour;
	g_ownedSessionForAbi = true;
	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	g_holdUpdateScope = true;
	g_abi = {};
	g_abi.stub = reinterpret_cast<IntentStubFn>(executable);
	Check(ArmNativeCloseIntent(108, 0x4100, 1), "owned close token arms on the second query seam");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA0001), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 1 &&
	          NativeIntentGateState().Phase() == IntentPhase::Admitted,
	      "owned close overrides only the no-request second query");
	const UInt32 closeInvocation = NativeIntentGateState().ActiveInvocationId();
	Check(NativeIntentGateState().ObserveMutation(0, kMenuIdBigFour,
	                                               kCloseIntentMutationReturn,
	                                               closeInvocation, false,
	                                               NativeIntentActiveUpdateInvocation(), true) &&
	          NativeIntentGateState().Phase() == IntentPhase::Acknowledged,
	      "close acknowledgement requires successful id1 removal and absent anchor");
	g_holdUpdateScope = false;
	NativeIntentUpdateLeave();

	NativeIntentGate directClose;
	OpenIntentQuery directCloseQuery{};
	directCloseQuery.callReturnAddress = kCloseIntentQueryContinuation;
	directCloseQuery.invocationDepth = 1;
	directCloseQuery.mainThread = true;
	directCloseQuery.contextValid = true;
	directCloseQuery.normalCloseState = true;
	directCloseQuery.ownedSession = true;
	directCloseQuery.updateDepth = 1;
	directCloseQuery.updateInvocationId = 77;
	directClose.BeginUpdate(77);
	Check(directClose.Arm(113, IntentKind::Close, 0x4100, 1) &&
	          directClose.Resolve(directCloseQuery).resolution == IntentResolution::OverrideClose,
	      "close gate admits only the verified second-query continuation");
	directClose.EndUpdate();

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	g_abi.result = 7;
	g_abi.calls = 0;
	Check(ArmNativeCloseIntent(109, 0x4100, 1), "native-close case arms independently");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA0002), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 7 &&
	          NativeIntentGateState().Phase() == IntentPhase::Rejected,
	      "native close request wins and preserves original EAX");

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	manager[0x08] = 2;
	g_abi.result = 0;
	Check(ArmNativeCloseIntent(110, 0x4100, 1), "page-close case arms independently");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA0003), manager.data(),
	                             0x3EA, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 0 &&
	          NativeIntentGateState().Phase() == IntentPhase::Rejected,
	      "page selection wins and does not trigger close");

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	manager[0x08] = 2;
	Check(ArmNativeCloseIntent(114, 0x4100, 1), "foreign-removal case arms independently");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA0006), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 1,
	      "foreign-removal case admits before observing the wrong mutation");
	const UInt32 foreignCloseInvocation = NativeIntentGateState().ActiveInvocationId();
	Check(!NativeIntentGateState().ObserveMutation(0, kMenuIdInventory,
                                                kCloseIntentMutationReturn,
                                                foreignCloseInvocation, false,
                                                NativeIntentActiveUpdateInvocation(), true) &&
	          NativeIntentGateState().Phase() == IntentPhase::Rejected,
	      "foreign menu removal rejects close attribution without auto-cleanup");

	g_ownedSessionForAbi = false;
	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	manager[0x08] = 2;
	Check(ArmNativeCloseIntent(111, 0x4100, 1), "foreign-close case arms independently");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA0004), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 0 &&
	          NativeIntentGateState().Phase() == IntentPhase::Cancelled,
	      "foreign or stale ownership proof cancels close without mutation");

	g_ownedSessionForAbi = true;
	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	manager[0x08] = 2;
	Check(ArmNativeCloseIntent(112, 0x4100, 1), "close cancellation case arms independently");
	CancelNativeCloseIntent();
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA0005), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 0 &&
	          NativeIntentGateState().Phase() == IntentPhase::Cancelled,
	      "pre-dispatch close cancellation cannot later close an owned menu");

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	manager[0x08] = 1;
	*reinterpret_cast<UInt32*>(manager.data() + 0xE0) = 0;
	g_abi.result = 0;
	g_holdUpdateScope = true;
	Check(ArmNativeOpenIntent(115), "no-op expiry case arms an open token");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA0007), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 1,
	      "no-op expiry case admits the query");
	const UInt32 staleInvocation = NativeIntentGateState().ActiveInvocationId();
	const UInt32 staleUpdate = NativeIntentActiveUpdateInvocation();
	g_holdUpdateScope = false;
	NativeIntentUpdateLeave();
	Check(NativeIntentGateState().Phase() == IntentPhase::Rejected &&
	          !NativeIntentGateState().ObserveMutation(0, kMenuIdBigFour,
	                                                   kOpenIntentMutationReturn,
	                                                   staleInvocation, true, staleUpdate, true),
	      "an admitted token expires at Update exit before a later matching mutation");

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	manager[0x08] = 2;
	g_abi.result = 0;
	g_holdUpdateScope = true;
	Check(ArmNativeCloseIntent(116, 0x4100, 1), "nested Update case arms an owned close token");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA0008), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 1,
	      "outer Update query admits before nested Update");
	const UInt32 nestedUpdateId = NativeIntentActiveUpdateInvocation();
	Check(NativeIntentUpdateEnter(), "nested Update enters without replacing outer context");
	g_holdUpdateScope = false;
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA0009), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 0 &&
	          NativeIntentGateState().Phase() == IntentPhase::Admitted,
	      "nested Update after the query returns passes through and cannot consume outer intent");
	NativeIntentUpdateLeave();
	g_holdUpdateScope = true;
	const UInt32 nestedOuterInvocation = NativeIntentGateState().ActiveInvocationId();
	g_ownedSessionForAbi = false;
	Check(!NativeIntentGateState().ObserveMutation(0, kMenuIdBigFour,
	                                               kCloseIntentMutationReturn,
	                                               nestedOuterInvocation, false, nestedUpdateId, false),
	      "ownership invalidation during nested Update rejects the outer mutation");
	g_ownedSessionForAbi = true;
	g_holdUpdateScope = false;
	NativeIntentUpdateLeave();

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	manager[0x08] = 1;
	*reinterpret_cast<UInt32*>(manager.data() + 0xE0) = 0;
	g_abi.result = 0;
	g_holdUpdateScope = true;
	Check(ArmNativeOpenIntent(117), "concurrent wrong-thread case arms an open token");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA000A), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 1,
	      "main Update admits before concurrent wrong-thread work");
	std::atomic<UInt32> concurrentResult{0};
	std::thread concurrent([&] {
		UInt32 a = 0, b = 0, c = 0, d = 0;
		bool s = false, f = false;
		concurrentResult.store(CallIntentWithCanaries(g_abi.stub,
		                                             reinterpret_cast<void*>(0xAAAA000B),
		                                             manager.data(), 0, &a, &b, &c, &d, &s, &f),
		                       std::memory_order_release);
	});
	concurrent.join();
	Check(concurrentResult.load(std::memory_order_acquire) == 0 &&
	          NativeIntentGateState().Phase() == IntentPhase::Admitted,
	      "wrong-thread activity passes through without touching main Update state");
	const UInt32 concurrentInvocation = NativeIntentGateState().ActiveInvocationId();
	Check(NativeIntentGateState().ObserveMutation(0, kMenuIdBigFour,
	                                               kOpenIntentMutationReturn,
	                                               concurrentInvocation, true,
	                                               NativeIntentActiveUpdateInvocation(), true),
	      "main Update still acknowledges its own successful mutation");
	g_holdUpdateScope = false;
	NativeIntentUpdateLeave();

	ConfigureNativeIntentProbe(GetCurrentThreadId(), 0);
	SetNativeIntentContextValid(true);
	manager[0x08] = 1;
	g_abi.result = 0;
	g_holdUpdateScope = true;
	Check(ArmNativeOpenIntent(118), "post-admission cancellation case arms a token");
	Check(CallIntentWithCanaries(g_abi.stub, reinterpret_cast<void*>(0xAAAA000C), manager.data(),
	                             0, &ebx, &esi, &edi, &edx, &stackOk, &carry) == 1,
	      "post-admission cancellation case reaches the engine query");
	CancelNativeOpenIntent();
	Check(NativeIntentGateState().Phase() == IntentPhase::Cancelled,
	      "post-admission cancellation clears the unresolved Update token");
	g_holdUpdateScope = false;
	NativeIntentUpdateLeave();

	SetNativeIntentOwnershipProbe(nullptr);
	NativeIntentGateState().Reset();
	VirtualFree(executable, 0, MEM_RELEASE);
#else
	Check(true, "Win32 x86 ABI harness is skipped on non-MSVC/non-x86 hosts");
#endif
}

}  // namespace

int main() {
	TestStackReads();
	TestLifecycle();
	TestRequests();
	TestMailbox();
	TestInstaller();
	TestUpdateEntryRelocation();
	TestMailboxContention();
	TestNativeIntentStub();
	std::printf("VR menu bridge: %d/%d checks passed\n", checks - failures, checks);
	return failures == 0 ? 0 : 1;
}
