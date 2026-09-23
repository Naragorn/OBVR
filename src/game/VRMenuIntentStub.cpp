#include "game/VRMenuIntentStub.h"

#include "core/CodeWriter.h"
#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/MenuType.h"
#include "platform/Win32Min.h"

namespace obvr::game::vrbridge {
namespace {

NativeIntentGate g_gate{};
UInt32 g_gameThreadId = 0;
UInt32 g_expectedContinuation = kOpenIntentQueryContinuation;
UInt32 g_expectedCloseContinuation = kCloseIntentQueryContinuation;
bool g_contextValid = false;
UInt8* g_stub = nullptr;
UInt8* g_closeStub = nullptr;
bool g_probeInstalled = false;
bool g_probeUnsafe = false;
NativeIntentOwnershipProbe g_ownershipProbe = nullptr;

#if defined(_MSC_VER)
__declspec(thread) UInt32 g_updateDepth = 0;
__declspec(thread) UInt32 g_updateInvocationId = 0;
__declspec(thread) UInt32 g_nextUpdateInvocationId = 0;
#else
thread_local UInt32 g_updateDepth = 0;
thread_local UInt32 g_updateInvocationId = 0;
thread_local UInt32 g_nextUpdateInvocationId = 0;
#endif

#if defined(_MSC_VER)
__declspec(thread) UInt32 g_invocationDepth = 0;
#else
thread_local UInt32 g_invocationDepth = 0;
#endif

constexpr UInt8 kOpenQueryBytes[kOpenIntentPatchSize] = {
	0xE8, 0x80, 0xFA, 0xE7, 0xFF,
};
constexpr UInt8 kCloseQueryBytes[kOpenIntentPatchSize] = {
	0xE8, 0xFB, 0xF9, 0xE7, 0xFF,
};

constexpr UInt32 kSha256RoundConstants[64] = {
	0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B,
	0x59F111F1, 0x923F82A4, 0xAB1C5ED5, 0xD807AA98, 0x12835B01,
	0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7,
	0xC19BF174, 0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
	0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA, 0x983E5152,
	0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147,
	0x06CA6351, 0x14292967, 0x27B70A85, 0x2E1B2138, 0x4D2C6DFC,
	0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
	0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819,
	0xD6990624, 0xF40E3585, 0x106AA070, 0x19A4C116, 0x1E376C08,
	0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F,
	0x682E6FF3, 0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
	0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

UInt32 RotateRight(UInt32 value, UInt32 count) {
	return (value >> count) | (value << (32 - count));
}

struct Sha256State {
	UInt32 words[8] = {0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
	                   0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19};
	UInt8 block[64]{};
	UInt32 used = 0;
	UInt64 total = 0;
};

void Sha256Block(Sha256State& state, const UInt8* input) {
	UInt32 schedule[64]{};
	for (UInt32 i = 0; i < 16; ++i) {
		schedule[i] = (static_cast<UInt32>(input[i * 4]) << 24) |
		              (static_cast<UInt32>(input[i * 4 + 1]) << 16) |
		              (static_cast<UInt32>(input[i * 4 + 2]) << 8) |
		              static_cast<UInt32>(input[i * 4 + 3]);
	}
	for (UInt32 i = 16; i < 64; ++i) {
		const UInt32 s0 = RotateRight(schedule[i - 15], 7) ^
		                  RotateRight(schedule[i - 15], 18) ^ (schedule[i - 15] >> 3);
		const UInt32 s1 = RotateRight(schedule[i - 2], 17) ^
		                  RotateRight(schedule[i - 2], 19) ^ (schedule[i - 2] >> 10);
		schedule[i] = schedule[i - 16] + s0 + schedule[i - 7] + s1;
	}
	UInt32 a = state.words[0], b = state.words[1], c = state.words[2], d = state.words[3];
	UInt32 e = state.words[4], f = state.words[5], g = state.words[6], h = state.words[7];
	for (UInt32 i = 0; i < 64; ++i) {
		const UInt32 s1 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
		const UInt32 choose = (e & f) ^ ((~e) & g);
		const UInt32 temp1 = h + s1 + choose + kSha256RoundConstants[i] + schedule[i];
		const UInt32 s0 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
		const UInt32 majority = (a & b) ^ (a & c) ^ (b & c);
		const UInt32 temp2 = s0 + majority;
		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}
	state.words[0] += a;
	state.words[1] += b;
	state.words[2] += c;
	state.words[3] += d;
	state.words[4] += e;
	state.words[5] += f;
	state.words[6] += g;
	state.words[7] += h;
}

void Sha256Update(Sha256State& state, const UInt8* input, UInt32 size) {
	state.total += size;
	while (size != 0) {
		const UInt32 copy = (size < 64 - state.used) ? size : 64 - state.used;
		for (UInt32 i = 0; i < copy; ++i) {
			state.block[state.used + i] = input[i];
		}
		state.used += copy;
		input += copy;
		size -= copy;
		if (state.used == 64) {
			Sha256Block(state, state.block);
			state.used = 0;
		}
	}
}

void Sha256Final(Sha256State& state, UInt8 output[32]) {
	const UInt64 bitLength = state.total * 8;
	state.block[state.used++] = 0x80;
	if (state.used > 56) {
		while (state.used < 64) {
			state.block[state.used++] = 0;
		}
		Sha256Block(state, state.block);
		state.used = 0;
	}
	while (state.used < 56) {
		state.block[state.used++] = 0;
	}
	for (UInt32 i = 0; i < 8; ++i) {
		state.block[56 + i] = static_cast<UInt8>(bitLength >> (56 - i * 8));
	}
	Sha256Block(state, state.block);
	for (UInt32 i = 0; i < 8; ++i) {
		output[i * 4] = static_cast<UInt8>(state.words[i] >> 24);
		output[i * 4 + 1] = static_cast<UInt8>(state.words[i] >> 16);
		output[i * 4 + 2] = static_cast<UInt8>(state.words[i] >> 8);
		output[i * 4 + 3] = static_cast<UInt8>(state.words[i]);
	}
}

char HexDigit(UInt8 value) { return value < 10 ? static_cast<char>('0' + value)
	                                             : static_cast<char>('A' + value - 10); }

bool ReadExecutableHash(char* output, UInt32 outputSize) {
	if (output == nullptr || outputSize < 65) {
		return false;
	}
	char path[512]{};
	const DWORD pathLength = GetModuleFileNameA(nullptr, path, sizeof(path));
	if (pathLength == 0 || pathLength >= sizeof(path)) {
		return false;
	}
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
	                          FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == InvalidHandle()) {
		return false;
	}
	Sha256State state{};
	UInt8 buffer[4096]{};
	bool success = true;
	for (;;) {
		DWORD read = 0;
		if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) {
			success = false;
			break;
		}
		if (read == 0) {
			break;
		}
		Sha256Update(state, buffer, read);
	}
	CloseHandle(file);
	if (!success) {
		return false;
	}
	UInt8 digest[32]{};
	Sha256Final(state, digest);
	for (UInt32 i = 0; i < 32; ++i) {
		output[i * 2] = HexDigit(static_cast<UInt8>(digest[i] >> 4));
		output[i * 2 + 1] = HexDigit(static_cast<UInt8>(digest[i] & 0x0F));
	}
	output[64] = '\0';
	return true;
}

bool SameText(const char* left, const char* right) {
	if (left == nullptr || right == nullptr) {
		return false;
	}
	for (;;) {
		if (*left != *right) {
			return false;
		}
		if (*left == '\0') {
			return true;
		}
		++left;
		++right;
	}
}

void FreeStub(UInt8* stub) {
	if (stub != nullptr) {
		VirtualFree(stub, 0, MEM_RELEASE);
	}
}

bool ReadOpenState(UInt32 manager) {
	if (manager == 0) {
		return false;
	}
	const auto* bytes = reinterpret_cast<const UInt8*>(manager);
	return bytes[0x08] == 1 && *reinterpret_cast<const UInt32*>(bytes + 0xE0) == 0;
}

bool ReadCloseState(UInt32 manager) {
	if (manager == 0) {
		return false;
	}
	const auto* bytes = reinterpret_cast<const UInt8*>(manager);
	return bytes[0x08] == 2;
}

UInt32 CallerReturn(const NativeIntentSavedFrame& frame) {
	if (frame.savedEsp == 0) {
		return 0;
	}
	return reinterpret_cast<const UInt32*>(frame.savedEsp)[1];
}

}  // namespace

