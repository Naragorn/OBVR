// Checks the menus on the big quad facing the body and gliding after it
// (vr/MenuFacing.h).

#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "vr/MenuFacing.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr float kDt = 1.0f / 90.0f;
constexpr float kEyesY = 1.6f;

// A head turned `yaw` degrees to the left and `pitch` up, at (x, y, z).
openvr::HmdMatrix34 Head(float yaw, float pitch = 0.0f, float x = 0.0f, float y = kEyesY, float z = 0.0f) {
	const float a = yaw * math::kDegreesToRadians;
	const float b = pitch * math::kDegreesToRadians;
	const float ca = std::cos(a), sa = std::sin(a), cb = std::cos(b), sb = std::sin(b);
	// Ry(a) * Rx(b): columns right, up, back.
	openvr::HmdMatrix34 m{};
	m.m[0][0] = ca;
	m.m[1][0] = 0.0f;
	m.m[2][0] = -sa;
	m.m[0][1] = sa * sb;
	m.m[1][1] = cb;
	m.m[2][1] = ca * sb;
	m.m[0][2] = sa * cb;
	m.m[1][2] = -sb;
	m.m[2][2] = ca * cb;
	m.m[0][3] = x;
	m.m[1][3] = y;
	m.m[2][3] = z;
	return m;
}

// The quad's anchor heading, degrees, left positive.
float YawOf(const openvr::HmdMatrix34& m) {
	return std::atan2(m.m[0][2], m.m[2][2]) * math::kRadiansToDegrees;
}

struct Run {
	MenuFacingState s;
	MenuFacingSettings set;
	MenuFacingInput in;
	MenuFacingVerdict last;
	openvr::HmdMatrix34 held{};  // what the layer holds: the last pose handed over
	bool haveHeld = false;
	int handed = 0;
	bool began = false;
	bool ended = false;

	Run() {
		in.haveHead = true;
		in.dtSeconds = kDt;
		in.menusInRoom = true;
		in.kind = MenuFacingKind::Body;
	}
	void Frame(const openvr::HmdMatrix34& head) {
		in.head = head;
		last = StepMenuFacing(s, in, set);
		in.opened = false;
		if (last.anchor) {
			held = last.pose;
			haveHeld = true;
			++handed;
		}
		began = began || last.glideBegan;
		ended = ended || last.glideEnded;
	}
	// Holds the head at `yaw` for `seconds`.
	void Hold(float yaw, float seconds, float pitch = 0.0f, float y = kEyesY) {
		for (float t = 0.0f; t < seconds - 1e-4f; t += kDt) {
			Frame(Head(yaw, pitch, 0.0f, y));
		}
	}
	// Turns the head from `from` to `to` over `seconds`, evenly.
	void Turn(float from, float to, float seconds) {
		const int frames = static_cast<int>(seconds / kDt + 0.5f);
		for (int i = 1; i <= frames; ++i) {
			Frame(Head(from + (to - from) * static_cast<float>(i) / static_cast<float>(frames)));
		}
	}
	void Open() {
		in.menuUp = true;
		in.opened = true;
	}
};

bool Near(float a, float b, float within) {
	return std::fabs(a - b) <= within;
}

