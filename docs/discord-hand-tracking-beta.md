# The hand-tracking beta on Discord - the texts

Written 2026-10-15 for the first closed beta of Full VR (standing with motion
controllers). The build is handed out in the Discord channel only; nothing on
GitHub or Nexus (docs/onboarding-spec.md section 5). Both texts are public
texts: simple, short, friendly.

## The Nexus comment

> **Full VR beta on Discord.** Standing with motion controllers (swords in
> your hand, blocking, grabbing, teleport) is in a closed beta. The beta build
> is handed out in our Discord channel only, not here yet - join if you want to
> test it, Index or Quest controllers: https://discord.com/channels/747967102895390741/1547539304786436228
> The download here stays the seated version.

## The Discord post

> **OBVR Full VR - beta 1** (OBVR-0.3.0-beta1.zip, attached)
>
> This is the first build of Full VR for testing: play Oblivion standing (or
> seated) with motion controllers. It is playable and has rough edges. Tested
> so far on one setup only (Valve Index controllers, Windows 11 and Linux with
> Proton), so your reports are the point of this. The log says "OBVR 0.3.0".
>
> **What is in it**
> - Draw your weapon by reaching to your hip (two-handers over the shoulder,
>   the bow with the other hand behind the shoulder).
> - Swings hit where the blade touches; a long, fast swing is a power attack.
>   The blade stops at walls and bodies, parries meet blade to blade.
> - Block with a raised shield or your blade - the angle and the reach count.
> - Grab, hold and throw things; grab people and drag them; shove, slap, lead
>   NPCs by the hand, insult them with your middle finger.
> - Teleport or walk with the stick, snap or smooth turning, crouch to sneak.
> - The HUD on your wrist, the menus on the other wrist with a laser, a hotkey
>   ring at your hand, your body with arms that follow your hands.
> - Enemy health bars over their heads, fighters that walk around you one at a
>   time, no stagger or knockdown thrown at your head.
>
> **Install**
> 1. Install the normal OBVR as the README says (xOBSE, DXVK gplasync,
>    MenuQue, Steam overlay off): https://github.com/Naragorn/OBVR#installing
> 2. Unzip the beta zip from this channel into Oblivion's `Data` folder. It
>    replaces `OBVR.dll`, `OBVR.ini` and the menu XML files. Keep a copy of
>    your old `OBVR.ini` if you tuned it. The new INI starts with Full VR on,
>    snap turning on (eased, with a vignette) and a short grab reach; all of
>    it is in the settings menu (Insert) and in the INI.
> 3. Start SteamVR, then the game with `obse_loader.exe`. The first start asks
>    which way you want to play: pick **Full VR**. A short guide follows:
>    stand straight, arms out like a T and pull both triggers (that sets
>    forward and fits the weapon spots to your height), your weapon hand, the
>    controls card, the comfort settings. Playing seated? "I'm seated" there
>    switches to gamepad, mouse and keyboard (Insert, "Full VR", switches back). Once you stand in the game, adjust
>    your hands when it offers it (hold a grip, move the controller into the
>    hand, let go).
>
> **Controls**: the table in the README, section "Controls in Full VR" -
> https://github.com/Naragorn/OBVR#controls-in-full-vr - and in the game under
> Insert > Help > Show the controls. Quest: click the right stick's button for
> the hotkey ring, click again on a slot; hold it for the rest menu (there is
> no trackpad, and no journal key - use the Tab menus). Left-handed: `[Hands] LeftHanded=1`
> or the settings row.
>
> **Known rough edges** (you do not need to report these):
> - Quest controllers are untested by us. Index is what we play.
> - New game: the face editor shows as a flat picture, and the name is typed
>   on the keyboard.
> - The hotkey ring: on the Index hold the trackpad, on the Quest click the
>   stick and click again on a slot. Setting a hotkey from the inventory with
>   the ring is fixed in this build but was not yet seen in a headset.
> - The yield gesture (both palms forward) is unreliable. The bow's block
>   flickers.
> - Rarely a hit still throws you down and the view flickers until you stand.
> - The wrists look pinched when you turn the controllers outwards, and for
>   the first seconds after a load.
> - Many settings (31 pages). The Comfort page after the first start has the
>   important ones.
>
> **Please report** in this channel, with:
> - `OBVR.log` and `OBVR.log.prev` from your Oblivion folder - without them we
>   can rarely fix anything.
> - Your headset and controllers, GPU, Windows or Linux/Proton.
> - What you did, what you saw, what you expected.
> - If the game crashed: install Cobb Crash Logger (Nexus) and attach its log;
>   OBVR has no crash handler of its own. If it froze: did the monitor still
>   move, did SteamVR still track?
>
> Please keep the build in this channel - it is not released yet.

## The Discord post for beta 5 (2026-10-19)

Packaged 2026-10-19 from eaf578c as dist/OBVR-0.3.0-beta5.zip (tools/package-release.py
--tag=-beta5); posted by Nadi 2026-10-19 evening after his headset round (the people's
performance "super", torches normal, the horse's two open bugs in controls-spec). Beta 4 was the last build in the channel
(2026-10-16). Everything below was measured in the harness; what was not yet seen in a
headset is said so in the specs, not here.

> **OBVR Full VR - beta 5** (OBVR-0.3.0-beta5.zip, attached)
>
> Beta 4 plus what came out of your reports since. Same install as before: unzip
> over `Data`, keep your tuned `OBVR.ini` if you have one. The log says
> "OBVR 0.3.0".
>
> **What changed**
> - People in view cost far less. Most of the frame time with NPCs around was
>   our own diagnostics reading the mesh data back every frame - that is gone.
>   The opening dungeon with the Blades and Jauffre's house now fit the frame on
>   our machine. If you still stutter around people, tell us where and at which
>   refresh rate.
> - Frames are paced to the headset's vsync: a quick frame no longer slips ahead
>   and drops the next one.
> - Left stick click: the spell ring at your left hand (your spellbook). Holding
>   the stick still runs. Right stick click: the hotkey ring, as before.
> - The tutorial cards in the opening dungeon say what to do with the
>   controllers, not with the keyboard.
> - Robes and suits that carry the hands in the body mesh (Black Hand Robe,
>   Shrouded Armor) no longer show a second pair of hands.
> - On a horse: no hover, and the jumps of the view are gone. Riding is still
>   rough: looking left or right while riding does not turn the view, and the
>   camera moves with the horse more than a headset likes. Known, being worked
>   on.
> - The water seam at the horizon when you turn your head is gone.
> - Stowing a weapon at your chest measures the drawn hand, not the controller,
>   so it works with any hand offset. The corpse and container panel no longer
>   flips when you look steeply down.
> - Settings: pitch rows for both hands and a stick-turn speed row; the controls
>   card names doors, chests and the stow ring.
> - The camera that starts circling you after idling stays off in VR.
> - Yield (both palms forward): the weapon hand's grip no longer blocks it; an
>   open palm counts.
> - A crash two seconds after the start with some mods (an actor read before its
>   base form was linked) is fixed.
>
> **Still rough**: wrists pinch when you turn the controllers outwards; smooth
> turn speed follows the mouse sensitivity; the yield still wants both palms
> squarely forward.
>
> **Reports** as before: `OBVR.log` and `OBVR.log.prev`, headset, controllers,
> GPU, Windows or Linux/Proton, what you did and saw. For stutter: set
> `[Performance] Enabled=1` and `StartAfterSeconds=60` in OBVR.ini, play the
> scene for a minute and a half, and attach the `OBVR-performance-1` folder from
> your Oblivion directory.