bool ReadNativeIntentExecutableHash(char* output, UInt32 outputSize) {
	return ReadExecutableHash(output, outputSize);
}

bool NativeIntentHashMatches(const char* observed) {
	return SameText(observed, kExpectedOblivionSha256);
}

bool NativeIntentGate::Arm(UInt32 token, IntentKind kind, UInt32 expectedRoot,
                           UInt32 expectedGeneration) {
	if (token == 0 || phase_ == IntentPhase::Pending || phase_ == IntentPhase::Admitted) {
		return false;
	}
	if (kind == IntentKind::Close && (expectedRoot == 0 || expectedGeneration == 0)) {
		return false;
	}
	phase_ = IntentPhase::Pending;
	token_ = token;
	activeInvocationId_ = 0;
	kind_ = kind;
	expectedRoot_ = expectedRoot;
	expectedGeneration_ = expectedGeneration;
	return true;
}

IntentDecision NativeIntentGate::Resolve(const OpenIntentQuery& query) {
	IntentDecision decision{IntentResolution::PassThrough, query.originalResult, token_};
	if (phase_ != IntentPhase::Pending) {
		return decision;
	}
	const UInt32 expectedContinuation = kind_ == IntentKind::Open
	                                       ? kOpenIntentQueryContinuation
	                                       : kCloseIntentQueryContinuation;
	if (!query.mainThread || query.invocationDepth != 1 || query.updateDepth != 1 ||
	    query.updateInvocationId == 0 || query.updateInvocationId != activeUpdateInvocationId_ ||
	    query.callReturnAddress != expectedContinuation) {
		return decision;
	}
	const bool validOpen = query.contextValid && query.originalResult == 0 &&
	                       query.pageSelection == 0 && query.normalOpenState;
	const bool validClose = query.contextValid && query.originalResult == 0 &&
                        query.pageSelection == 0 && query.normalCloseState &&
	                        query.ownedSession;
	if (kind_ == IntentKind::Open ? !validOpen : !validClose) {
		phase_ = query.originalResult != 0 || query.pageSelection != 0
		             ? IntentPhase::Rejected
		             : IntentPhase::Cancelled;
		token_ = 0;
		activeInvocationId_ = 0;
		expectedRoot_ = 0;
		expectedGeneration_ = 0;
		decision.resolution = query.originalResult != 0
		                         ? IntentResolution::PreserveNative
		                         : IntentResolution::PassThrough;
		decision.token = 0;
		return decision;
	}

	++nextInvocationId_;
	if (nextInvocationId_ == 0) {
		++nextInvocationId_;
	}
	activeInvocationId_ = nextInvocationId_;
	phase_ = IntentPhase::Admitted;
	decision.resolution = kind_ == IntentKind::Open ? IntentResolution::OverrideOpen
	                                                : IntentResolution::OverrideClose;
	decision.result = 1;
	decision.token = token_;
	return decision;
}

