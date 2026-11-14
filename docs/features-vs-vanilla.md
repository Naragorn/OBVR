# OBVR features compared with vanilla Oblivion

What OBVR's Full VR does, set against what the unmodded game (2006, PC)
offers for the same thing. It is meant for the onboarding (onboarding-spec.md
3.3): a feature the game has no form of is one nobody finds on their own, so it
needs a hint, the controls card or the guide.

**Kind**
- **new**: the game has nothing like it.
- **by hand**: the game has it behind a key or a menu, and OBVR does it with the
  hands or the body.
- **changed**: the game has it, and OBVR changes how it works.
- **comfort**: VR comfort; the game has no need of it.

**Source of the vanilla column**
- **[U]**: read on UESP (Oblivion:Block, Oblivion:Hand_to_Hand,
  Oblivion:Controls, read 2026-11-14).
- **[C]**: the game has no control for it: it is missing from UESP's full
  control list (Oblivion:Controls), so there is no way to do it on demand.
- **[?]**: from knowledge of the game, not checked against a source.

The OBVR column names the INI key (OBVR.ini). A key's default is what ships.

## Melee combat

| Feature | OBVR | Vanilla | Kind |
|---|---|---|---|
| Swinging | The swing of the controller is the attack. Speed and length decide light or power (`MotionHits`, `MotionHitsByTouch`, `PowerStrikeSpeed`). A hit lands when the blade touches. | Click to attack, hold for a power attack, and the direction you walk picks which power attack [U] | by hand |
| Shield bash | Drive the shield into someone: they stagger, and it costs fatigue (`ShieldBash`, `ShieldBashFatigue`). Any time, at any skill. | Only from Block Expert on: a 25 % chance of a knockback counterattack when blocking with a shield. Master adds 5 % to disarm. It is automatic, not on demand [U] | new |
| Kick | Flick the weapon hand's stick up: the leg kicks (leg IK). The damage is the engine's fist (`Kick`, `KickFatigue`). | None [C] | new |
| Shove | Push someone away with a hand or the held weapon. It counts as a hit (`Shove`, `ShoveCountsAsHit`). | None [C] | new |
| Off-hand strikes | A punch with the free hand while holding a weapon (`OffHandStrikes`). | One weapon or the fists, never both [?] | new |
| Pommel strike | Hit with the pommel (`PommelStrikes`). | None [C] | new |
| Half-swording, choke-up | Grip the blade with the second hand: a short, strong stab. Blunt weapons are gripped higher (`HalfSwording`, `ChokeUp`). | None [C] | new |
| Two-hand grip | The second hand on the grip steadies a weapon and slides along it (`TwoHandGrip`, `TwoHandOneHanders`, `TwoHandSlide`). | Two-handers are always two-handed. There is no grip choice [?] | new |
| Two-hander in one hand | Possible, but heavier, slower and costs fatigue (`OneHandTwoHander*`). | Not possible [?] | new |
| Weapon weight | Heavy weapons lag behind the hand and keep swinging (`WeaponWeight`, `WeaponSwingThrough`). | The animation is the same speed for every weight class [?] | new |
| Blocking | Hold the weapon or the shield in the blow's way. The angle and reach decide (`BlockByAngle`, `BlockShieldDegrees`). | Hold a key; every blow from the front is blocked, scaled by the Block skill [U] | by hand |
| Parry | Blade on blade stops the blow and costs fatigue (`WeaponParries`, `ParryFatigue`). | Blocking only [U] | new |
| Torch blocks and parries | A raised torch blocks, and a torch hit staggers (`TorchBlocks`). | No blocking with the torch [?] | new |
| Blade in the body | A stab goes in by armour and skin, sticks, can be pulled out or skewer (`BladeEntry*`, `BladeSticks*`). Cut marks and blood. | A hit is a number. Nothing goes in [?] | new |
| Weapon stops at walls and bodies | The blade stops at walls and people, with hit-stop (`WeaponStopsAtWalls`, `WeaponHitStop`, `BladeSlidesOffPeople`). | The animation goes through [?] | new |
| Dismemberment | Head, hands, forearms, arms and lower legs come off by a fast enough cut. Wound caps, sound, blood, a bleeding neck (`Dismemberment*`, `NeckBleeds`). | None [?] | new |
| Knockdown by force | Blunt blows stagger at 3 m/s and knock down at 6 m/s. The force grows with speed (`StaggerSpeed`, `KnockdownSpeed`). | Chances on certain power attacks: e.g. Hand to Hand Expert has a 5 % knockdown on the backward power attack [U]. OBVR turns that off (`VanillaKnockdown=0`). | changed |
| Stagger to the side hit | They stagger away from the side the blow came from (`StaggerSides`). | One stagger animation [?] | changed |
| Skills train by striking | Every VR strike is an engine hit with its own skill (blade, blunt, hand to hand). Cuts give a bonus (`StrikesTrainSkills`). | Each landed hit trains the weapon's skill, e.g. Hand to Hand 0.6 XP per hit [U] | same, kept |
| Gear wear, gear knocked off | Wear multipliers. The player's helmet or shield can fly off (`ArmorWearMult`, `PlayerGearBreaksOff`). | Gear wears down [?], it never comes off [?] | changed |
| Combat ring | Melee enemies take turns: one attacks, the others wait around you (`CombatRing`, `MeleeAttackersAtOnce=1`). | Everyone attacks at once [?] | changed |
| Enemy attack tempo | Enemies attack slower, so a person can block with the body (`EnemyAttackSpeed=75`). | Engine speed [?] | changed |
| Slow approach | Enemies slow down in the last 5 m (`SlowApproach`). | Run straight in [?] | changed |
| Combat reach | 85 units, so the arm reaches what the hand touches (`CombatReach`). | Engine reach [?] | changed |
| Yield | Raise both open hands, palms towards the enemy (`Yield`). | Hold Block and activate the enemy [U] | by hand |
| Enemy health bars | Over an injured enemy you can see, fading after 4 s (`EnemyHealthBars`). | The target's health is on the HUD [?] | changed |

