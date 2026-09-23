#pragma once

#include <atomic>

#include "core/Types.h"
#include "game/MenuType.h"

namespace obvr::game::vrbridge {

// Oblivion represents the four personal Tab pages by one stack entry.  The
// entry is an anchor for the session; the visible page and any child modal
// may change while that anchor remains in the active stack.
constexpr UInt32 kPersonalStackId = kMenuIdBigFour;
constexpr UInt32 kMenuStackSlots = 10;

struct SessionIdentity {
	// InterfaceManager's shared UI root (+0x68), paired with a generation.
	// This is not a personal-menu tile root; the BigFour stack anchor is the
	// ownership evidence and the generation distinguishes reuse.
	UInt32 interfaceRoot = 0;
	UInt32 generation = 0;

	friend constexpr bool operator==(SessionIdentity a, SessionIdentity b) {
		return a.interfaceRoot == b.interfaceRoot && a.generation == b.generation;
	}
	friend constexpr bool operator!=(SessionIdentity a, SessionIdentity b) {
		return !(a == b);
	}
};

constexpr bool Valid(SessionIdentity identity) {
	return identity.interfaceRoot != 0 && identity.generation != 0;
}

// This is the data read after a native stack mutation.  It deliberately has
// no ActiveMenuId or GenericRoot field: those are cursor/array observations,
// not the interface manager's visible stack or its actual root.
struct StackSnapshot {
	bool available = false;
	bool focused = false;
	bool gameplay = false;
	UInt32 interfaceRoot = 0;  // InterfaceManager +0x68.
	UInt32 entries[kMenuStackSlots]{};  // InterfaceManager +0xE0..+0x104.
	UInt32 topVisible = kMenuIdNone;
};

constexpr bool ContainsPersonal(const StackSnapshot& snapshot) {
	for (UInt32 i = 0; i < kMenuStackSlots; ++i) {
		if (snapshot.entries[i] == kPersonalStackId) {
			return true;
		}
	}
	return false;
}

constexpr bool Empty(const StackSnapshot& snapshot) {
	for (UInt32 i = 0; i < kMenuStackSlots; ++i) {
		if (snapshot.entries[i] != kMenuIdNone) {
			return false;
		}
	}
	return true;
}

// Mirrors GetTopVisibleMenuID's ten contiguous stack reads.  A gap terminates
// the engine's scan; the hook uses this pure form after the original function
// returns so a refusal/no-op can never be mistaken for a mutation.
constexpr UInt32 TopVisible(const UInt32 (&entries)[kMenuStackSlots]) {
	UInt32 top = kMenuIdNone;
	for (UInt32 i = 0; i < kMenuStackSlots; ++i) {
		if (entries[i] == kMenuIdNone) {
			break;
		}
		top = entries[i];
	}
	return top;
}

constexpr bool EligibleForOpen(const StackSnapshot& snapshot) {
	// A request must start from loaded, focused gameplay with an empty native
	// stack.  This keeps a stale/foreign menu from being attributed to Tab.
	return snapshot.available && snapshot.focused && snapshot.gameplay &&
	       snapshot.interfaceRoot == 0 && Empty(snapshot);
}

constexpr bool EligibleForClose(const StackSnapshot& snapshot, SessionIdentity current,
	                             SessionIdentity expected) {
	// Close is a lifecycle operation.  It intentionally does not inspect
	// interaction, cursor geometry, or the top child: the personal anchor is
	// enough to identify the owned session, and child pages/modal entries are
	// part of that session while the anchor remains present.
	return snapshot.available && snapshot.focused && Valid(current) && current == expected &&
	       snapshot.interfaceRoot == current.interfaceRoot && ContainsPersonal(snapshot);
}

enum class Operation : UInt8 { None, OpenPersonalMenu, CloseOwnedMenu };

struct Request {
	UInt32 token = 0;
	Operation operation = Operation::None;
	SessionIdentity expected{};
};

struct SessionLease {
	UInt32 openToken = 0;
	SessionIdentity identity{};