bool NativeIntentGate::ObserveMutation(SInt32 result, UInt32 menuId,
                                       UInt32 mutationReturnAddress, UInt32 invocationId,
                                       bool anchorPresent,
                                       UInt32 updateInvocationId, bool ownershipValid) {
	if (phase_ != IntentPhase::Admitted) {
		return false;
	}
	const bool anchorMatches = kind_ == IntentKind::Open ? anchorPresent : !anchorPresent;
	const UInt32 expectedReturn = kind_ == IntentKind::Open
	                                 ? kOpenIntentMutationReturn
	                                 : kCloseIntentMutationReturn;
	if (result < 0 || menuId != kMenuIdBigFour ||
	    mutationReturnAddress != expectedReturn ||
	    invocationId != activeInvocationId_ ||
	    updateInvocationId != activeUpdateInvocationId_ || !anchorMatches ||
	    (kind_ == IntentKind::Close && !ownershipValid)) {
		phase_ = IntentPhase::Rejected;
		token_ = 0;
		activeInvocationId_ = 0;
		expectedRoot_ = 0;
		expectedGeneration_ = 0;
		return false;
	}
	lastAcknowledgedToken_ = token_;
	phase_ = IntentPhase::Acknowledged;
	token_ = 0;
	activeInvocationId_ = 0;
	expectedRoot_ = 0;
	expectedGeneration_ = 0;
	return true;
}