## Ranged combat and magic

| Feature | OBVR | Vanilla | Kind |
|---|---|---|---|
| Bow | Take an arrow from the quiver at the hip, nock it, draw the string with the hand (`BowByHand`, `QuiverAtHips`). | Click and hold [U] | by hand |
| Arrow as a weapon | Stab with an arrow held in the hand (`ArrowStabs`). | None [C] | new |
| Throwing | Throw weapons, torches and objects. They hit people and make noise (`ThrowHits`, `ThrowNoise`). | Grab (Z) carries an object. Attack while using Telekinesis pushes it [U]. You cannot throw [C] | new |
| Casting | The spell leaves the casting hand (`SpellFromCastingHand`). | From the crosshair [?] | by hand |
| Picking a spell | Spell ring on the stick (`SpellRing`). | Spell menu or hotkeys 1-8 [U] | by hand |
| Aiming | With the head, and the hand if you want (`AimFollowsGaze`, `AimWithHand`). | With the mouse [U] | by hand |

## Hands in the world

| Feature | OBVR | Vanilla | Kind |
|---|---|---|---|
| Picking up | Grip the item and bring it to your chest (`StowAtBody`, `TakeOnlyByHand`). | Activate (Space) [U] | by hand |
| Holding and carrying | Hold any small object in the hand, pull it from afar, put it down where you like (`PullToHand`, `AttachSmallObjects`). | Grab (Z) carries it in front of you [U] | by hand |
| Holsters | Four spots on the body (hips, back). Draw by gripping, sheathe by letting go (`Holsters`, `HolsterLabels`). | Draw and sheathe with F [U] | by hand |
| Drawing at once | The weapon is in the hand at once instead of after the draw animation (`TakeDrawsAtOnce`). | Draw animation [?] | changed |
| Grab weapons from the world | A weapon on the ground gripped by its handle is equipped (`GrabEquipsWeapons`, `GrabFromInventory`). | Pick it up, then equip it in the inventory [U] | by hand |
| Torch | Grip, drop it burning, hold it with a two-hander (`GrabEquipsTorches`, `TorchDrop`, `TorchWithTwoHander`). | Equip it in the inventory. Torch and two-hander do not go together [?] | new |
| Potion belt | Potions at the body spots, drink them by hand (`PotionBelt`). | Hotkeys or the inventory [U] | new |
| Eat and drink | Let go of food or a potion at your mouth (`EatAtMouth`). | Use it in the inventory [U] | by hand |
| Doors | Grip the handle, pull or push (`DoorsByGrip`). Containers and doors open as the hand comes near (`ReachOpens`). | Activate [U] | by hand |
| Containers | The panel stands in the world next to the chest (`ContainerInWorld`). | Full-screen menu [?] | changed |
| Lockpicking | Point at a pin, push the stick up (`LockpickByHand`). | Lockpick menu with the mouse [?] | by hand |
| Grab people | Grab and hold someone; a ragdoll can be dragged and thrown (`OffHandGrabs`, `GrabRagdoll`). | Grab (Z) drags bodies [?]. Living people cannot be held [?] | new |
| Lead by the hand | Take an NPC by the hand and lead them (`LeadByHand`). | None [C] | new |
| Slap, middle finger | Slap someone: disposition drops. The middle finger: disposition drops (`SlapDisposition`, `MiddleFinger`). | None [C] | new |
| Fists | Close your hand to make a fist (`Fists`). | Fists when nothing is equipped [?] | by hand |
| Push the world | The body and held things push objects and people away (`PushWorld`, `HeldObjectsPush`, `SoftPush`). | The player capsule pushes nothing [?] | new |
| Haptics | Rumble in menus, on contact and on hits (`MenuHaptics`, `HitHaptics`). | Gamepad rumble [?] | changed |