	constexpr bool Valid() const {
		return openToken != 0 && obvr::game::vrbridge::Valid(identity);
	}
};

enum class DispatchPhase : UInt8 {
	None,
	Queued,
	Dispatched,
	CancelledAfterDispatch,
	AmbiguousObservation,
	ForeignObservation,
};

enum class CancelResult : UInt8 {
	NothingPending,
	CancelledBeforeDispatch,
	CannotRevokeDispatchedInput,
};

enum class LifecycleEvent : UInt8 { None, Opened, Closed, Replaced, Unavailable };

struct Observation {
	LifecycleEvent event = LifecycleEvent::None;
	SessionIdentity identity{};
	SessionIdentity previous{};
	UInt32 topVisible = kMenuIdNone;
	bool foreignStack = false;
};

// Main-thread state machine.  It consumes snapshots only after the original
// engine stack function has returned.  A generation changes only on an
// observed anchor appearance/disappearance or root replacement, so a reused
// root pointer cannot satisfy an older close request.
class Lifecycle {
public:
	Observation Observe(const StackSnapshot& snapshot) {
		Observation result{};
		result.topVisible = snapshot.topVisible;
		if (!snapshot.available) {
			result.event = LifecycleEvent::Unavailable;
			return result;
		}

		const bool present = ContainsPersonal(snapshot);
		if (!known_) {
			known_ = true;
			if (present) {
				Open(snapshot, result);
			} else {
				result.foreignStack = !Empty(snapshot);
			}
			return result;
		}

		if (!present) {
			if (present_) {
				result.event = LifecycleEvent::Closed;
				result.previous = identity_;
				present_ = false;
				identity_ = {};
			}
			result.foreignStack = !Empty(snapshot);
			return result;
		}

		if (!present_) {
			Open(snapshot, result);
			return result;
		}

		if (identity_.interfaceRoot != snapshot.interfaceRoot) {
			result.event = LifecycleEvent::Replaced;
			result.previous = identity_;
			NextGeneration();
			identity_.generation = generation_;
			identity_.interfaceRoot = snapshot.interfaceRoot;
			result.identity = identity_;
			return result;
		}

		result.identity = identity_;
		return result;
	}

	bool Known() const { return known_; }
	bool Present() const { return present_; }
	SessionIdentity Current() const { return identity_; }

private:
	void NextGeneration() {
		++generation_;
		if (generation_ == 0) {
			++generation_;
		}
	}

	void Open(const StackSnapshot& snapshot, Observation& result) {
		NextGeneration();
		present_ = true;
		identity_ = {snapshot.interfaceRoot, generation_};
		result.event = LifecycleEvent::Opened;
		result.identity = identity_;
	}

	bool known_ = false;
	bool present_ = false;
	UInt32 generation_ = 0;
	SessionIdentity identity_{};
};

constexpr bool MatchesCloseAcknowledgement(const Request& request,
	                                           const Observation& observation) {
	return request.token != 0 && request.operation == Operation::CloseOwnedMenu &&
	       observation.event == LifecycleEvent::Closed &&
	       observation.previous == request.expected;
}

constexpr bool MatchesSynchronousOpenAcknowledgement(const Request& request,
	                                                    const Observation& observation,
	                                                    bool synchronousEngineProof) {
	// Queued TapKey has no revocation or causal tag.  An observed personal
	// insertion is therefore not an acknowledgement unless the adapter proves
	// a synchronous main-thread engine operation caused it.
	return synchronousEngineProof && request.token != 0 &&
	       request.operation == Operation::OpenPersonalMenu &&
	       observation.event == LifecycleEvent::Opened && Valid(observation.identity);
}

// A one-slot mailbox makes ownership explicit: producer threads may publish a
// request, while only the game thread consumes and dispatches it.  Lifecycle
// and engine calls stay on the game thread.  No native input call is made by
// this mailbox, and Cancel cannot revoke an already dispatched TapKey.
class RequestMailbox {
public:
	bool Publish(const Request& request) {
		Lock();
		const bool accepted = !occupied_ && request.token != 0 &&
		                      request.operation != Operation::None;
		if (accepted) {
			request_ = request;
			occupied_ = true;
		}
		Unlock();
		return accepted;
	}

