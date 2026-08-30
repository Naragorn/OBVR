#include "vr/OpenVRBackend.h"

#include "core/MathFns.h"

#include "core/Log.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "vr/OpenVRTypes.h"
#include "vr/Quaternion.h"

namespace obvr::vr {
namespace {

// LoadLibraryA searches next to the running executable first. So the x86
// build of openvr_api.dll belongs next to Oblivion.exe - not the x64 build
// that ships with other games, and not the one inside SteamVR's own folder.
constexpr const char* kOpenVRLibrary = "openvr_api.dll";

template <typename Fn>
Fn Resolve(void* module, const char* name) {
	return reinterpret_cast<Fn>(GetProcAddress(static_cast<HMODULE>(module), name));
}

}  // namespace

void OpenVRBackend::LogOnce(bool& alreadyLogged, const char* message) const {
	if (!alreadyLogged) {
		alreadyLogged = true;
		OBVR_LOG("OpenVR: %s", message);
	}
}

bool OpenVRBackend::Connect(int applicationType) {
	auto initInternal = Resolve<openvr::VR_InitInternalFn>(m_module, "VR_InitInternal");
	auto getInterface =
		Resolve<openvr::VR_GetGenericInterfaceFn>(m_module, "VR_GetGenericInterface");
	auto errorText = Resolve<openvr::VR_GetVRInitErrorAsEnglishDescriptionFn>(
		m_module, "VR_GetVRInitErrorAsEnglishDescription");

	int error = openvr::kInitErrorNone;
	initInternal(&error, applicationType);
	if (error != openvr::kInitErrorNone) {
		OBVR_LOG("OpenVR: VR_InitInternal as type %d failed (%d: %s)", applicationType, error,
		         errorText != nullptr ? errorText(error) : "no description");
		return false;
	}

	error = openvr::kInitErrorNone;
	m_system = getInterface(openvr::kIVRSystemFnTableVersion, &error);
	if (m_system == nullptr || error != openvr::kInitErrorNone) {
		OBVR_LOG("OpenVR: %s unavailable (%d: %s)", openvr::kIVRSystemFnTableVersion, error,
		         errorText != nullptr ? errorText(error) : "no description");

		// Registered but unusable. Unregistering here rather than leaving it
		// is what makes a second attempt with a different application type
		// safe: SteamVR would otherwise still hold the first registration.
		if (auto shutdown =
				Resolve<openvr::VR_ShutdownInternalFn>(m_module, "VR_ShutdownInternal")) {
			shutdown();
		}
		return false;
	}

	return true;
}

bool OpenVRBackend::Start(bool wantScene) {
	if (m_system != nullptr) {
		return true;
	}
	if (m_startAttempted) {
		// Retrying would print the same failure every frame. Anyone who
		// starts SteamVR afterwards can restart Oblivion.
		return false;
	}
	m_startAttempted = true;

	// Next to OBVR.dll first, which means Data/OBSE/Plugins. That is inside
	// the folder Mod Organizer 2 virtualises, so the library can ship as part
	// of the mod rather than needing the Root Builder plugin to place it in
	// the game root. The load happens long after USVFS has installed its
	// hooks, so the virtual path resolves.
	char path[512];
	if (platform::BuildPluginPath(kOpenVRLibrary, path, sizeof(path))) {
		m_module = LoadLibraryA(path);
	}

	// Otherwise the plain name, which LoadLibrary resolves next to
	// Oblivion.exe - the layout every non-MO2 installation uses.
	if (m_module == nullptr) {
		m_module = LoadLibraryA(kOpenVRLibrary);
	}

	if (m_module == nullptr) {
		OBVR_LOG("OpenVR: %s not found, camera stays unchanged", kOpenVRLibrary);
		OBVR_LOG("OpenVR: the x86 build belongs next to OBVR.dll or next to Oblivion.exe");
		return false;
	}

	auto isRuntimeInstalled =
		Resolve<openvr::VR_IsRuntimeInstalledFn>(m_module, "VR_IsRuntimeInstalled");
	auto initInternal = Resolve<openvr::VR_InitInternalFn>(m_module, "VR_InitInternal");
	auto getInterface =
		Resolve<openvr::VR_GetGenericInterfaceFn>(m_module, "VR_GetGenericInterface");

	if (initInternal == nullptr || getInterface == nullptr) {
		OBVR_LOG("OpenVR: %s does not have the expected exports", kOpenVRLibrary);
		FreeLibrary(static_cast<HMODULE>(m_module));
		m_module = nullptr;
		return false;
	}

	if (isRuntimeInstalled != nullptr && !isRuntimeInstalled()) {
		OBVR_LOG("OpenVR: no runtime installed, camera stays unchanged");
		FreeLibrary(static_cast<HMODULE>(m_module));
		m_module = nullptr;
		return false;
	}

	// The scene attempt first, when asked for. It is the only way to submit a
	// frame, and it is also the only way to take the headset away from
	// whatever else is using it - which is why it happens on request rather
	// than by default.
	if (wantScene && Connect(openvr::kApplicationScene)) {
		int error = openvr::kInitErrorNone;
		m_compositor = getInterface(openvr::kIVRCompositorFnTableVersion, &error);

		if (m_compositor != nullptr && error == openvr::kInitErrorNone) {
			// One tracking space for everything, declared rather than
			// defaulted. The camera path reads its poses seated, and the
			// overlay anchor is interpreted seated - but WaitGetPoses hands
			// out poses in the compositor's space, which defaults to
			// standing. Three places seated and one on the default is how
			// the HUD anchor landed at (-9.2, -8.1, -2.7): a standing-space
			// pose hung in the seated space, metres from where anyone
			// looked. Where the seated origin sits does not matter; that
			// every pose and the overlay agree on it does.
			auto* compositorTable =
				static_cast<openvr::IVRCompositorFnTable*>(m_compositor);
			if (compositorTable->SetTrackingSpace != nullptr) {
				compositorTable->SetTrackingSpace(openvr::kTrackingUniverseSeated);
			}
			OBVR_LOG("OpenVR: connected as a scene application through %s and %s, "
			         "tracking space set to seated",
			         openvr::kIVRSystemFnTableVersion, openvr::kIVRCompositorFnTableVersion);
			return true;
		}

		// Registered as a scene application but without a compositor to
		// submit to. Retreating to background costs the picture and keeps
		// head tracking, which is the better half to keep: a camera that
		// still follows the head is a working mod, a headset showing nothing
		// is not.
		OBVR_LOG("OpenVR: %s unavailable (%d), falling back to head tracking only",
		         openvr::kIVRCompositorFnTableVersion, error);
		m_compositor = nullptr;
		m_system = nullptr;
		if (auto shutdown =
				Resolve<openvr::VR_ShutdownInternalFn>(m_module, "VR_ShutdownInternal")) {
			shutdown();
		}
	}

	// Background: reads poses and leaves the compositor alone. Everything up
	// to 0.0.4 works this way, and it stays the default until rendering is
	// asked for explicitly.
	if (!Connect(openvr::kApplicationBackground)) {
		FreeLibrary(static_cast<HMODULE>(m_module));
		m_module = nullptr;
		return false;
	}

	OBVR_LOG("OpenVR: connected through %s", openvr::kIVRSystemFnTableVersion);
	return true;
}

void OpenVRBackend::Stop() {
	if (m_module == nullptr) {
		return;
	}

	if (auto shutdown =
			Resolve<openvr::VR_ShutdownInternalFn>(m_module, "VR_ShutdownInternal")) {
		shutdown();
	}

	FreeLibrary(static_cast<HMODULE>(m_module));
	m_module = nullptr;
	m_system = nullptr;
	m_compositor = nullptr;
	m_overlay = nullptr;
	m_overlayTried = false;
	OBVR_LOG("OpenVR: disconnected");
}

bool OpenVRBackend::EnsureOverlayInterface() {
	if (m_overlay != nullptr) {
		return true;
	}
	if (m_overlayTried || m_module == nullptr) {
		return false;
	}
	m_overlayTried = true;

	auto getInterface =
		Resolve<openvr::VR_GetGenericInterfaceFn>(m_module, "VR_GetGenericInterface");
	if (getInterface == nullptr) {
		return false;
	}

	int error = openvr::kInitErrorNone;
	m_overlay = getInterface(openvr::kIVROverlayFnTableVersion, &error);
	if (m_overlay == nullptr || error != openvr::kInitErrorNone) {
		m_overlay = nullptr;
		OBVR_LOG("OpenVR: %s unavailable (%d), so the 2D layer cannot leave the picture",
		         openvr::kIVROverlayFnTableVersion, error);
		return false;
	}

	OBVR_LOG("OpenVR: overlay interface connected through %s",
	         openvr::kIVROverlayFnTableVersion);
	return true;
}

bool OpenVRBackend::CreateOverlay(const char* key, const char* name,
                                  openvr::VROverlayHandle& handle) {
	if (!EnsureOverlayInterface()) {
		return false;
	}

	auto* table = static_cast<openvr::IVROverlayFnTable*>(m_overlay);
	if (table->CreateOverlay == nullptr) {
		return false;
	}

	openvr::VROverlayHandle created = openvr::kOverlayHandleInvalid;
	const int error = table->CreateOverlay(key, name, &created);
	if (error != openvr::kOverlayErrorNone || created == openvr::kOverlayHandleInvalid) {
		OBVR_LOG("OpenVR: CreateOverlay('%s') failed (%d)", key, error);
		return false;
	}

	handle = created;
	return true;
}

int OpenVRBackend::SetOverlayTexture(openvr::VROverlayHandle handle,
                                     const void* vulkanData) const {
	auto* table = static_cast<openvr::IVROverlayFnTable*>(m_overlay);
	if (table == nullptr || table->SetOverlayTexture == nullptr) {
		return -1;
	}

	// The same shape SubmitEye hands the compositor: the handle field carries
	// the pointer to the Vulkan description, and the type says so.
	openvr::Texture texture{};
	texture.handle = const_cast<void*>(vulkanData);
	texture.type = openvr::kTextureTypeVulkan;
	texture.colorSpace = openvr::kColorSpaceAuto;
	return table->SetOverlayTexture(handle, &texture);
}

int OpenVRBackend::SetOverlayTransformHmdRelative(
	openvr::VROverlayHandle handle, const openvr::HmdMatrix34& hmdToOverlay) const {
	auto* table = static_cast<openvr::IVROverlayFnTable*>(m_overlay);
	if (table == nullptr || table->SetOverlayTransformTrackedDeviceRelative == nullptr) {
		return -1;
	}
	return table->SetOverlayTransformTrackedDeviceRelative(
		handle, openvr::kTrackedDeviceIndexHmd, &hmdToOverlay);
}

int OpenVRBackend::SetOverlayTransformAbsolute(
	openvr::VROverlayHandle handle, const openvr::HmdMatrix34& trackingToOverlay) const {
	auto* table = static_cast<openvr::IVROverlayFnTable*>(m_overlay);
	if (table == nullptr || table->SetOverlayTransformAbsolute == nullptr) {
		return -1;
	}
	// Seated, and it must stay the same space the compositor hands poses out
	// in - Initialize declares that space seated for exactly this call's
	// sake. An anchor pose from one space hung in another is a HUD nobody
	// finds.
	return table->SetOverlayTransformAbsolute(handle, openvr::kTrackingUniverseSeated,
	                                          &trackingToOverlay);
}

int OpenVRBackend::SetOverlayWidthInMetres(openvr::VROverlayHandle handle,
                                           float metres) const {
	auto* table = static_cast<openvr::IVROverlayFnTable*>(m_overlay);
	if (table == nullptr || table->SetOverlayWidthInMeters == nullptr) {
		return -1;
	}
	return table->SetOverlayWidthInMeters(handle, metres);
}

int OpenVRBackend::SetOverlayTextureBounds(openvr::VROverlayHandle handle,
                                           const openvr::VRTextureBounds& bounds) const {
	auto* table = static_cast<openvr::IVROverlayFnTable*>(m_overlay);
	if (table == nullptr || table->SetOverlayTextureBounds == nullptr) {
		return -1;
	}
	return table->SetOverlayTextureBounds(handle, &bounds);
}

int OpenVRBackend::ShowOverlay(openvr::VROverlayHandle handle) const {
	auto* table = static_cast<openvr::IVROverlayFnTable*>(m_overlay);
	if (table == nullptr || table->ShowOverlay == nullptr) {
		return -1;
	}
	return table->ShowOverlay(handle);
}

int OpenVRBackend::HideOverlay(openvr::VROverlayHandle handle) const {
	auto* table = static_cast<openvr::IVROverlayFnTable*>(m_overlay);
	if (table == nullptr || table->HideOverlay == nullptr) {
		return -1;
	}
	return table->HideOverlay(handle);
}

int OpenVRBackend::DestroyOverlay(openvr::VROverlayHandle handle) {
	auto* table = static_cast<openvr::IVROverlayFnTable*>(m_overlay);
	if (table == nullptr || table->DestroyOverlay == nullptr ||
	    handle == openvr::kOverlayHandleInvalid) {
		return -1;
	}
	return table->DestroyOverlay(handle);
}

bool OpenVRBackend::GetEyeProjection(int eye, float& left, float& right, float& top,
                                     float& bottom) const {
	if (m_system == nullptr) {
		return false;
	}

	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table->GetProjectionRaw == nullptr) {
		return false;
	}

