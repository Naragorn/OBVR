# Left-handed play - everything that has to mirror

The tester, 2026-09-30: "links händer: erstelle eine spec die alles sammelt was gespiegelt werden muss für links händer ... und generell eine option in den settings wo was sein soll. denke da an 2 händer rechte hand rechte schulter zum ziehen bei mir als rechtshänder. links händer wollen whl alles links, kann aber auch n paar geben die wollen dann mixen und matchen. soll möglich sein."

Scope: Full VR (`[Hands] Enabled=1`). The inventory below was read from the code on 2026-09-30 (file:line at that commit). Nothing in it has been tried by a left-hander in a headset.

## 1. How left-handed works today

`[Hands] LeftHanded=1` swaps the two controllers **as a whole**, before anything reads them: pose, buttons, sticks, finger curls and velocities (`vr::AssignHandRoles`, src/vr/HandInput.h:1278, called at src/camera/CameraHook.cpp:1720).
- Everything downstream works by **role**: "right" is the weapon hand, "left" the other hand, whichever controller that is.
- What hangs on a physical device - the laser beam, the wrist quads, the controller models - maps the role back with `vr::HandDeviceForRole` (HandInput.h:1289).
- The swap only happens while Full VR is active (`active &&`, CameraHook.cpp:1721). With the controllers only steering menus nothing swaps.

The consequence that matters most: **the bodies are not mirrored.** The game's weapon hangs on its right hand bone ("Bip01 R Hand"), and a left-hander's left controller drives that bone. So a left-hander holds the weapon in a right hand, drawn where their left hand is (section 2).

## 2. The hands themselves

| What | Where | Left-handed today | Status |
| --- | --- | --- | --- |
| Hand bones | CameraHook.cpp:4436-4447 (`PinAdjustableHand`) | "Bip01 R Hand" follows the left controller, "Bip01 L Hand" the right. The meshes are the game's own, unmirrored: a right hand shows on the left controller. | **Open.** See 2.1. |
| Hand calibration | CameraHook.cpp:4271-4280, 4310-4326 | The right hand's calibration (`RightHandRoll/Pitch/Yaw`, `RightHandGrip*`) is used on the left controller. Adjusting it saves into the right-handed keys. | **Gap:** switching LeftHanded on and off overwrites one profile. A second set, `LeftHanded*` or per controller, is needed. |
| Arms at rest | HandMode.cpp:178-187, HandMode.h:32 | The rest offset `restHandRight=+0.20` is not mirrored. The arms sit about 40 cm to the side. | **Gap:** mirror its x with LeftHanded. |
| Finger tracking | HandGrip.h:233, 242 | Curls come from the swapped controller. The left bone's mirror (`MirroredForLeft`) goes by bone. | OK. |
| Controller models | CameraHook.cpp:4658-4667 | Drawn at each physical controller. | OK. |

### 2.1 A left hand holding the weapon - open question

For a left-hander to see a **left** hand holding the weapon, one of these has to happen:
- **The weapon on the game's left hand bone.** Re-parent the "Weapon" node under "Bip01 L Hand" and pin the bones by physical side again. Every weapon's attach point is modelled for a right hand, so each weapon would need a mirrored local transform. The engine's own code (strikes, the Weapon node lookups, the first-person animations) expects it on the right. Large, and not tried.
- **A mirrored first-person skeleton.** Scale the first-person root by -1 on x. Mirroring flips the triangle winding, and every culling and lighting path in the engine would see it. Not tried.
- **Live with it.** The weapon hand is a right hand on the left controller. Cheapest, and it is what is built.

Decision needed before any of it is started. It is the only item here that changes what a left-hander sees rather than where things are.

## 3. Features