	bool Consume(Request& request) {
		Lock();
		const bool available = occupied_;
		if (available) {
			request = request_;
			occupied_ = false;
		}
		Unlock();
		return available;
	}

	bool Cancel(Request& request) {
		Lock();
		const bool available = occupied_;
		if (available) {
			request = request_;
			occupied_ = false;
		}
		Unlock();
		return available;
	}

	bool Pending() const {
		Lock();
		const bool pending = occupied_;
		Unlock();
		return pending;
	}

private:
	void Lock() const {
		while (lock_.test_and_set(std::memory_order_acquire)) {
		}
	}
	void Unlock() const { lock_.clear(std::memory_order_release); }

	mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
	Request request_{};
	bool occupied_ = false;
};

// Requests are deliberately a second state machine from Lifecycle.  A
// command becoming dispatchable is not an acknowledgement.  In particular,
// the xOBSE TapKey path is asynchronous and ReleaseKey is not documented as
// removing its already queued tap, so a late personal insertion remains
// ambiguous and is never closed as if it were ours.
class RequestController {
public:
	Observation Observe(const StackSnapshot& snapshot, UInt32 causalToken = 0) {
		const Observation observation = lifecycle_.Observe(snapshot);
		lastObservation_ = observation;
		if (observation.event == LifecycleEvent::Closed ||
		    observation.event == LifecycleEvent::Replaced) {
			lease_ = {};
		}
		if (phase_ == DispatchPhase::Queued &&
		    (observation.event == LifecycleEvent::Opened || observation.foreignStack)) {
			phase_ = DispatchPhase::ForeignObservation;
			active_ = {};
		} else if (phase_ == DispatchPhase::Dispatched &&
		           active_.operation == Operation::OpenPersonalMenu &&
		           observation.event == LifecycleEvent::Opened) {
			if (causalToken != 0 && causalToken == active_.token &&
			    MatchesSynchronousOpenAcknowledgement(active_, observation, true)) {
				lease_ = {active_.token, observation.identity};
				phase_ = DispatchPhase::None;
				active_ = {};
			} else {
				phase_ = DispatchPhase::AmbiguousObservation;
			}
		} else if ((phase_ == DispatchPhase::Dispatched ||
		            phase_ == DispatchPhase::CancelledAfterDispatch) &&
		           observation.event == LifecycleEvent::Opened) {
			phase_ = DispatchPhase::AmbiguousObservation;
		} else if ((phase_ == DispatchPhase::Dispatched ||
		            phase_ == DispatchPhase::CancelledAfterDispatch) &&
		           active_.operation == Operation::OpenPersonalMenu &&
		           observation.foreignStack) {
			phase_ = DispatchPhase::ForeignObservation;
			active_ = {};
		} else if (phase_ == DispatchPhase::Dispatched &&
		           active_.operation == Operation::CloseOwnedMenu) {
			if (MatchesCloseAcknowledgement(active_, observation)) {
				phase_ = DispatchPhase::None;
				active_ = {};
			} else if (observation.event == LifecycleEvent::Replaced ||
			           observation.foreignStack) {
				phase_ = DispatchPhase::ForeignObservation;
				active_ = {};
			}
		}
		return observation;
	}

	bool RequestOpen(const StackSnapshot& snapshot, Request& request) {
		if (phase_ != DispatchPhase::None || !EligibleForOpen(snapshot)) {
			return false;
		}
		active_ = {NextToken(), Operation::OpenPersonalMenu, {}};
		phase_ = DispatchPhase::Queued;
		request = active_;
		return true;
	}