void NativeIntentGate::RejectForeignMutation() {
	if (phase_ == IntentPhase::Admitted) {
		phase_ = IntentPhase::Rejected;
		token_ = 0;
		activeInvocationId_ = 0;
		expectedRoot_ = 0;
		expectedGeneration_ = 0;
	}
}

void NativeIntentGate::BeginUpdate(UInt32 updateInvocationId) {
	activeUpdateInvocationId_ = updateInvocationId;
	// A previous admitted token that reached no expected stack mutation cannot
	// survive into another normal Update invocation.  A pending request is
	// intentionally preserved until this update gets a chance to query it.
	if (phase_ == IntentPhase::Admitted) {
		phase_ = IntentPhase::Rejected;
		token_ = 0;
		activeInvocationId_ = 0;
		expectedRoot_ = 0;
		expectedGeneration_ = 0;
	}
}

void NativeIntentGate::EndUpdate() {
	if (phase_ == IntentPhase::Pending || phase_ == IntentPhase::Admitted) {
		phase_ = IntentPhase::Rejected;
		token_ = 0;
		activeInvocationId_ = 0;
		expectedRoot_ = 0;
		expectedGeneration_ = 0;
	}
	activeUpdateInvocationId_ = 0;
}

void NativeIntentGate::Cancel() {
	if (phase_ == IntentPhase::Pending || phase_ == IntentPhase::Admitted) {
		phase_ = IntentPhase::Cancelled;
		token_ = 0;
		activeInvocationId_ = 0;
		expectedRoot_ = 0;
		expectedGeneration_ = 0;
	}
}

void NativeIntentGate::Reset() {
	phase_ = IntentPhase::Idle;
	token_ = 0;
	activeInvocationId_ = 0;
	lastAcknowledgedToken_ = 0;
	activeUpdateInvocationId_ = 0;
	expectedRoot_ = 0;
	expectedGeneration_ = 0;
	nextInvocationId_ = 0;
	kind_ = IntentKind::Open;
}

UInt32 BuildNativeIntentStub(UInt8* buffer, UInt32 capacity,
                             const NativeIntentStubSpec& spec) {
	mem::CodeWriter code(buffer, capacity, spec.stubAddress);
	code.PushFlags();
	code.PushAllRegisters();
	code.SubStackPointer(4);             // local invocation depth; frame is esp+4
	code.CallRelative(spec.enterTarget);
	code.MoveStackFromEax(0);            // local depth
	code.MoveEcxFromStack(28);           // saved incoming this
	code.MoveEdxFromStack(24);           // restore saved incoming EDX
	code.PushStackValue(48);             // original second argument
	code.PushStackValue(48);             // original first argument after first push
	code.CallRelative(spec.originalTarget);
	code.MoveStackFromEax(32);           // saved EAX in pushad frame
	code.MoveEdxFromStack(0);            // invocation depth
	code.LoadEffectiveAddressEax(4);     // NativeIntentSavedFrame*
	code.PushEdx();
	code.PushEax();
	code.CallRelative(spec.resolveTarget);
	code.AddStackPointer(8);
	code.MoveStackFromEax(32);           // final EAX for popad
	code.CallRelative(spec.leaveTarget);
	code.AddStackPointer(4);
	code.PopAllRegisters();
	code.PopFlags();
	code.ReturnAndPop(8);                // native query is ret 8
	return code.Overflowed() ? 0 : code.Size();
}