| Feature | Where | Left-handed today | Status |
| --- | --- | --- | --- |
| Attack, swing, power attacks, bow draw, reach back for an arrow | HandMode.cpp:232-308 | The weapon hand's, so the left controller. A power attack's direction (`ClassifyPowerSwing`, HandInput.h:276) is judged in the world: up, down, across, thrust. It needs no mirror. | OK |
| Left fist (hand-to-hand) | HandMode.cpp:310-341 | The other hand's, so the right controller. | OK |
| Block, shield | HandMode.cpp:223 | The other hand, the game's left arm, so the right controller. | OK |
| Casting (magicNode on the left palm) | CameraHook.cpp:4033-4035 | The game's left hand, so the right controller. | OK |
| Torch | "Bip01 L Hand" | The right controller. | OK |
| Two hands on a two-hander | TwoHandLogic.h, CameraHook.cpp | The other hand, the right controller, goes onto the handle. The weapon hand leads. | OK |
| Grab, throw, stow, shove, lead by hand | HandMode.cpp:501-511, CameraHook.cpp | By role, or either hand. | OK |
| The palm's direction for the item search | CameraHook.cpp:2072-2075 | `side = left ? 1 : -1` goes by role but reads the controller's own x. Swapped, the palm points the wrong way: open-palm reaching finds nothing. | **Bug** |
| Laser yaw correction | HandMode.cpp:579, 721, 769; CameraHook.cpp:602, 2064, 2143, 5422, 5580, 6480, 6488 | The sign goes by role, while the beam hangs on the physical controller. The left controller gets the right controller's yaw. | **Inconsistent.** If the yaw corrects controller geometry, it must go by physical side. |
| Laser device, crosshair hand | CameraHook.cpp:5578, 5414-5420 | Mapped back to the device. | OK |
| Wrist menu | HandMode.cpp:660-680, CameraHook.cpp:2765 | By role and x-symmetric: it goes on the weapon hand's wrist, the other hand points. | OK |
| Wrist HUD | HandMode.cpp:514, CameraHook.cpp:2768 | On the weapon hand's wrist. | OK |
| HUD on the hands | HandHud.h:95-96, CameraHook.cpp:1718-1719, 5438-5480 | Goes by **physical** controller. Bars and spell stay on the physical left, which is a left-hander's weapon hand. Weapon, effects and level-up stay on the physical right. | **Gap:** "left"/"right" should mean the other hand and the weapon hand under LeftHanded. Or add an auto/left/right per element, as the holsters have (section 4). |
| Walk direction | CameraHook.cpp:7146-7153, WalkDirection.h | The setting names physical controllers; the stick hand is known (`stickHandRight`). | OK |
| Sticks: walk, run, snap turn, sneak, jump, teleport | HandMode.cpp:404-440 | Swapped: walking on the right stick, turning and teleport on the left. | OK |
| Quick menu | HandInput.h:608, CameraHook.cpp:1205-1208, 1724 | Both paths by role, but on different hands. | Check in the headset |
| Inventory, menu B, escape | HandMode.cpp:400-431 | By role. | OK |
| **Weapon places (holsters)** | Holster.h, HolsterFit.h | Mirrored by x, and a side per place since 2026-09-30 (section 4). | **Built** |
| Stow at the body | Stow.h:68, 173 | The hand is by role; the spot's `centreRight` is not mirrored. | Minor gap |
| Activate | HandInput.h:593, 603 vs HandMode.cpp:471 | The swap does it. The `leftHanded` branch in `PlanHandControls` never runs (`in.leftHanded = false`). | Clean up the dead branch |
| Onboarding and settings texts | Onboarding.cpp:114, assets OBVR_Onboarding.xml:60, SettingsList.cpp:913 | Name physical buttons ("right stick up ... teleport", "Left B: inventory"). | **Gap:** wrong for a left-hander |
| SteamVR bindings | assets/input/*.json, OpenVRBackend.cpp:48 | Bound to physical hands; the swap comes after reading. | N/A |

## 4. Where each weapon hangs - built 2026-09-30

Every place has its own side, to mix as wanted:

| INI (`[Hands]`) | Settings row (Hands) | Values |
| --- | --- | --- |
| `HolsterOneHandSide` | One-hander side | `auto`, `right`, `left` |
| `HolsterTwoHandSide` | Two-hander side | `auto`, `right`, `left` |
| `HolsterBowSide` | Bow side | `auto`, `right`, `left` |

- **auto** is the old behaviour: a right-hander's places, mirrored with `LeftHanded=1`. A right-hander gets the one-hander at the left hip, the two-hander over the right shoulder and the bow behind the left shoulder. A left-hander gets everything the other way round.
- **right / left** put that place on that side of the body, whatever the handedness. The place's distance out to the side (`Holster*X`), its height and depth stay; only the side changes (`vr::ZoneFor`).
- The hand that draws does not change. One- and two-handers are drawn by the weapon hand, the bow by the other hand. A right-hander who sets the two-hander to `left` reaches across to the left shoulder with the right hand.
- **Fitting** ("Fit weapon places") keeps a chosen side where the hand really was (`vr::FittedSide`): set `right` and shown on the left, it becomes `left`, so a fit never puts a place on the other shoulder. `auto` stays `auto`, stored the right-handed way as before.
- Code: `vr/Holster.h` (`HolsterSide`, `ZoneFor`, `FittedSide`, covered by `holster_test` and `holster_fit_test`), `Config.cpp`, `SettingsList.cpp`, the fit's save in `CameraHook.cpp`.
- **Not tested in the game yet.** The game was running when it was built, so there was no harness run. The existing holster scenarios use the defaults (`auto`), which behave as before.

Not covered: the sheathed weapon's own model (the game's `SideWeapon` / `BackWeapon` nodes) still hangs where the game puts it, on the right-hander's side. It is seen in third person and in a mirror. Moving it with the side is a follow-up.

## 5. Order proposed

1. The palm-direction bug and the laser-yaw inconsistency: small, and they break reaching and pointing for a left-hander.
2. HUD on the hands by role (or auto/left/right per element, like the holsters).
3. A separate hand calibration for left-handed play, and the mirrored rest offset of the arms.
4. The onboarding and settings texts naming the buttons by role.
5. The sheathed weapon's model on the chosen side.
6. Decide 2.1 (a left hand holding the weapon), the only big one.

## 6. Open: a full left-handed playthrough

The tester, 2026-09-30: "notiere irgendwo in den specs dass ich mal den ganzen mod als links händer durchteste". To do, by the tester in the headset: play the whole mod once with `[Hands] LeftHanded=1`, every feature in sections 2 to 4. Each finding goes into this spec as an open bug.