	bool RequestClose(const StackSnapshot& snapshot, Request& request) {
		const SessionIdentity current = lifecycle_.Current();
		if (phase_ != DispatchPhase::None ||
		    !lease_.Valid() || lease_.identity != current ||
		    !EligibleForClose(snapshot, current, lease_.identity)) {
			return false;
		}
		active_ = {NextToken(), Operation::CloseOwnedMenu, lease_.identity};
		phase_ = DispatchPhase::Queued;
		request = active_;
		return true;
	}

	// This check is repeated on the game thread immediately before the adapter
	// calls native code.  A producer cannot hold an old open permission across
	// a foreign menu insertion or focus loss.
	bool Dispatch(const StackSnapshot& snapshot, Request& request) {
		if (phase_ != DispatchPhase::Queued || active_.token == 0) {
			return false;
		}
		const bool allowed = active_.operation == Operation::OpenPersonalMenu
		                         ? EligibleForOpen(snapshot)
		                         : EligibleForClose(snapshot, lifecycle_.Current(),
		                                            active_.expected);
		if (!allowed) {
			phase_ = DispatchPhase::ForeignObservation;
			active_ = {};
			return false;
		}
		phase_ = DispatchPhase::Dispatched;
		request = active_;
		return true;
	}

	CancelResult CancelPending() {
		if (phase_ == DispatchPhase::Queued) {
			phase_ = DispatchPhase::None;
			active_ = {};
			return CancelResult::CancelledBeforeDispatch;
		}
		if (phase_ == DispatchPhase::Dispatched) {
			phase_ = DispatchPhase::CancelledAfterDispatch;
			return CancelResult::CannotRevokeDispatchedInput;
		}
		return CancelResult::NothingPending;
	}

	// A timeout follows the same rule as explicit cancellation: before native
	// dispatch it drops the request; after dispatch it cannot revoke queued
	// input and must wait for an observed, attributable lifecycle event.
	CancelResult ExpirePending() { return CancelPending(); }

	// Drops a command after the adapter reports a native guard refusal, no-op,
	// or an unconsumed Update exit.  It never clears the immutable open lease.
	void ClearActiveFailure() {
		active_ = {};
		phase_ = DispatchPhase::None;
	}

	DispatchPhase Phase() const { return phase_; }
	Request PendingRequest() const { return active_; }
	SessionIdentity Current() const { return lifecycle_.Current(); }
	SessionLease Lease() const { return lease_; }
	Observation LastObservation() const { return lastObservation_; }

private:
	UInt32 NextToken() {
		++nextToken_;
		if (nextToken_ == 0) {
			++nextToken_;
		}
		return nextToken_;
	}

