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
>    switches to gamepad, mouse and keyboard; Back takes you back. Once you stand in the game, adjust
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
> - The rings on an Index: hold the right trackpad for the hotkey ring, the
>   left trackpad for the spell ring, and let go on a slot. On other
>   controllers click the stick and click again on a slot. Setting a hotkey from the inventory with
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

## The Discord post for beta 6 (2026-10-22)

Packaged 2026-10-22 from 070ca9a as dist/OBVR-0.3.0-beta6.zip (tools/package-release.py
--tag=-beta6), the water reflections off by default (Nadi: "releasen wir die neue beta mit wasser
reflexionen einfach aus erstmal"). Not posted; Nadi posts. Beta 5 was the last build in the channel
(2026-10-19). What Nadi accepted in the headset before packaging: the distant trees, the T-pose
measuring, the seated page, the arm stretch, the Index detection, the controls card, the horse, the
left-handed controls and sheaths, the rings on the fingers, the main menu on its quad. Harness only:
the hotkey fix and the water default (water-reflections-off.txt not run yet).

> **OBVR Full VR - beta 6** (OBVR-0.3.0-beta6.zip, attached)
>
> Same install as always: unzip over `Data`. If you keep your own `OBVR.ini`, set
> `WaterReflectionMode=1` under `[Render]` yourself - see the water line below.
>
> **What changed**
> - **Water reflections are off for now.** The water keeps its waves, colour and sun, but
>   mirrors nothing - the reflections kept resetting when you turn your head and we have not
>   nailed why yet. Want them back anyway? `WaterReflectionMode=2`.
> - **Distant trees stand still** when you turn your head. They used to turn with you.
> - **New start guide:** stand in a T-pose and pull both triggers. OBVR measures your height
>   and arm span and places the weapons, quiver and stow spot for your body. Playing seated?
>   There's a button for that.
> - **Valve Index controllers are detected.** Hold the right trackpad for the hotkey ring,
>   the left trackpad for the spell ring; the stick clicks stay free. Other controllers keep
>   the stick clicks as before.
> - **Controls card** redrawn in the game's parchment style, five pages, with Index drawings
>   when you have one.
> - **Setting a hotkey** in the inventory works again - the laser no longer slides off the
>   item when you move to the slot.
> - **Horse:** looking around while riding turns the view, and the camera bobs far less
>   with the horse. `Horse bob` in the settings if you want some of it back.
> - **Left-handed play** mirrors the body: weapon in the left hand, shield and spells in the
>   right, sheaths on the other side.
> - **No more forced first person.** Chairs, falls, paralysis and the vampire feed go through
>   the game's own camera switches, so the view no longer flickers when you get up.
> - **Rings** stay on your fingers instead of sticking out of the body's hand.
> - **Main menu** sits on its own panel, so the laser still hits the buttons after you step
>   around.
> - `HeadLook=0` under `[Head]` lets the stick steer the view with a fixed picture, if your
>   stomach prefers that.
> - "Smooth turning" is now "Ease the turn" and only shows in seated VR, where it does
>   something.
>
> **Still rough**: an old save made in third person loads in third person (known, fix coming);
> the spell ring shows your whole spellbook, no favourites yet; wrists pinch when you turn
> the controllers outwards.
>
> **Reports** as before: `OBVR.log` and `OBVR.log.prev`, headset, controllers, GPU,
> Windows or Linux/Proton, what you did and saw.

## The Discord post for beta 7 (2026-10-29)

Written 2026-10-29 from the commits since beta 6 (070ca9a..88ad755, 247 commits), brought up
to 127bf81 (264 commits) on 2026-10-30; not packaged
and not posted yet - the zip comes from `python3 tools/package-release.py --tag=-beta7`, Nadi
posts. Beta 6 was the last build in the channel (2026-10-22): dist/OBVR-0.3.0-beta6.zip was
written 2026-10-22 14:57:08, two seconds after 070ca9a, and ea34dcd names 070ca9a as its source.
The version stays 0.3.0 (CMakeLists.txt), so the log still says "OBVR 0.3.0". The frame pacing
line is the one set on 2026-10-26 (a2d192f, confirmed on Nadi's Pimax Dream Air: half rate on
2.5 % of the frames instead of 90 %; the Gold Road A/B of performance-spec section 8 found no
regression between beta 5 and beta 6, so a tester's random drops over Virtual Desktop are this
hold or the GPU's budget - his log says which, hence the ask in the line). Accepted in
Nadi's headset since beta 6: the pacing, the inventory figure no longer blown up, the torch's
second fireball gone, the wheel under a container no longer switching to third person, the
torch flame moving, the lean, the upper body no longer turning to NPCs. Everything else below passed the harness only; the "Please try" list names
the newest of those.

The post is about 7,000 characters; Discord takes 2,000 a message (4,000 with Nitro), so it
goes out in parts: split at New, Changed and Fixed (two parts are enough with Nitro).

> **OBVR Full VR - beta 7** (OBVR-0.3.0-beta7.zip, attached)
>
> Same install as always: unzip over `Data`. Your own `OBVR.ini` keeps working - new settings
> take their defaults when they are missing. Two small extras from the README: add
> `d3d9.cachedWriteOnlyBuffers = True` to your `dxvk.conf` (saves time with people in view),
> and if you use Trifle, set `EnableFirstPersonShadow=0` in `Trifle.ini`, or your body shows
> only the hands.
>
> **New**
> - **Frame pacing fixed for headsets whose runtime hands out the poses early** (Virtual
>   Desktop, Pimax). OBVR used to hold every finished frame until the vsync it had worked
>   out from that, and on those headsets that pushed every frame late: half the frame rate,
>   random drops when you look around. If you had that before, tell me whether beta 7
>   changed it - and attach your `OBVR.log` if it didn't.
> - **Four holster places, as in Blade & Sorcery.** Both hips take a one-hander, the torch or
>   a shield; behind both shoulders a two-hander, a staff, the bow or a shield. The weapon
>   hand reaches for weapons and arrows, the other hand for the bow, the shield and the
>   torch. A small sign low in your view says what a place keeps.
> - **Pick up any weapon by its grip** - from the floor or a table: it is equipped and
>   drawn. Out of reach? Close your grip and it flies to your hand.
> - **Let go of your weapon:** open the hand at a holster to put it away, at your chest to
>   unequip it, anywhere else to drop it. Swing and let go to throw it - it spins with your
>   flick. A one-hander can go from one hand to the other and back.
> - **Torches:** take one from the world with the other hand (it lights up), drop it and it
>   keeps burning, keep it at a holster place, parry with it. A torch blow staggers; it
>   knocks down only with a power swing.
> - **Eat and drink at your mouth:** let go of food or a potion at your mouth. Poisons are
>   not drunk.
> - **Grab from the inventory:** close your hand on an item in the inventory. A weapon goes
>   to the weapon hand, a shield, torch or bow to the other; anything else is held in your
>   hand.
> - **Run by swinging your arms** while you walk with the stick. Off by default - the start
>   guide asks you on its new "How do you run?" page.
> - **Spells leave from the casting hand**, aimed along its laser. The crosshair only shows
>   while you aim (bow drawn, target spell raised) and sits at the distance of what you aim
>   at.
> - **Sneaking without a crosshair:** a ring at the edge of the view shows how hidden you
>   are - black while unseen, turning white as you are seen. Rather the game's eye above the view? Comfort page, or
>   `[Hands] SneakIndicator=eye`.
> - **Your body leans:** bend forward or to the side and the trunk bends at the hips, while
>   the feet stay where you stand. `[Body] IKLean=0` turns it off.
> - Your body stays with your hands in the inventory, containers, the map and books.
> - Soft push: on a body with BBB bones, your palms press the chest in a little.
>   `[Body] SoftPush=0` turns it off.
> - Your weapon strikes train your weapon skill, like the game's own attacks.
> - Leave someone out of the fighters' ring or the attack tempo by name or race:
>   `[Hands] CombatRingExceptions` and `AttackTempoExceptions` (for example `Zombie,Walker`).
> - `[Render] MonitorMenusOnly=1`: the monitor shows only the menus, no game picture.
> - The last 30 logs are kept: each start copies the previous `OBVR.log` into `OBVR-logs`,
>   named by date and time - a report can attach the log of the run that went wrong.
>
> **Changed**
> - **A hit lands the moment the blade or fist touches**, as in Blade & Sorcery. Hit at
>   4.5 m/s or faster and it is a power attack. Small twitches no longer count as swings.
> - **The blade slides off people** instead of sticking to them. People's bodies, your
>   blade (mace and axe heads too) and your hands now have shapes fitted to what they are,
>   so steel and hands rest on armour instead of hovering near it.
> - **A two-hander in one hand** costs 40 % more to attack, block, swing, parry, shove and
>   grab, and held up over your hip it drains fatigue. The other hand takes the handle with a
>   normal grip now, no hard squeeze needed.
> - Your blade blocks a creature's blow when it is in the way - zombies and creatures were
>   hardly blockable.
> - Fighters waiting their turn in the ring no longer swing from afar.
> - Enemy health bars show only while a fighter is hurt and in sight, fade after 4 s, and
>   sit at the fighter's own height (a rat's bar is no longer 2 m up).
> - The rubbing buzz comes only while a blade or hand slides along something, stronger the
>   faster - no more pulsing while you just rest against someone.
> - Start guide: it asks first how you play - **Full VR** (standing, motion controllers) or
>   **Vanilla VR** (gamepad or mouse and keyboard, the headset to look around) - and says what
>   each means. No Skip buttons any more.
> - Messages and subtitles glide after your head instead of jumping back ("Text follow time"
>   in the settings).
> - Loading screens: you stand under OBVR's own dome while the game loads - parchment at the
>   start, stars once you are in the world - instead of a frozen picture with your hands in it
>   or SteamVR's grid. The game's fades darken the whole view instead of a panel.
> - Ultimate Leveling's XP bar sits at your hand with the other bars; active effects at the
>   left hand.
> - Grass grows all around you - no more popping when you turn your head.
> - Crouching for real no longer drops the view twice; sneaking with the stick crouches the
>   body as before.
> - Throws take the hand's fastest moment, and an item's name shows as soon as its ring does.
> - Smoother and lighter: a precise frame clock, a faster vertex copy, crowds cheaper,
>   menus that open without a hitch, no stutter after saving a setting, and the physics
>   step taken from your headset's real refresh rate.
>
> **Fixed**
> - An old save made in third person loads in first person.
> - The race menu shows your face from the front.
> - The inventory figure shows whole, legs too, and no longer blows up.
> - Quest controllers: clicking a save in the Load menu works.
> - Hotkey ring pages are kept with your save (they were lost on every load). The ring has
>   as many pages as you use, plus one empty.
> - The mouse wheel under a container menu no longer switches to third person.
> - Your hand no longer sticks to someone after you walk away.
> - Riding up or down a slope no longer pulls the view out of the saddle.
> - The held torch's flame moves again, and the second fireball in the middle of the view
>   is gone.
> - Drawing the bowstring no longer hides part of your arm near the face.
> - Cuirasses with unusual parts (Vanilla Gear Redux, BreakArmor) no longer hang in front of
>   your eyes, and the cuirass and torso no longer vanish when you look down after stepping
>   around the room.
> - Back in first person after a knockdown or an Oblivion Combat Extender scene that began
>   in first person.
> - Your upper body no longer turns towards people standing near you.
> - The first start's T-pose no longer leaves your body floating above the ground.
> - A crash at some save loads (something OBVR remembered from before the load was read
>   after it).
>
> **Please try** (new, not seen in a headset yet):
> - Soft push on a BBB body.
> - An empty holster place shows no sign, unless it would take what your hand carries.
> - Pull a weapon to your hand, then holster or stow it (a crash there was fixed just before
>   this build).
> - Step around the room and look down: your body should stay.
> - The four holster places, grabbing from the inventory, and handing a sword to the other
>   hand.
> - The loading dome and the new start guide.
>
> **Reports** as before: `OBVR.log` and `OBVR.log.prev` (or the one from `OBVR-logs`), headset, controllers, GPU,
> Windows or Linux/Proton, what you did and saw.
