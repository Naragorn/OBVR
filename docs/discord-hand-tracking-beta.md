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

> **OBVR Full VR - beta 1**
>
> This is the first build of Full VR for testing: play Oblivion standing (or
> seated) with motion controllers. It is playable and has rough edges. Tested
> so far on one setup only (Valve Index controllers), so your reports are the
> point of this.
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
>    your old `OBVR.ini` if you tuned it.
> 3. Start SteamVR, then the game with `obse_loader.exe`. The first start asks
>    which way you want to play: pick **Full VR**. A short guide follows:
>    recenter (both stick buttons held three seconds), your weapon hand, the
>    controls card, the comfort settings. Once you stand in the game, adjust
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
> - Setting a hotkey from the inventory with the ring does not work right now
>   (the laser moves off the item).
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