void TestKinds() {
	std::printf("Which menus face the body\n");
	Check(MenuFacingFor(nullptr, 0) == MenuFacingKind::None, "no stack: none");
	const UInt32 hud[] = {game::kMenuIdHudMain, game::kMenuIdHudInfo, game::kMenuIdHudReticle,
	                      game::kMenuIdHudSubtitle};
	Check(MenuFacingFor(hud, 4) == MenuFacingKind::None, "the HUD's own entries alone: none");
	const UInt32 box[] = {game::kMenuIdHudMain, game::kMenuIdMessage};
	Check(MenuFacingFor(box, 2) == MenuFacingKind::Body, "a message box in the world: the body");
	const UInt32 generic[] = {game::kMenuIdGeneric};
	Check(MenuFacingFor(generic, 1) == MenuFacingKind::Body, "a big box (Generic: a tutorial's help): the body");
	const UInt32 pause[] = {game::kMenuIdPause, game::kMenuIdOptions, game::kMenuIdMessage};
	Check(MenuFacingFor(pause, 3) == MenuFacingKind::Body, "the Esc menu, its options and a box over them: the body");
	const UInt32 pages[] = {game::kMenuIdBigFour, game::kMenuIdQuantity};
	Check(MenuFacingFor(pages, 2) == MenuFacingKind::Body, "the journal's pages (the tablet off): the body");
	const UInt32 mod[] = {0x41Au};
	Check(MenuFacingFor(mod, 1) == MenuFacingKind::Body, "a menu not named here: the body");
	const UInt32 talk[] = {game::kMenuIdDialog, game::kMenuIdPause};
	Check(MenuFacingFor(talk, 2) == MenuFacingKind::OwnPlace, "the Esc menu over a talk: the dialogue's place");
	const UInt32 train[] = {game::kMenuIdTraining};
	Check(MenuFacingFor(train, 1) == MenuFacingKind::OwnPlace, "training: the dialogue's place");
	const UInt32 persuade[] = {game::kMenuIdPersuasion};
	Check(MenuFacingFor(persuade, 1) == MenuFacingKind::OwnPlace, "persuasion: the dialogue's place");
	const UInt32 main[] = {game::kMenuIdMain, game::kMenuIdMessage};
	Check(MenuFacingFor(main, 2) == MenuFacingKind::NotWorld, "a box over the main menu: as before");
	const UInt32 loading[] = {game::kMenuIdLoading};
	Check(MenuFacingFor(loading, 1) == MenuFacingKind::NotWorld, "a load: as before");
	const UInt32 race[] = {game::kMenuIdRaceSex, game::kMenuIdDialog};
	Check(MenuFacingFor(race, 2) == MenuFacingKind::NotWorld, "the race menu: as before, ahead of a talk");
	const UInt32 credits[] = {game::kMenuIdCredits};
	Check(MenuFacingFor(credits, 1) == MenuFacingKind::NotWorld, "the credits: as before");
}

void TestOpensBeforeBody() {
	std::printf("A menu opens in front of the body, not the head\n");
	Run r;
	r.Hold(0.0f, 2.0f);
	r.Turn(0.0f, 30.0f, 0.3f);  // a look aside, not held
	r.Open();
	r.Frame(Head(30.0f, -20.0f));  // and down
	Check(r.last.placed && r.last.anchor, "placed the frame it opens");
	Check(Near(YawOf(r.held), 0.0f, 1.0f), "along the body's heading, not the head's 30 degrees");
	Check(Near(r.last.headYawDegrees, 30.0f, 0.5f) && Near(r.last.bodyYawDegrees, 0.0f, 1.0f),
	      "the verdict says the head's and the body's");
	Check(Near(r.held.m[1][2], 0.0f, 1e-5f) && Near(r.held.m[1][1], 1.0f, 1e-5f), "level: no pitch, no roll");
	Check(Near(r.held.m[1][3], kEyesY, 1e-5f), "at the eyes' height, as before");
	Check(Near(r.held.m[0][3], 0.0f, 1e-5f) && Near(r.held.m[2][3], 0.0f, 1e-5f), "from the eyes");
	const int handed = r.handed;
	r.Hold(30.0f, 0.5f, -20.0f);
	Check(r.handed == handed, "standing still: nothing handed over again");
}

void TestGlance() {
	std::printf("A glance does not move it\n");
	Run r;
	r.Hold(0.0f, 1.0f);
	r.Open();
	r.Frame(Head(0.0f));
	const int handed = r.handed;
	r.Turn(0.0f, 25.0f, 0.2f);
	r.Hold(25.0f, 0.6f);
	r.Turn(25.0f, 0.0f, 0.2f);
	r.Hold(0.0f, 1.0f);
	Check(r.handed == handed && !r.began, "looked 25 degrees aside for 0.6 s and back: it stood");
}