## Moving and the body

| Feature | OBVR | Vanilla | Kind |
|---|---|---|---|
| Analog walking and sprint | Walk speed follows the stick. Pushing it all the way sprints at 1.4x and costs fatigue (`AnalogWalk`, `SprintFactor`). | PC: walk or run (Shift). A joystick walks analog. No sprint [U] | new (sprint) |
| Walk direction | Head, hand or a blend of both (`WalkDirection`). | Where the camera looks [?] | comfort |
| Arm-swing running | Swing your arms to run (`ArmSwingRun`, off by default). | None | new |
| Teleport | Optional, with a glide and a vignette (`Teleport*`). | None | comfort |
| Snap and smooth turning | Snap 45° with a vignette (`SnapTurning`, `SnapTurnVignette`). | Mouse turn | comfort |
| Room scale | Walk in the room. The body follows (`RoomScaleBody`). | None | new |
| Crouch to sneak | Duck in the room to sneak (`CrouchSneak`). | Ctrl [U] | by hand |
| Sneak indicator | The edge of the view darkens while you are hidden, and clears when you are seen (`SneakIndicator=vignette`). | Eye icon in the crosshair [?] | changed |
| Visible body | Your own body with IK for arms, torso and legs, the head hidden (`[Body] IK`). | First person: arms only. Third person: the whole animated body [?] | new |
| Hands | Hand bones on the controllers, finger tracking (`PinHands`, `FingerTracking`). | Animated arms [?] | new |
| Left-handed | The skeleton is mirrored (`LeftHanded`). | Weapon always in the right hand [?] | new |
| Exhaustion | Out of fatigue, you get slow and heavy instead of collapsing (`Exhaustion`). | At 0 fatigue the actor falls over [?] | changed |
| No stagger or knockdown for the player | The view stays still (`NoPlayerStagger`, `NoPlayerKnockdown`). | The player is staggered and knocked down [?] | comfort |
| No hit blur | (`NoHitBlur`) | Blur on a hit [?] | comfort |
| Animation vignette | Vignette while the game animates the camera (`AnimationVignette`). | None | comfort |
| Riding | Horse bob reduced (`HorseBob=0.3`). | Full bob [?] | comfort |
| Death | The view stays still and turns grey (`DeathViewStill`, `DeathGrey`). | The camera falls with the body [?] | comfort |