extern "C" UInt32 __cdecl NativeIntentStubEnter() {
	++g_invocationDepth;
	return g_invocationDepth;
}

extern "C" UInt32 __cdecl NativeIntentStubResolve(NativeIntentSavedFrame* frame,
                                                   UInt32 invocationDepth) {
	if (frame == nullptr) {
		return 0;
	}
	const UInt32 returnAddress = CallerReturn(*frame);
	const UInt32 currentThread = GetCurrentThreadId();
	// A worker-thread query is a pure pass-through.  Do this before reading
	// any gate fields so concurrent native work cannot race the main-thread
	// intent state merely by entering the generated stub.
	if (g_gameThreadId == 0 || currentThread != g_gameThreadId) {
		return frame->savedEax;
	}
	OpenIntentQuery query{};
	query.originalResult = frame->savedEax;
	query.pageSelection = frame->savedEdi;
	// Tests may use a synthetic caller.  Production config supplies the
	// verified open/close continuations; a zero config is an explicit harness
	// wildcard and still exercises the same generated stub and gate.
	const UInt32 expectedOrigin = g_gate.Kind() == IntentKind::Open
	                                 ? g_expectedContinuation
	                                 : g_expectedCloseContinuation;
	const UInt32 expectedContinuation = g_gate.Kind() == IntentKind::Open
	                                       ? kOpenIntentQueryContinuation
	                                       : kCloseIntentQueryContinuation;
	query.callReturnAddress = (expectedOrigin == 0 || returnAddress == expectedOrigin)
	                              ? expectedContinuation
	                              : 0;
	query.invocationDepth = invocationDepth;
	query.updateDepth = g_updateDepth;
	query.updateInvocationId = g_updateInvocationId;
	query.mainThread = g_gameThreadId != 0 && currentThread == g_gameThreadId;
	query.contextValid = g_contextValid;
	// Nested or wrong-thread calls must pass through without interpreting the
	// caller's transient ESI as an InterfaceManager pointer.
	query.normalOpenState = query.mainThread && invocationDepth == 1
	                          ? ReadOpenState(frame->savedEsi)
	                          : false;
	query.normalCloseState = query.mainThread && invocationDepth == 1
	                           ? ReadCloseState(frame->savedEsi)
	                           : false;
	query.ownedSession = g_gate.Kind() == IntentKind::Close && invocationDepth == 1 &&
	                     query.mainThread && g_ownershipProbe != nullptr &&
	                     g_ownershipProbe(frame->savedEsi, g_gate.ExpectedRoot(),
	                                       g_gate.ExpectedGeneration());
	const UInt32* originalStack = reinterpret_cast<const UInt32*>(frame->savedEsp);
	(void)originalStack;  // control/arg are retained by the target ABI harness.
	const IntentDecision decision = g_gate.Resolve(query);
	if (g_probeInstalled && (decision.resolution == IntentResolution::OverrideOpen ||
	                         decision.resolution == IntentResolution::OverrideClose)) {
		OBVR_LOG("Menu intent probe: %s override result=%u token=%u invocation=%u",
		         g_gate.Kind() == IntentKind::Open ? "open" : "close", decision.result,
		         decision.token, g_gate.ActiveInvocationId());
	}
	return decision.result;
}

extern "C" void __cdecl NativeIntentStubLeave() {
	if (g_invocationDepth != 0) {
		--g_invocationDepth;
	}
}

NativeIntentGate& NativeIntentGateState() { return g_gate; }