	float l = 0.0f;
	float r = 0.0f;
	float t = 0.0f;
	float b = 0.0f;
	table->GetProjectionRaw(eye, &l, &r, &t, &b);

	// A frustum with no width or no height is not a frustum. This catches a
	// runtime that answered without filling anything in, which would
	// otherwise turn into a division by zero downstream and be reported as a
	// projection fault rather than a missing answer.
	if (r - l == 0.0f || b - t == 0.0f) {
		return false;
	}

	left = l;
	right = r;
	top = t;
	bottom = b;
	return true;
}

bool OpenVRBackend::GetEyeOffset(int eye, NiPoint3& offsetMetres) const {
	if (m_system == nullptr) {
		return false;
	}

	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table->GetEyeToHeadTransform == nullptr) {
		return false;
	}

	// Returns by value; see the note in OpenVRTypes.h about the hidden return
	// pointer. The translation is the fourth column, exactly as in a tracked
	// device pose, so it goes through the same reader.
	const openvr::HmdMatrix34 transform = table->GetEyeToHeadTransform(eye);
	offsetMetres = PositionFromOpenVRMatrix(transform.m);
	return true;
}

int OpenVRBackend::WaitGetPoses() {
	m_renderPoseValid = false;

	if (m_compositor == nullptr) {
		return openvr::kCompositorErrorIsNotSceneApplication;
	}

	auto* table = static_cast<openvr::IVRCompositorFnTable*>(m_compositor);
	if (table->WaitGetPoses == nullptr) {
		return openvr::kCompositorErrorIsNotSceneApplication;
	}

	// One entry rather than kMaxTrackedDeviceCount, for the same reason
	// ReadHeadPose asks for one: the HMD is index 0 and the array is filled
	// from the front, so the rest would be five kilobytes of controllers and
	// base stations on the stack every frame.
	//
	// The game poses are declined outright. They are the poses to run game
	// logic against, and Oblivion's logic knows nothing about a headset.
	openvr::TrackedDevicePose renderPose{};
	const int result = table->WaitGetPoses(&renderPose, 1, nullptr, 0);

	// Kept, not discarded. This is the pose the compositor will reproject the
	// submitted picture against, so it is the pose the picture has to be drawn
	// with - see the comment on the declaration, and Valve's own statement
	// that rendering with any other pose "will result in incorrect behavior".
	//
	// It is also better in its own right: predicted forward to when the image
	// will be lit, rather than the head's position at the moment of asking.
	if (renderPose.poseIsValid && renderPose.deviceIsConnected) {
		m_renderOrientation = FromOpenVRMatrix(renderPose.deviceToAbsoluteTracking.m);
		m_renderPosition = PositionFromOpenVRMatrix(renderPose.deviceToAbsoluteTracking.m);
		m_renderPoseMatrix = renderPose.deviceToAbsoluteTracking;
		m_renderPoseValid = true;
	}

	return result;
}