void TestTurnGlides() {
	std::printf("A turn held takes it along, gliding, its height kept\n");
	Run r;
	r.Hold(0.0f, 1.0f);
	r.Open();
	r.Frame(Head(0.0f));
	r.Turn(0.0f, 90.0f, 0.5f);
	const int handedAtTurn = r.handed;
	r.Hold(90.0f, 0.1f);
	Check(r.began, "the body past the follow: the glide begins");
	Check(r.handed > handedAtTurn, "a step of it handed over");
	const float early = YawOf(r.held);
	Check(early > 0.5f && early < 80.0f, "part of the way after a few frames, not at once");
	r.Hold(90.0f, 4.0f, 0.0f, kEyesY + 0.2f);  // and stood up meanwhile
	Check(r.ended, "settled in front of the body");
	// The torso settles within 2 degrees of the head, the glide within
	// kViewSettledDegrees of the torso as it settles.
	Check(Near(YawOf(r.held), r.last.bodyYawDegrees, kViewSettledDegrees + 1.5f) &&
	          Near(YawOf(r.held), 90.0f, 6.0f),
	      "the body's heading, within the settle");
	Check(Near(r.held.m[1][3], kEyesY, 1e-5f), "its height as it was placed: sideways only");
	Check(Near(r.held.m[1][2], 0.0f, 1e-5f), "still level");
	const int handed = r.handed;
	r.Hold(90.0f, 1.0f, 0.0f, kEyesY + 0.2f);
	Check(r.handed == handed, "settled: nothing handed over");
}

void TestLaserHolds() {
	std::printf("A laser on it holds it\n");
	Run r;
	r.Hold(0.0f, 1.0f);
	r.Open();
	r.Frame(Head(0.0f));
	const int handed = r.handed;
	r.in.laserOn = true;
	r.Turn(0.0f, 90.0f, 0.5f);
	r.Hold(90.0f, 2.0f);
	Check(r.handed == handed && Near(YawOf(r.held), 0.0f, 1e-3f), "turned with the beam on it: it stood");
	r.in.laserOn = false;
	r.Hold(90.0f, 4.0f);
	Check(Near(YawOf(r.held), 90.0f, 6.0f), "the beam gone: it came along");
	r.in.laserOn = true;
	r.Turn(90.0f, 0.0f, 0.5f);
	r.Hold(0.0f, 0.3f);
	const float mid = YawOf(r.held);
	r.Hold(0.0f, 1.0f);
	Check(Near(YawOf(r.held), mid, 1e-3f), "the beam back on mid-way: the glide stops where it is");
}

void TestOffAndOwnPlaces() {
	std::printf("Off, and places of their own\n");
	{
		Run r;
		r.set.enabled = false;
		r.Open();
		r.Hold(0.0f, 0.5f);
		r.Turn(0.0f, 90.0f, 0.5f);
		r.Hold(90.0f, 2.0f);
		Check(r.handed == 0, "MenusFaceBody=0: nothing handed over, the layer's own placing as before");
	}
	{
		Run r;
		r.in.menusInRoom = false;
		r.Open();
		r.Hold(0.0f, 0.5f);
		Check(r.handed == 0, "menus on the cinema screen or the HUD on the head: nothing");
	}
	for (MenuFacingKind kind : {MenuFacingKind::OwnPlace, MenuFacingKind::NotWorld, MenuFacingKind::None}) {
		Run r;
		r.in.kind = kind;
		r.Open();
		r.Hold(0.0f, 0.5f);
		Check(r.handed == 0, kind == MenuFacingKind::OwnPlace   ? "a conversation's menus: nothing"
		                     : kind == MenuFacingKind::NotWorld ? "the main menu's: nothing"
		                                                        : "no menu: nothing");
	}
	{
		Run r;
		r.Hold(0.0f, 1.0f);
		r.in.ownPlace = true;  // the container's panel, the tablet, the wrist
		r.Open();
		r.Hold(0.0f, 0.5f);
		Check(r.handed == 0, "on a place of its own: nothing");
		r.in.ownPlace = false;
		r.Frame(Head(0.0f));
		Check(r.last.placed, "back on the big quad with the menu still up: placed afresh");
	}
	{
		Run r;
		r.Open();
		r.Hold(0.0f, 0.5f);
		Check(r.handed == 1, "while it is up: placed once");
		r.in.menuUp = false;
		r.Hold(0.0f, 0.5f);
		Check(r.handed == 1, "closed: nothing");
		r.Turn(0.0f, 90.0f, 0.5f);
		r.Hold(90.0f, 4.0f);  // the body turned with the menus shut
		r.Open();
		r.Frame(Head(90.0f));
		Check(r.last.placed && Near(YawOf(r.held), r.last.bodyYawDegrees, 0.01f) && Near(YawOf(r.held), 90.0f, 3.0f),
	      "the next opens before the body as it is then");
	}
}