void ConfigureNativeIntentProbe(UInt32 gameThreadId, UInt32 expectedContinuation,
                                UInt32 expectedCloseContinuation) {
	g_gameThreadId = gameThreadId;
	g_expectedContinuation = expectedContinuation;
	g_expectedCloseContinuation = expectedCloseContinuation != 0
	                                 ? expectedCloseContinuation
	                                 : expectedContinuation;
	g_gate.Reset();
	g_contextValid = false;
	g_invocationDepth = 0;
	g_updateDepth = 0;
	g_updateInvocationId = 0;
	g_nextUpdateInvocationId = 0;
}

void SetNativeIntentContextValid(bool valid) { g_contextValid = valid; }
void SetNativeIntentOwnershipProbe(NativeIntentOwnershipProbe probe) {
	g_ownershipProbe = probe;
}

bool ArmNativeOpenIntent(UInt32 token) { return g_gate.Arm(token, IntentKind::Open); }
bool ArmNativeCloseIntent(UInt32 token, UInt32 expectedRoot, UInt32 expectedGeneration) {
	return g_gate.Arm(token, IntentKind::Close, expectedRoot, expectedGeneration);
}
bool ArmNativeRequest(UInt32 token, UInt32 operation, UInt32 expectedRoot,
	                  UInt32 expectedGeneration) {
	return operation == 2 ? ArmNativeCloseIntent(token, expectedRoot, expectedGeneration)
	                      : operation == 1 ? ArmNativeOpenIntent(token) : false;
}
void CancelNativeOpenIntent() { g_gate.Cancel(); }
void CancelNativeCloseIntent() { g_gate.Cancel(); }
UInt32 NativeIntentInvocationDepth() { return g_invocationDepth; }
UInt32 NativeIntentUpdateDepth() { return g_updateDepth; }
UInt32 NativeIntentActiveUpdateInvocation() { return g_updateInvocationId; }

bool NativeIntentUpdateEnter() {
	if (g_gameThreadId == 0 || GetCurrentThreadId() != g_gameThreadId) {
		return false;
	}
	if (g_updateDepth == 0) {
		++g_nextUpdateInvocationId;
		if (g_nextUpdateInvocationId == 0) {
			++g_nextUpdateInvocationId;
		}
		g_updateInvocationId = g_nextUpdateInvocationId;
		g_gate.BeginUpdate(g_updateInvocationId);
	}
	++g_updateDepth;
	return true;
}

void NativeIntentUpdateLeave() {
	if (g_updateDepth == 0) {
		return;
	}
	--g_updateDepth;
	if (g_updateDepth == 0) {
		g_gate.EndUpdate();
		g_updateInvocationId = 0;
	}
}

void RunNativeIntentUpdate(NativeIntentUpdateBody body, void* context) {
	if (body == nullptr) {
		return;
	}
	const bool scoped = NativeIntentUpdateEnter();
	body(context);
	if (scoped) {
		NativeIntentUpdateLeave();
	}
}

bool IsNativeIntentProbeInstalled() { return g_probeInstalled; }

