#pragma once

#include "core/Types.h"

namespace obvr::game::vrbridge {

// The call-site patch is deliberately separate from the stack lifecycle
// hooks.  It wraps the engine's native query once and returns through the
// original call-site contract (two callee-cleaned arguments).
constexpr UInt32 kOpenIntentQueryTarget = 0x00403520;
constexpr UInt32 kOpenIntentQueryCallSite = 0x00583A9B;
constexpr UInt32 kOpenIntentQueryContinuation = 0x00583AA0;
constexpr UInt32 kOpenIntentMutationReturn = 0x00583B10;
constexpr UInt32 kOpenIntentPatchSize = 5;
constexpr UInt32 kCloseIntentQueryContinuation = 0x00583B25;
constexpr UInt32 kCloseIntentMutationReturn = 0x00583B64;
constexpr UInt32 kInterfaceManagerUpdate = 0x005821F0;
constexpr UInt32 kInterfaceManagerUpdatePatchSize = 6;
constexpr const char* kExpectedOblivionSha256 =
	"A8F313845C1545E9A60E1E995961EEF4C033115DA9443F6D756341DF3C2B7DC6";

// Both query sites are five-byte calls to the same thiscall query.  The
// continuations select the open and close branches respectively.
constexpr UInt32 kCloseIntentQueryCallSite = 0x00583B20;

enum class IntentResolution : UInt8 {
	PassThrough,
	PreserveNative,
	OverrideOpen,
	OverrideClose,
};

enum class IntentKind : UInt8 { Open, Close };

enum class IntentPhase : UInt8 {
	Idle,
	Pending,
	Admitted,
	Acknowledged,
	Rejected,
	Cancelled,
};

struct OpenIntentQuery {
	UInt32 originalResult = 0;
	UInt32 pageSelection = 0;
	UInt32 callReturnAddress = 0;
	UInt32 invocationDepth = 0;
	UInt32 updateDepth = 0;
	UInt32 updateInvocationId = 0;
	bool mainThread = false;
	bool contextValid = false;
	bool normalOpenState = false;
	bool normalCloseState = false;
	bool ownedSession = false;
};

struct IntentDecision {
	IntentResolution resolution = IntentResolution::PassThrough;
	UInt32 result = 0;
	UInt32 token = 0;
};

// Pure state machine for the one-shot open intent.  The engine's native/page
// request always wins.  A nested or wrong-origin call passes through without
// consuming the token; context loss and a known refusal consume it safely.
class NativeIntentGate {
public:
	bool Arm(UInt32 token, IntentKind kind = IntentKind::Open,
	         UInt32 expectedRoot = 0, UInt32 expectedGeneration = 0);
	IntentDecision Resolve(const OpenIntentQuery& query);

	// Called after the original stack mutation, with the return address of the
	// mutation caller and a post-mutation anchor observation.  Only the normal
	// personal-open callsite can acknowledge this gate.
	bool ObserveMutation(SInt32 result, UInt32 menuId, UInt32 mutationReturnAddress,
	                     UInt32 invocationId, bool anchorPresent,
                     UInt32 updateInvocationId, bool ownershipValid);

	// A foreign/modal insertion while the open scope is admitted invalidates
	// attribution.  It never closes that menu automatically.
	void RejectForeignMutation();
	void BeginUpdate(UInt32 updateInvocationId);
	void EndUpdate();
	void Cancel();
	void Reset();