bool OpenVRBackend::GetRenderPose(Quaternion& orientation, NiPoint3& position) const {
	if (!m_renderPoseValid) {
		return false;
	}

	orientation = m_renderOrientation;
	position = m_renderPosition;
	return true;
}

bool OpenVRBackend::GetRenderPoseMatrix(openvr::HmdMatrix34& out) const {
	if (!m_renderPoseValid) {
		return false;
	}
	out = m_renderPoseMatrix;
	return true;
}

int OpenVRBackend::SubmitEye(int eye, void* handle, int textureType,
                             const openvr::VRTextureBounds* bounds,
                             const openvr::HmdMatrix34* renderPose) const {
	if (m_compositor == nullptr || handle == nullptr) {
		return openvr::kCompositorErrorIsNotSceneApplication;
	}

	auto* table = static_cast<openvr::IVRCompositorFnTable*>(m_compositor);
	if (table->Submit == nullptr) {
		return openvr::kCompositorErrorIsNotSceneApplication;
	}

	// The two shapes share their first three fields, which is why the pose
	// carrying one can be handed to a parameter typed as the plain one - the
	// flag is what tells the compositor which it is looking at. Getting the
	// flag and the structure out of step would have it read a matrix out of
	// whatever follows.
	openvr::VRTextureWithPose withPose{};
	withPose.texture.handle = handle;
	withPose.texture.type = textureType;

	// Auto rather than a stated colour space. The texture is
	// R8G8B8A8_UNORM, and letting the compositor apply its own rule for that
	// format is more likely to be right than OBVR asserting one - the ramp in
	// the test pattern is there precisely so that a wrong guess here is
	// visible rather than merely suspected.
	withPose.texture.colorSpace = openvr::kColorSpaceAuto;

	if (renderPose == nullptr) {
		return table->Submit(eye, &withPose.texture, bounds, openvr::kSubmitDefault);
	}

	withPose.deviceToAbsoluteTracking = *renderPose;
	return table->Submit(eye, &withPose.texture, bounds, openvr::kSubmitTextureWithPose);
}
bool OpenVRBackend::GetRecommendedRenderTargetSize(UInt32& width, UInt32& height) const {
	if (m_system == nullptr) {
		return false;
	}

	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table->GetRecommendedRenderTargetSize == nullptr) {
		return false;
	}

	UInt32 w = 0;
	UInt32 h = 0;
	table->GetRecommendedRenderTargetSize(&w, &h);

	// A zero from a runtime that answered anyway would otherwise become a
	// texture of no size, and Direct3D reports that as a generic failure with
	// nothing pointing back to here.
	if (w == 0 || h == 0) {
		return false;
	}

	width = w;
	height = h;
	return true;
}