	Lifecycle lifecycle_{};
	Request active_{};
	SessionLease lease_{};
	Observation lastObservation_{};
	DispatchPhase phase_ = DispatchPhase::None;
	UInt32 nextToken_ = 0;
};

enum class HookSite : UInt8 { Push, Remove };

enum class HookInstallResult : UInt8 {
	Installed,
	AlreadyInstalled,
	AlreadyAttempted,
	InvalidPort,
	UnsafePartialState,
	PushSignatureMismatch,
	RemoveSignatureMismatch,
	PushPrepareFailed,
	RemovePrepareFailed,
	PushWriteFailed,
	RemoveWriteRolledBack,
	UnsafeRollbackFailure,
};

enum class UpdateHookInstallCheck : UInt8 {
	Ready,
	AlreadyInstalled,
	HashReadFailed,
	HashMismatch,
	SignatureMismatch,
	PrepareFailed,
	WriteFailed,
	RollbackFailed,
};

constexpr UpdateHookInstallCheck EvaluateUpdateHookInstall(
	bool installed, bool hashRead, bool hashMatches, bool signature,
	bool prepared, bool writeSucceeded, bool rollbackSucceeded) {
	if (installed) {
		return UpdateHookInstallCheck::AlreadyInstalled;
	}
	if (!hashRead) {
		return UpdateHookInstallCheck::HashReadFailed;
	}
	if (!hashMatches) {
		return UpdateHookInstallCheck::HashMismatch;
	}
	if (!signature) {
		return UpdateHookInstallCheck::SignatureMismatch;
	}
	if (!prepared) {
		return UpdateHookInstallCheck::PrepareFailed;
	}
	if (!writeSucceeded) {
		return rollbackSucceeded ? UpdateHookInstallCheck::WriteFailed
		                         : UpdateHookInstallCheck::RollbackFailed;
	}
	return UpdateHookInstallCheck::Ready;
}

struct HookInstallState {
	bool installTried = false;
	bool pushPatched = false;
	bool removePatched = false;
	bool resourcesRetained = false;
	bool unsafe = false;
};

// The installer decision is separated from addresses and VirtualProtect so
// every refusal and rollback path can be tested without patching a process.
// Callbacks run synchronously on the installing thread; `prepare` returns the
// original trampoline handle, and `write(..., true)` restores that site's
// exact original bytes.
using VerifyHookSite = bool (*)(HookSite site, void* context);
using PrepareHookSite = bool (*)(HookSite site, void** original, void* context);
using WriteHookSite = bool (*)(HookSite site, bool restore, void* context);

struct HookInstallOperations {
	VerifyHookSite verify = nullptr;
	PrepareHookSite prepare = nullptr;
	WriteHookSite write = nullptr;
	void* context = nullptr;
};

inline HookInstallResult InstallHookPair(const HookInstallOperations& operations,
	                                       HookInstallState& state) {
	if (operations.verify == nullptr || operations.prepare == nullptr ||
	    operations.write == nullptr) {
		return HookInstallResult::InvalidPort;
	}
	if (state.unsafe || state.pushPatched != state.removePatched) {
		state.unsafe = true;
		return HookInstallResult::UnsafePartialState;
	}
	if (state.pushPatched && state.removePatched) {
		return HookInstallResult::AlreadyInstalled;
	}
	if (state.installTried) {
		return HookInstallResult::AlreadyAttempted;
	}
	state.installTried = true;

	if (!operations.verify(HookSite::Push, operations.context)) {
		return HookInstallResult::PushSignatureMismatch;
	}
	if (!operations.verify(HookSite::Remove, operations.context)) {
		return HookInstallResult::RemoveSignatureMismatch;
	}

	void* pushOriginal = nullptr;
	if (!operations.prepare(HookSite::Push, &pushOriginal, operations.context)) {
		return HookInstallResult::PushPrepareFailed;
	}
	state.resourcesRetained = true;
	void* removeOriginal = nullptr;
	if (!operations.prepare(HookSite::Remove, &removeOriginal, operations.context)) {
		return HookInstallResult::RemovePrepareFailed;
	}

	if (!operations.write(HookSite::Push, false, operations.context)) {
		return HookInstallResult::PushWriteFailed;
	}
	state.pushPatched = true;
	if (!operations.write(HookSite::Remove, false, operations.context)) {
		if (operations.write(HookSite::Push, true, operations.context)) {
			state.pushPatched = false;
			return HookInstallResult::RemoveWriteRolledBack;
		}
		// The push patch and its trampoline must remain owned if restoration
		// fails; a second installer must never overwrite a live handler.
		state.unsafe = true;
		return HookInstallResult::UnsafeRollbackFailure;
	}
	state.removePatched = true;
	return HookInstallResult::Installed;
}

// Installs guarded entry detours around InterfaceManager stack mutation.  The
// hooks only observe post-mutation state and preserve every original return
// value.  They are intentionally separate from any Tab/laser/render adapter.
bool InstallVRMenuBridge();
bool IsVRMenuBridgeInstalled();
// Installs the complete guarded runtime diagnostic seam: stack/update hooks,
// both verified query call sites, and the explicit F9/F10 diagnostic adapter.
// It is never called by the default plugin path unless the Debug probe is on.
bool InstallNativeMenuLifecycleProbe();
bool IsNativeMenuLifecycleProbeEnabled();
bool IsOwnedPersonalSession(UInt32 manager, UInt32 expectedRoot,
	                        UInt32 expectedGeneration);
bool ArmNativeRequest(const Request& request);

}  // namespace obvr::game::vrbridge