	IntentPhase Phase() const { return phase_; }
	UInt32 Token() const { return token_; }
	UInt32 ActiveInvocationId() const { return activeInvocationId_; }
	UInt32 LastAcknowledgedToken() const { return lastAcknowledgedToken_; }
	UInt32 ExpectedRoot() const { return expectedRoot_; }
	UInt32 ExpectedGeneration() const { return expectedGeneration_; }
	IntentKind Kind() const { return kind_; }

private:
	IntentPhase phase_ = IntentPhase::Idle;
	UInt32 token_ = 0;
	UInt32 nextInvocationId_ = 0;
	UInt32 activeInvocationId_ = 0;
	UInt32 lastAcknowledgedToken_ = 0;
	UInt32 activeUpdateInvocationId_ = 0;
	UInt32 expectedRoot_ = 0;
	UInt32 expectedGeneration_ = 0;
	IntentKind kind_ = IntentKind::Open;
};

// Frame written by pushfd/pushad in the generated stub.  pushad leaves the
// last-pushed registers at the lowest addresses: savedEsp points at the flags
// word; savedEsp[1] is the call-site return address and [2]/[3] are the
// original first/second arguments.
struct NativeIntentSavedFrame {
	UInt32 savedEdi;
	UInt32 savedEsi;
	UInt32 savedEbp;
	UInt32 savedEsp;
	UInt32 savedEbx;
	UInt32 savedEdx;
	UInt32 savedEcx;
	UInt32 savedEax;
	UInt32 savedFlags;
};
static_assert(sizeof(NativeIntentSavedFrame) == 36, "pushad frame layout");

struct NativeIntentStubSpec {
	UInt32 stubAddress = 0;
	UInt32 originalTarget = 0;
	UInt32 enterTarget = 0;
	UInt32 resolveTarget = 0;
	UInt32 leaveTarget = 0;
};

// Generates a per-invocation stack frame:
// pushfd/pushad, enter, original call, resolve, leave, popad/popfd, ret 8.
// No static return-address or argument slots are used, so nested calls have
// independent frames.  Returns zero on capacity overflow.
UInt32 BuildNativeIntentStub(UInt8* buffer, UInt32 capacity,
                             const NativeIntentStubSpec& spec);

// Runtime callbacks used by the generated machine code.  They are cdecl and
// intentionally small so the generated stub remains testable without the
// engine.  The resolver returns the final EAX value for the original caller.
extern "C" UInt32 __cdecl NativeIntentStubEnter();
extern "C" UInt32 __cdecl NativeIntentStubResolve(NativeIntentSavedFrame* frame,
                                                   UInt32 invocationDepth);
extern "C" void __cdecl NativeIntentStubLeave();

// The probe is not installed by the default plugin path.  These functions are
// the explicit adapter seam for a later opt-in runtime probe.
NativeIntentGate& NativeIntentGateState();
void ConfigureNativeIntentProbe(UInt32 gameThreadId, UInt32 expectedContinuation,
	                            UInt32 expectedCloseContinuation = 0);
void SetNativeIntentContextValid(bool valid);
// The lifecycle bridge supplies this only after its verified stack hooks are
// installed.  The immutable expected identity belongs to the open lease that
// produced the close request; a null probe fails closed.
using NativeIntentOwnershipProbe = bool (*)(UInt32 manager, UInt32 expectedRoot,
	                                           UInt32 expectedGeneration);
void SetNativeIntentOwnershipProbe(NativeIntentOwnershipProbe probe);
bool ArmNativeOpenIntent(UInt32 token);
bool ArmNativeCloseIntent(UInt32 token, UInt32 expectedRoot, UInt32 expectedGeneration);
bool ArmNativeRequest(UInt32 token, UInt32 operation, UInt32 expectedRoot,
	                  UInt32 expectedGeneration);
void CancelNativeOpenIntent();
void CancelNativeCloseIntent();
UInt32 NativeIntentInvocationDepth();
UInt32 NativeIntentUpdateDepth();
UInt32 NativeIntentActiveUpdateInvocation();
bool NativeIntentUpdateEnter();
void NativeIntentUpdateLeave();
using NativeIntentUpdateBody = void (*)(void* context);
void RunNativeIntentUpdate(NativeIntentUpdateBody body, void* context);
bool IsNativeIntentProbeInstalled();
bool InstallNativeIntentProbe();
bool ReadNativeIntentExecutableHash(char* output, UInt32 outputSize);
bool NativeIntentHashMatches(const char* observed);

enum class NativeIntentInstallCheck : UInt8 {
	Ready,
	AlreadyInstalled,
	HashReadFailed,
	HashMismatch,
	OpenSignatureMismatch,
	CloseSignatureMismatch,
};

constexpr NativeIntentInstallCheck EvaluateNativeIntentInstall(
	bool installed, bool hashRead, bool hashMatches, bool openSignature,
	bool closeSignature) {
	if (installed) {
		return NativeIntentInstallCheck::AlreadyInstalled;
	}
	if (!hashRead) {
		return NativeIntentInstallCheck::HashReadFailed;
	}
	if (!hashMatches) {
		return NativeIntentInstallCheck::HashMismatch;
	}
	if (!openSignature) {
		return NativeIntentInstallCheck::OpenSignatureMismatch;
	}
	if (!closeSignature) {
		return NativeIntentInstallCheck::CloseSignatureMismatch;
	}
	return NativeIntentInstallCheck::Ready;
}

}  // namespace obvr::game::vrbridge
