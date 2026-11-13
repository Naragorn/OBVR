# Tutorial and help messages in Full VR

The new game's tutorial cards and the other vanilla texts that name keyboard, mouse or
gamepad controls, read out of `Oblivion.esm` (English, the Steam install), with what Full VR
says today and what it should say on the bindings of 2026-11-13 (README "Controls in Full
VR", docs/controls-spec.md section 3, OBVR_Controls.ini: right B jumps, the weapon stick
flicked up kicks, left A + B held is the quick save, left A drops in the inventory, the
weapon stick's button held is the rest/wait menu). A list for review; nothing here is built.

## How the texts reach the screen (read, not new)

- **The cards are MQ01's stage log entries** (QUST `0001E723`, "Tutorial"). Each stage has
  a console entry and a PC entry where the words differ, chosen by the condition `IsXBox`
  (CTDA function 309, value 1 console / 0 PC); on PC only the value-0 and the unconditioned
  entries are ever shown. The quest update shows them as the Generic menu from
  `quest_added.xml` ("Quest updated - Tutorial"), not a MessageMenu (bf94bc8). MQ01Script
  (SCPT `000297AD`) carries the old MessageBox versions, all commented out.
- **The `&sUActn...;` tokens** are resolved at text layout: the font layout `0x00575B40`
  looks the name up among the 29 control-name settings and calls `0x0057C240(control, out)`
  for "push W"-style words. OBVR detours that call (game/ControlWords.h, ui/ControlPhrase.h,
  bf94bc8) and answers the mode's words - for every text that carries a token, in every
  language, mods included.
- **Whole cards** are swapped where a sentence is bound to the mouse beyond a token: the
  stage entry reader `0x0052AF40` is detoured (game/TutorialCards.h) and an entry matched
  whole against ui/TutorialCards.h (nine English cards) gets OBVR's words (ffc8cb9,
  a753727). Another language or a mod's changed text matches nothing and keeps its words
  (its tokens still get the VR phrases).