bool OpenVRBackend::ReadHeadPose(Quaternion& orientation, NiPoint3& position) const {
	if (m_system == nullptr) {
		return false;
	}

	auto* table = static_cast<openvr::IVRSystemFnTable*>(m_system);
	if (table->GetDeviceToAbsoluteTrackingPose == nullptr) {
		return false;
	}

	// One entry instead of kMaxTrackedDeviceCount: the HMD is always index 0
	// and the call fills the array from the front. That saves a good five
	// kilobytes of stack per frame which would only describe controllers and
	// base stations.
	openvr::TrackedDevicePose pose{};

	// No prediction: predictedSecondsFromNow stays zero because OBVR
	// has no frame loop of its own and therefore does not know when the image
	// will be presented. That belongs to the milestone that brings a frame loop.
	table->GetDeviceToAbsoluteTrackingPose(openvr::kTrackingUniverseSeated, 0.0f, &pose, 1);

	if (!pose.poseIsValid || !pose.deviceIsConnected) {
		// Happens routinely right after startup while tracking has not picked
		// up yet. The caller keeps the last valid pose instead of letting the
		// camera snap back to rest.
		LogOnce(m_loggedNoPose, "no valid HMD pose yet");
		return false;
	}

	orientation = FromOpenVRMatrix(pose.deviceToAbsoluteTracking.m);
	position = PositionFromOpenVRMatrix(pose.deviceToAbsoluteTracking.m);
	return true;
}