## Menus, HUD and talking

| Feature | OBVR | Vanilla | Kind |
|---|---|---|---|
| HUD on the hands | Health, magicka and fatigue bars, the spell and the weapon on the wrists. The compass is in the sky (`[HandHud]`). | Fixed screen HUD [?] | changed |
| Menus | Menus on a panel in the world, laser pointer and poke (`Menus=world`, `LaserBeam`). | Full screen with the mouse [U] | changed |
| Quick menu | Hold a button: a ring with the menu pages (`QuickMenu`). | Tab, F1-F4 [U] | by hand |
| Talking | First person, facing the speaker, the panel beside them (`DialogFirstPerson`). | The camera zooms onto the face [?] | changed |
| Keyboard | VR keyboard for names (`VrKeyboard`). | Physical keyboard | new |
| Character creation | A mirror view at 0.9 m (`ChargenView`). | Face menu [?] | changed |
| Tooltips | Names over the thing your hand reaches (`ReachTooltip`). | At the crosshair [?] | changed |
| Button layout | Free button bindings (`OBVR_Controls.ini`). | Rebinding in the options [U] | same |

## OBVR's own helpers

The game has nothing like these.

| Feature | Key |
|---|---|
| Start guide in the dome, with music | `[Onboarding] StartGuide`, `GuideDome` |
| Hints in play, ten of them (yield, draw, shield bash, parry, kick, stow, eat, potion belt, throw, door) | `[Onboarding] Hints` |
| Settings menu in the headset (Insert) | `[SettingsMenu]` |
| Self-check and doctor | `[SelfCheck]` |
| Update check | `[Updates]` |
| Performance ring; it saves slow stretches by itself | `[Performance] Ring`, `AutoSave*` |

## Rendering (technical, no gameplay)

Stereo per eye, Single Pass Stereo, head tracking, crosshair at depth, steady
leaf billboards and distant trees, grass all around, steady water reflections,
dynamic resolution. The game renders one flat picture. There is nothing to
learn here, so none of it needs a hint.

## When vanilla gives it: the skill levels

Vanilla hands out several combat moves by skill level (mastery perks). The
levels: Novice 0-24, Apprentice 25-49, Journeyman 50-74, Expert 75-99,
Master 100 [U: Oblivion:Skills]. OBVR's own moves come at any skill. Below,
each OBVR feature that has a vanilla counterpart tied to a skill level, then
the ones vanilla has at any level. Read on UESP 2026-11-14 (Oblivion:Block,
Blade, Blunt, Hand_to_Hand, Marksman, Acrobatics, Athletics, Sneak, Skills,
Aggression, Controls, Responsibility).