void TestFollowSettings() {
	std::printf("The follow's settings\n");
	{
		Run r;
		r.set.followDegrees = 0.0f;
		r.Open();
		r.Frame(Head(0.0f));
		r.Turn(0.0f, 90.0f, 0.5f);
		r.Hold(90.0f, 3.0f);
		Check(r.handed == 1 && Near(YawOf(r.held), 0.0f, 1e-3f), "MenuFollowDegrees=0: it stands where it opened");
	}
	{
		Run r;
		r.set.followSeconds = 0.0f;
		r.Open();
		r.Frame(Head(0.0f));
		r.Turn(0.0f, 90.0f, 0.5f);
		r.Hold(90.0f, 0.1f);
		const float jumped = YawOf(r.held);
		Check(r.began && r.ended && jumped > 30.0f, "MenuFollowSeconds=0: taken to the body at once");
	}
}

void TestRecentreAndEdges() {
	std::printf("The recenter, a new menu, no head\n");
	Run r;
	r.Hold(0.0f, 1.0f);
	r.Open();
	r.Frame(Head(0.0f));
	r.Turn(0.0f, 20.0f, 0.3f);
	r.Hold(20.0f, 4.0f);  // the body follows, under the follow's 30 degrees
	Check(Near(YawOf(r.held), 0.0f, 1e-3f), "20 degrees: it stood");
	r.in.recenterSerial = 1;
	r.Frame(Head(20.0f));
	Check(r.last.placed && Near(YawOf(r.held), r.last.bodyYawDegrees, 0.01f) && Near(YawOf(r.held), 20.0f, 3.0f),
	      "the recenter: placed afresh before the body");
	r.Hold(20.0f, 0.2f);
	r.in.opened = true;  // another menu opened as this one closed, in one frame
	r.Frame(Head(20.0f));
	Check(r.last.placed, "a menu opening: placed afresh");
	r.Hold(20.0f, 0.2f);
	r.in.menuUp = false;
	r.Frame(Head(20.0f));
	r.in.menuUp = true;  // menu mode back, nobody saying so
	r.Frame(Head(20.0f));
	Check(r.last.placed, "the menu mode's own start: placed afresh");
	const int handed = r.handed;
	r.in.haveHead = false;
	r.Hold(80.0f, 1.0f);
	Check(r.handed == handed && !r.last.placed, "no head: kept as it is");
	r.in.haveHead = true;
	r.Frame(Head(20.0f));
	Check(r.handed == handed, "the head back where it was: nothing");
}

void TestBodyOnItsOwn() {
	std::printf("The body's heading is the headset's, drawn body or not\n");
	Run r;
	r.in.menuUp = false;
	r.Hold(0.0f, 1.0f);
	r.Frame(Head(0.0f, -85.0f));
	Check(Near(r.last.bodyYawDegrees, 0.0f, 1.0f), "bowed nearly straight down: the heading kept");
	r.in.moving = true;
	r.Turn(0.0f, 30.0f, 0.4f);
	r.Hold(30.0f, 5.0f);
	Check(Near(r.last.bodyYawDegrees, 30.0f, 0.5f), "walking: the body follows the head");
	Check(r.handed == 0, "no menu: nothing handed over");
	float lx = 0.0f, lz = 0.0f, qx = 0.0f, qz = 0.0f;
	const openvr::HmdMatrix34 h = Head(40.0f, 30.0f);
	LevelHeadingOf(NiPoint3{-h.m[0][2], -h.m[1][2], -h.m[2][2]}, NiPoint3{h.m[0][1], h.m[1][1], h.m[2][1]}, lx, lz);
	const float half = 20.0f * math::kDegreesToRadians;
	const float pitchHalf = 15.0f * math::kDegreesToRadians;
	const Quaternion yaw{0.0f, std::sin(half), 0.0f, std::cos(half)};
	const Quaternion pitch{std::sin(pitchHalf), 0.0f, 0.0f, std::cos(pitchHalf)};
	LevelHeading(yaw * pitch, qx, qz);
	Check(Near(lx, qx, 1e-4f) && Near(lz, qz, 1e-4f), "the matrix's level heading is the quaternion's");
}

}  // namespace

int main() {
	TestKinds();
	TestOpensBeforeBody();
	TestGlance();
	TestTurnGlides();
	TestLaserHolds();
	TestOffAndOwnPlaces();
	TestFollowSettings();
	TestRecentreAndEdges();
	TestBodyOnItsOwn();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("All checks passed\n");
	return 0;
}