bool InstallNativeIntentProbe() {
	if (g_probeInstalled || g_probeUnsafe) {
		return false;
	}
	char observedHash[65]{};
	const bool hashRead = ReadExecutableHash(observedHash, sizeof(observedHash));
	const bool hashMatches = hashRead && SameText(observedHash, kExpectedOblivionSha256);
	const NativeIntentInstallCheck hashCheck =
		EvaluateNativeIntentInstall(false, hashRead, hashMatches, true, true);
	if (hashCheck == NativeIntentInstallCheck::HashReadFailed) {
		OBVR_LOG("Menu intent probe: executable hash could not be read; expected %s",
		         kExpectedOblivionSha256);
		return false;
	}
	if (hashCheck == NativeIntentInstallCheck::HashMismatch) {
		OBVR_LOG("Menu intent probe: executable hash mismatch; expected %s observed %s",
		         kExpectedOblivionSha256, observedHash);
		return false;
	}
	const bool openSignature = mem::Verify(kOpenIntentQueryCallSite, kOpenQueryBytes,
	                                       kOpenIntentPatchSize);
	const bool closeSignature = mem::Verify(kCloseIntentQueryCallSite, kCloseQueryBytes,
	                                        kOpenIntentPatchSize);
	switch (EvaluateNativeIntentInstall(false, true, true, openSignature,
	                                     closeSignature)) {
	case NativeIntentInstallCheck::Ready:
		break;
	case NativeIntentInstallCheck::OpenSignatureMismatch:
		mem::ReportForeignCode("Menu intent open query", kOpenIntentQueryCallSite);
		return false;
	case NativeIntentInstallCheck::CloseSignatureMismatch:
		mem::ReportForeignCode("Menu intent close query", kCloseIntentQueryCallSite);
		return false;
	default:
		return false;
	}
	constexpr UInt32 kStubCapacity = 96;
	auto* stub = static_cast<UInt8*>(mem::AllocExecutable(kStubCapacity));
	if (stub == nullptr) {
		return false;
	}
	const UInt32 stubAddress = reinterpret_cast<UInt32>(stub);
	const NativeIntentStubSpec spec{
		stubAddress,
		kOpenIntentQueryTarget,
		reinterpret_cast<UInt32>(&NativeIntentStubEnter),
		reinterpret_cast<UInt32>(&NativeIntentStubResolve),
		reinterpret_cast<UInt32>(&NativeIntentStubLeave),
	};
	if (BuildNativeIntentStub(stub, kStubCapacity, spec) == 0) {
		FreeStub(stub);
		return false;
	}
	auto* closeStub = static_cast<UInt8*>(mem::AllocExecutable(kStubCapacity));
	if (closeStub == nullptr) {
		FreeStub(stub);
		return false;
	}
	const UInt32 closeStubAddress = reinterpret_cast<UInt32>(closeStub);
	NativeIntentStubSpec closeSpec = spec;
	closeSpec.stubAddress = closeStubAddress;
	if (BuildNativeIntentStub(closeStub, kStubCapacity, closeSpec) == 0) {
		FreeStub(closeStub);
		FreeStub(stub);
		return false;
	}
	UInt8 patch[kOpenIntentPatchSize]{};
	UInt8 closePatch[kOpenIntentPatchSize]{};
	if (mem::BuildCallSitePatch(patch, sizeof(patch), kOpenIntentQueryCallSite,
	                            stubAddress) != kOpenIntentPatchSize ||
	    mem::BuildCallSitePatch(closePatch, sizeof(closePatch), kCloseIntentQueryCallSite,
	                            closeStubAddress) != kOpenIntentPatchSize ||
	    !mem::SafeWrite(kOpenIntentQueryCallSite, patch, kOpenIntentPatchSize)) {
		FreeStub(closeStub);
		FreeStub(stub);
		return false;
	}
	if (!mem::SafeWrite(kCloseIntentQueryCallSite, closePatch, kOpenIntentPatchSize)) {
		if (!mem::SafeWrite(kOpenIntentQueryCallSite, kOpenQueryBytes, kOpenIntentPatchSize)) {
			g_probeUnsafe = true;
			g_stub = stub;
			g_closeStub = closeStub;
			OBVR_LOG("Menu intent probe: UNSAFE rollback failure at %08X; retry refused",
			         kCloseIntentQueryCallSite);
		} else {
			FreeStub(closeStub);
			FreeStub(stub);
		}
		return false;
	}
	g_stub = stub;
	g_closeStub = closeStub;
	g_probeInstalled = true;
	OBVR_LOG("Menu intent probe installed at %08X/%08X; verified executable hash is "
	         "%s", kOpenIntentQueryCallSite, kCloseIntentQueryCallSite,
	         kExpectedOblivionSha256);
	return true;
}

}  // namespace obvr::game::vrbridge