float PoseDistanceSq(const openvr::HmdMatrix34& a, const openvr::HmdMatrix34& b) {
	const float dx = a.m[0][3] - b.m[0][3];
	const float dy = a.m[1][3] - b.m[1][3];
	const float dz = a.m[2][3] - b.m[2][3];
	return dx * dx + dy * dy + dz * dz;
}

openvr::HmdMatrix34 OverlayPoseAhead(const openvr::HmdMatrix34& pose,
                                     float distanceMetres) {
	openvr::HmdMatrix34 result = pose;

	// Forward is the negative third column. Walking the position along it
	// leaves the rotation alone, which is what keeps a levelled anchor level
	// and stops the quad from tipping with wherever the head was.
	for (int row = 0; row < 3; ++row) {
		result.m[row][3] = pose.m[row][3] - pose.m[row][2] * distanceMetres;
	}
	return result;
}

void LevelPose(openvr::HmdMatrix34& pose) {
	// Yaw only: no pitch, no roll.
	//
	// Roll has to go because a horizon at an angle in a headset is nausea
	// within seconds. Pitch goes with it for a different reason, and it is a
	// deliberate choice rather than a consequence: in the world the recenter
	// key turns the wearer to face a direction and does not change how high
	// they are looking, because vertical aim belongs to the game. An anchor
	// that kept pitch would make the same key mean two different things
	// depending on whether a menu was open.
	//
	// What that costs, stated rather than discovered later: the picture hangs
	// at eye level, not where the wearer happens to be looking. A head at rest
	// tilts slightly down, so pressing the key while looking down leaves the
	// picture above the line of sight. That is what a screen on a wall does,
	// and the answer to it is to look up rather than to move the wall.
	//
	// Backwards is the third column, so forward is its negative.
	float fx = -pose.m[0][2];
	float fz = -pose.m[2][2];

	const float lengthSquared = fx * fx + fz * fz;
	if (!(lengthSquared > 0.0001f)) {
		// Straight up or straight down: there is no heading to keep, and
		// inventing one would swing the picture by whatever the arithmetic
		// happened to produce. Left as it is - tilted, but not arbitrarily so.
		return;
	}

	const float length = math::Sqrt(lengthSquared);
	fx /= length;
	fz /= length;

	// Right is forward crossed with up, for up = (0, 1, 0). Written out
	// rather than called, because two cross products and a normalise would be
	// three chances to get a sign wrong in something whose failure is a world
	// that is subtly mirrored.
	//
	//   right    = ( -fz, 0,  fx )
	//   up       = (   0, 1,   0 )
	//   backward = ( -fx, 0, -fz )
	pose.m[0][0] = -fz;
	pose.m[1][0] = 0.0f;
	pose.m[2][0] = fx;

	pose.m[0][1] = 0.0f;
	pose.m[1][1] = 1.0f;
	pose.m[2][1] = 0.0f;

	pose.m[0][2] = -fx;
	pose.m[1][2] = 0.0f;
	pose.m[2][2] = -fz;

	// The fourth column is the position and is left alone: a menu anchored
	// where the wearer is standing is right, and moving it would be a second
	// change hiding inside this one.
}