- **Stages 92, 94 and 96 are never shown in vanilla**: stage 90's result script sets stage
  100 directly and the lines that would go on to 92 are commented out ("all subsequent
  tutorials turned off"). Stage 26 is marked "REMOVED CURRENTLY" in its own result script
  but its entry is still read when a script sets it (the 2026-10-16 harness showed it).

## MQ01, the PC entries (FormID `0001E723`, by stage)

Status: **token** = only tokens, VR words by the phrase hook; **card** = swapped whole by
ui/TutorialCards.h; **vanilla** = shown as written. "Stale" marks what no longer fits.

| Stage | Original text (PC) | When | Today in Full VR | Proposed VR text |
|---|---|---|---|---|
| 10 | To move forward, &sUActnForward;, &sUActnBack; to move back. To move left, &sUActnSldleft;. To move right, &sUActnSldright;. The mouse turns you. To run, &sUActnRun;. | prison, the first card | card: "...Your weapon hand's stick turns you; hold it up to aim a teleport and let go to go there..." | keep; add "Flick it up to kick." only if the kick should be taught here |
| 15 | You need a weapon. Check Captain Renault's body. To open any container, approach it and &sUActnActivate; when its name appears. | after the ambush, no weapon yet | card: "...or just reach into it." | keep |
| 20 | To bring up your journal, &sUActnMenumode;. You can use this to view your character's attributes, equip armor and weapons, and choose spells. | after looting a corpse | token ("press B on the left controller") | keep; optional "Tap B on your other hand; the journal opens on your wrist." |
| 22 | Click on the fist icon at the bottom of the journal to bring up the inventory tab. From here, you can equip armor and weapons by clicking on them. | journal open | card: "...Point your weapon hand's laser at it and **press A to click**..." | **stale/wrong**: the laser clicks with the trigger (controls-spec "the trigger pulled ... sends the click", StepLaserPress). "The journal hangs on your other wrist. Point your weapon hand's laser at it and pull the trigger to click. Click the fist icon at the bottom to bring up the inventory tab..." |
| 26 | Click on an item to equip it. Click again to unequip it. Shift-click to drop an item. | removed in vanilla; shown when a script sets it | card: "...To drop an item, **hold A** on your other hand over it." | the drop is a press: "To drop an item, point the laser at it and press A on your other hand (X on Quest). Or take it out with the grip and let it fall." |
| 28 | To ready your weapon or fists, &sUActnRdyitem;. To attack, &sUActnUse;. To block, &sUActnBlock;. | the rats | token | the draw phrase says "reach to your left hip and squeeze the right grip, or make a fist" - add the back: "reach to your hip or over your shoulder and squeeze the grip" (two-handers and bows hang on the back) |
| 34 | Now practice casting spells. To cast your selected spell, &sUActnCast;. If you want to change spells, &sUActnMenumode; to bring up your journal again. | after the rats | token | keep (optional: "or click your other hand's stick for the spell ring") |
| 36 | Click on the spell icon at the bottom of the journal to bring up the magic tab. | journal open | card | keep, "with the laser" already said; "click" = trigger, consistent once stage 22 says it |
| 50 | Before you can use the bow, you'll need arrows. Check the skeleton to see what it contains. | the bow room | vanilla | keep (no control) |
| 52 | Now that you have arrows, pick up the bow and practice using it. | | vanilla | keep |
| 54 | To use a bow, you'll need to have both the bow and some arrows equipped. Go into your inventory and equip them now. | | vanilla | keep |
| 56 | Now you're ready to practice with your bow... To draw and aim, &sUActnUse;. Release it when you're ready to fire. | near the bucket | card, three forms by BowByHand / ArrowWithGrip | keep |
| 58 | Nice shot! You can retrieve arrows from the world... | the bucket hit | vanilla | keep |
| 64 | You have found a Mortar and Pestle. Equip it to create potions... | picked up | vanilla | keep |
| 65 | You have found a Repair Hammer. Equip it to repair your weapons and armor... | picked up | vanilla | keep |
| 70 | If you have a lockpick... When the tumbler reaches the top of the lock, **click the left mouse button** to secure it in place... | first locked door | vanilla | **stale**: "...Point at a pin, push the stick up to lift it, and pull the trigger when it reaches the top to secure it. When all tumblers are at the top, the lock will open." (README "Picking a lock") |
| 72 | Your Health is low... The red bar **at the bottom left** displays your current Health. | Health < 40 | vanilla | **stale**: "The red bar on the HUD at your wrist shows your current Health." (where the HUD hangs: check the mode's HUD placement before the words are fixed) |
| 73 | Your Magicka is low... The blue bar **at the bottom left**... | Magicka < 25 | vanilla | **stale**, as 72 |
| 74 | Your Fatigue is low... The green bar **at the bottom left**... | Fatigue < 50 | vanilla | **stale**, as 72 |
| 75 | You have picked up more than you can carry... so &sUActnMenumode; to bring up your journal. | over encumbrance | token | keep |
| 76 | You can hotkey items and spells from your journal by holding down a number key and then clicking the item... | near the goblin door | card: "...&sUActnQuickmenu; to open the hotkey ring, move your hand to a slot and click again..." | keep (the token answers "click the right stick") |
| 78 | Your current encumbrance exceeds the maximum... **Shift-click** to drop items you don't need. | over encumbrance in the inventory | card: "...**hold A** on your other hand over them." | the drop is a press, as 26: "...point the laser at them and press A on your other hand (X on Quest)." |
| 80 | That goblin up ahead hasn't seen you yet... To go into sneak mode, &sUActnCrouch;. | the goblins | token ("flick the right stick down, or crouch") | keep |
| 82 | Notice that the crosshair changes to an 'eye' while you are sneaking... | sneaking | vanilla | keep (the crosshair is drawn in VR) |
| 84 | You now must choose your class... | the class menu | vanilla | keep |
| 90 | The red arrow on your compass shows the direction of Weynon Priory... use your world map to travel there directly... | out of the sewers | vanilla | keep (the compass is on the HUD; the map in the journal) |
| 92 | To view your quest log, &sUActnMenumode; to bring up your journal. | never in vanilla | token | nothing to do |
| 94 | Click on the compass at the bottom of the journal to bring up the quest and map tab. | never in vanilla | card | dead in vanilla; keep for mods that turn it on |
| 96 | This page displays all your active quests. Click on an active quest and **hit the MAP button**... | never in vanilla | vanilla | dead in vanilla; if a mod shows it: "...click an active quest and then the MAP button with the laser..." |
| 110-130 | The persuasion minigame's three cards ("As you highlight each possible action ...") | first persuasion | vanilla | keep (no key named; the laser highlights) |
| 140 | All of your possessions have been confiscated... Activate the bed to begin serving your sentence. | jailed | vanilla | keep ("Activate" in plain words; optional "press A on the bed") |
| 150 | Combine ingredients with like effects... | first alchemy | vanilla | keep |

The console variants (stages 10, 22, 26, 36, 70, 76, 78, 94, 96 with `IsXBox` 1: "Use the left
stick to move around", "Press A to equip ... X will drop items", "holding Y and pressing a
direction on the D-pad", ...) are never shown on PC and need nothing.

## Loading screen tips (LSCR) that name a control

337 LSCR records; four name a control or a button. They are not hooked today.

| FormID | EDID | Text | Proposed VR text |
|---|---|---|---|
| `0006AAC6` | InGameCombat13 | Power attacks do large amounts of damage and are done by holding down the Attack button. | Power attacks do large amounts of damage and are done with a long, fast swing. |
| `000CB620` | InGameCombat20 | If you get into a fight with a friend, you can yield to him to stop the combat by holding Block and activating that character... | ...you can yield to him by raising both open hands, palms to him, and waving them gently for a second... |
| `0006AB11` | InGamePerksAcrobatics03 | A Journeyman of Acrobatics can dodge by jumping while holding Block. | A Journeyman of Acrobatics can dodge by jumping (B on the weapon hand) while blocking. (whether OBVR's dodge works that way: controls-spec section 2 says "gesture + button"; check before fixing the words) |
| `0006AB15` | InGamePerksMarksman03 | A Journeyman of Marksman can zoom in on the target by holding Block while shooting. | to be decided: whether the bow's zoom exists in Full VR is not read here |

Not counted as control texts: tips that only say "jump", "run" or "journal" as an action
("When in combat, don't always run", "Actions like jumping ... decrease your Fatigue").

## The token phrases themselves (ui/ControlPhrase.h)

Any text with a token gets these words, so a wrong phrase is wrong everywhere:

- **Jump (`&sUActnJump;`) is stale**: it says "flick the right stick up"; since 2026-11-13
  the jump is the right B and the flick kicks. Should be "press B on the right controller"
  (the weapon hand's). No MQ01 card uses the token; mods' texts and menus can.
- **DrawSheatheWeapon** names only the hip ("reach to your left hip and squeeze the right
  grip, or make a fist"); two-handers and bows hang over the shoulder, and with
  `[Hands] Holsters=0` the draw is the weapon stick's button held.
- **Rest** ("hold the right stick clicked in") holds only with `Holsters=1`.
- QuickSave, QuickLoad, AutoRun, AutoMove and TogglePOV are left to the engine's words; the
  quick save now has a VR answer ("hold A and B on your other hand for a second").

## Not covered by this list

- Other plugins (DLC, Shivering Isles, the testers' mods) and other languages: the whole-card
  swap matches only the English entries; their tokens still get the VR words.
- The menus' own XML texts in the BSAs (inventory hints and the like) were not read.
- Where the HUD and its bars hang in Full VR was not re-read for stage 72-74's words.

## Ways to build it, with cost

| Route | What | Cost | Risk |
|---|---|---|---|
| **A: extend what is built (recommended)** | ui/ControlPhrase.h: the jump phrase, the draw's shoulder, the quick save; ui/TutorialCards.h: fix 22, 26, 78 and add 70, 72, 73, 74 (whole-match, English) | small - both hooks and their tests (control_phrase_test, tutorial_cards_test, harness tutorial-cards.txt) exist; new are the strings and their test cases | the HUD wording needs the HUD's place in the mode; other languages keep vanilla words |
| B: loading tips through the DLL | find where the loading menu takes an LSCR's DESC and swap the four by FormID | small to medium - the read site is not found yet (REA), then the same table pattern | an unread engine path |
| C: an ESP | override MQ01's stage entries and the four LSCRs | medium - a plugin to ship and keep in the load order | MQ01 is touched by the Unofficial Patch and other mods: a quest override would hide their fixes; the DLL route avoids the conflict |
| D: a token per stage (placeholder hook only) | let the phrase hook carry more and leave the cards as vanilla | not enough - the stale sentences (left mouse button, bottom left, Shift-click) carry no token | - |

Recommended: A now (it is where the words already live), B when the loading tips matter,
C not at all.