| OBVR feature | Vanilla counterpart | From which skill level in vanilla | OBVR |
|---|---|---|---|
| Shield bash | Knockback counterattack: blocking with a shield has a 25 % chance to stagger the attacker; Master: +5 % disarm with it. Automatic, not on demand. (Oblivion Remastered changed it: an on-demand shield bash from Block Expert.) | Block **Expert (75)** | Any skill, on demand; costs fatigue |
| Hand-to-hand block, shove | Blocking with the fists makes the attacker recoil; H2H Expert: a chance of a knockback when blocking; Master: a disarm with it | Block / H2H **Journeyman (50)**, Expert, Master | Shove at any skill; fists block only by the engine |
| Blocking without fatigue | Novice: blocking costs fatigue; Apprentice: free; Journeyman: the shield or weapon no longer wears | Block **Apprentice (25)**, Journeyman | The parry and the shield bash cost fatigue at any skill (`ParryFatigue`, `ShieldBashFatigue`); the engine's block perks stay [?] |
| Power attacks | Novice: a basic power attack in every direction; Apprentice: the standing one stronger; Journeyman: left and right (5 % disarm); Expert: backward (5 % knockdown); Master: forward (5 % paralysis) | Blade / Blunt / H2H, Novice to **Master** | Power by swing speed and length at any skill; the direction from the swing's path is handed to the engine (`PowerAttackGroup`). Whether the engine then rolls its disarm, knockdown and paralysis perks for a VR swing is **not checked** [?] |
| Knockdown | 5 % on the backward power attack; Marksman: 5 % on any arrow hit | Blade / Blunt / H2H **Expert (75)**, Marksman **Expert (75)** | By the blow's force at any skill: stagger 3 m/s, knockdown 6 m/s. Vanilla's knockdown roll is off (`VanillaKnockdown=0`) |
| Disarm | 5 % on the side power attacks; Block Master with the shield | **Journeyman (50)**, Block **Master (100)** | No own disarm; the engine's [?] (see power attacks) |
| Paralysis | 5 % on the forward power attack; Marksman 5 % on a hit | **Master (100)** | No own paralysis; the engine's [?] |
| Dodge roll | Block and jump in a direction | Acrobatics **Journeyman (50)** | Same rule: the raised hand (block) and a flick up (controls-spec.md) |
| Attacks in the air | Novice: none; Apprentice: normal attacks; Journeyman: power attacks | Acrobatics **Apprentice (25)** | Swings in the air not changed [?] |
| Bow zoom | Hold block while drawing | Marksman **Journeyman (50)** | Not mapped in VR [?]; the bow is drawn by hand at any skill |
| Drawing a bow without fatigue | Novice: drawing costs fatigue; Apprentice: free | Marksman **Apprentice (25)** | Engine's, unchanged [?] |
| Sprint | None; Athletics Master regenerates fatigue whether running or not | (Athletics Master is the nearest) | Sprint at 1.4x at any skill, costs fatigue (`SprintFactor`) |
| Sneak attack | 4x (Novice), 6x (Apprentice) melee damage undetected; Master ignores armour | Sneak, every level | Crouch to sneak; the engine's bonus applies to VR strikes [?] |

At any skill in vanilla, and in OBVR alike:

- **Yield**: block and activate at any skill. Whether it works is the NPC's
  disposition alone, and creatures never accept one [U: Aggression,
  Controls]. Bandits have responsibility 0 (97 of the 98 bandits and
  marauders in Oblivion.esm, read 2026-11-14) and a low disposition: a
  yield to them is wasted. So OBVR's hint shows only against a guard (class
  flag) or an NPC of responsibility 50 or more. 1,658 of Oblivion.esm's NPCs
  have 50 or more, 824 under 50.
- **Skill training**: every landed hit trains its weapon's skill, OBVR's
  strikes as well (`StrikesTrainSkills`).

Not in vanilla at any level: kick, pommel strike, half-swording and
choke-up, two-hand grip, a two-hander in one hand, parry, torch block,
throwing, blade in the body, dismemberment, grabbing people, leading by the
hand, the potion belt, the arrow stab, slap and middle finger.

## What follows for the onboarding

These are the "new" features a player cannot find, because the game has no
key, menu or example for them. Each one is listed with how it is taught today.

| Feature | Taught by |
|---|---|
| Shield bash | Hint, controls card |
| Kick | Hint, controls card |
| Drawing at the body (holsters) | Hint, holster labels |
| Stowing at the body | Hint, stow spot |
| Eating at the mouth | Hint |
| Doors by grip | Hint |
| Shove, slap, middle finger | Controls card only |
| Half-swording, choke-up, pommel, two-hand grip | Not taught |
| Parry | Hint (since 2026-11-14) |
| Torch block | Not taught |
| Throwing | Hint (since 2026-11-14) |
| Lead by the hand | Not taught |
| Potion belt | Hint (since 2026-11-14): a potion held, or hurt in a fight with one kept |
| Arrow stab | Not taught |
| Grab people | Not taught |
| Yield gesture | Hint (since 2026-11-14), only against a guard or a lawful NPC; controls card (README) |

Parry and throwing got their hints on 2026-11-14 (Nadi: "ja go"). Half-swording
is for experts and fits the guide better than a hint.