void LevelRollOnly(openvr::HmdMatrix34& pose) {
	// Keeps where the wearer is looking, horizontally and vertically, and
	// takes out only the tilt of the head.
	//
	// This is what a flat picture wants and LevelPose is not. LevelPose drops
	// pitch on purpose, so that the recenter key means the same thing whether
	// or not a menu is open - which is right for the world camera, where
	// vertical aim belongs to the game. A picture hanging in the room is a
	// different question: the key means "put it where I am looking", and a
	// wearer who is looking down at the time means down. Measured from the
	// headset: pressing the key during a film moved nothing, and the anchors
	// it took were within five centimetres of each other - the head had been
	// tilted, not turned, and tilt was exactly what was being discarded.
	//
	// Roll still goes. A horizon at an angle is nausea within seconds, and
	// nobody presses a key to ask for one.
	const float bx = pose.m[0][2];
	const float by = pose.m[1][2];
	const float bz = pose.m[2][2];

	// Right is horizontal by construction: the cross product of world up with
	// backward has no vertical component.
	float rx = bz;
	float rz = -bx;

	const float lengthSquared = rx * rx + rz * rz;
	if (!(lengthSquared > 0.0001f)) {
		// Looking straight up or straight down: there is no horizontal
		// direction left to square the picture against, and inventing one
		// would spin it by whatever the arithmetic produced. Left alone.
		return;
	}

	const float length = math::Sqrt(lengthSquared);
	rx /= length;
	rz /= length;

	// Up completes the frame: backward crossed with right.
	const float ux = by * rz;
	const float uy = bz * rx - bx * rz;
	const float uz = -by * rx;

	pose.m[0][0] = rx;
	pose.m[1][0] = 0.0f;
	pose.m[2][0] = rz;

	pose.m[0][1] = ux;
	pose.m[1][1] = uy;
	pose.m[2][1] = uz;

	// Backward is untouched, which is what keeps both the heading and the
	// pitch. The position, as above, is not this function's business.
}
}  // namespace obvr::vr
