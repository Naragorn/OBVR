# Voice: talking to people and saying commands (spec, not built)

Asked 2026-11-27 (Nadi, after a look at other VR mods' voice commands): a spec first.
Nadi's four decisions:

1. **"hello" said to someone you look at starts the conversation** without a button. **A
   held** is push to talk for the commands outside a conversation.
2. In a conversation, **saying a topic picks it**, and **it listens the whole conversation**
   with no button (Nadi, the same day: "warum eig a halten müssen, kann doch während des
   gesamten gespräch lauschen").
3. The recognition runs **on its own thread, only while it listens**.
4. The recognised line **clicks the topic** through the menus' existing click.

**Windows first; Linux later** ("linux, kommt dann halt später"). The engine sits behind
one interface so a second one can come for Linux without touching the rest.

Labels used below: [R] read (code, EXE or a source named), [D] derived, [G] guessed.

## 1. How the others do it

- **Dragonborn Speaks Naturally** (Skyrim SE/AE/VR) [R: its Nexus page, mods/16514;
  github.com/DougHamil/DragonbornSpeaksNaturally README]:
  - A C++ DLL hooks the dialogue menu's update. A C# service recognises speech with
    Microsoft's `System.Speech` and passes the line to the DLL over stdin/stdout.
  - The DLL selects the matching dialogue line, or runs a console command bound to a phrase.
  - Its own words on the Microsoft engine: bound to the system locale, a low hit rate, and
    random commands from noise after a while. Hence the optional Voice2Json engine (Docker
    or WSL), plus `dialogueMinConfidence=0.5` and `commandMinConfidence=0.7`.
- **FNVVRMC** (New Vegas VR) [R: Nexus newvegas/mods/100085]:
  - Look at someone and say "hello", "hey" or "excuse me" to start a conversation.
  - Say an answer's words, or "option 2", to pick it.
  - Offline, through the headset microphone, in a separate add-on.
  - Its page: hello works, saying answers does not yet.
- **A Tale of Two Realities** (TTW VR): "hold the A button and speak your reply" [R: a review
  quoted by arvrtips.com, 2026-11-19]. Its engine is not public.
- **Chasms VR Oblivion**: Windows' offline speech recognition for menu words and "what do you
  have for sale" [R: its README].

What OBVR takes from them:

- **Recognise against a short list of allowed lines, never free text.** Topics,
  "option N", a few commands and greetings. This is what keeps the false triggers down.
- **A narrow window instead of a button where the list is short.** The greeting (someone
  in front of you) and the conversation (its topics) listen on their own. Push to talk
  only for the commands, which would otherwise listen all game. A confidence floor on
  each list.

## 2. What the player does

- **Starting a conversation.** Look at someone near you and say "hello". Also accepted:
  "hey", "hi", "excuse me", "greetings", "hail"; in a German game, "hallo", "hey",
  "entschuldigung", "seid gegrüßt". The conversation opens as if you had pressed A on them.
  No button is needed. Holding A and saying it works too.
- **In a conversation.** Just say a topic's name, or "option 3", or "goodbye". No button:
  it listens from the menu's opening to its close, except while the other one speaks
  (section 5). Holding A works too and changes nothing.
- **Anywhere else.** Hold A and say:

  | Line | Does |
  |---|---|
  | "map" | opens the map |
  | "inventory" | opens the inventory |
  | "journal" / "quests" | opens the journal |
  | "magic" | opens the magic page |
  | "quick save" | quick save |
  | "wait" | opens the wait menu |

  These are the same actions as the buttons (`vr::ControlAction`, `OBVR_Controls.ini`), so
  they open what the buttons open, on the tablet when it carries them.
- **Feedback.**
  - While A is held, a small ring fills at the hand, as on the other held buttons, and a
    microphone mark shows once the hold counts as talking.
  - A line that was heard but matched nothing gives a short buzz and no click.
  - Nothing is written on the screen. The log gets the heard line, its confidence and what
    it matched.
- **Settings** (a "Voice" row on the guide's controls page, `[Voice]` in OBVR.ini):
  - `Enabled` (off by default [G: a microphone that listens should be the player's choice;
    the onboarding asks]).
  - `Greeting` (hello without a button; on when Voice is on).
  - `MinConfidence` (0.5 in a conversation, 0.7 for commands, as DSN).
  - `Language` (`auto` = the game's language; the recogniser must be installed for it).

## 3. A held: tap or talk (the commands only)

A is needed only for the commands: the greeting and the conversation listen without it.
Inside a conversation A keeps whatever it does there today. A is Activate today (`vr::DefaultControlBindings`: `Activate = RightA`), and the right A's
hold (`RightAHold`) is free [R]. Both must keep working:

- **Press.** The audio capture starts at once, so the first word is not lost.
- **Release before `[Voice] TalkHoldSeconds` (0.25 s [G]).** A tap: Activate as now, and the
  audio is dropped. This moves Activate from the press to the release [D: as every button
  with a hold bound already does - `OBVR_Controls.ini`: "a button with a hold bound does its
  press on the release"], about 0.1 to 0.25 s later than today. That is the one cost to the
  existing controls. Measure it in the headset; if it bothers, the alternative is the left
  A's hold (`LeftAHold`, free).
- **Held past it.** Talking: no Activate. The recognition ends at the release, and its result
  comes back a few hundred ms later [G].
- **A bound elsewhere** in `OBVR_Controls.ini`: push to talk is its own action, `Talk`
  (default `RightAHold` under this rule), so it moves with the file like every other action.

Pure logic, `vr/VoiceTalk.h`: the step from the button's state and time to
{Idle, Capturing, Tap, Talking, Done}. Tested on every edge: the release exactly at the
limit, a press during a pending result, a menu opening mid-hold, A rebound, Voice off.

## 4. The greeting without a button

- **When it listens.** Only while every one of these holds [D: the narrowest window, so noise
  in the room starts nothing]:
  - Voice and Greeting are on;
  - no menu is up;
  - the crosshair reference (HUDInfoMenu's `crosshairRef`, `game/GameAddresses.h`) is a
    living actor who can talk (not a creature without dialogue, not hostile, not in combat
    with the player);
  - that actor is within `[Voice] GreetMetres` (3 m [G]).
- **What it listens for.** Only the greetings list; confidence at least the command floor.
- **Starting the talk.** The same activation A gives that reference: the Activate control
  pressed for one frame with the crosshair on them [D: what A does now, so it starts exactly
  the conversation A would - the guard's arrest line, a merchant's greeting]. Whether to call
  the engine's activate on the reference directly instead is to be read [open: which
  function, and whether it runs outside the input poll].
- **Cost.** The recogniser runs only while someone stands in front of you. It stops when
  they leave the crosshair or a menu opens.

## 5. In a conversation: the topics

- **Reading the topics.** The dialogue menu (`kMenuIdDialog` 0x3F1, `game/MenuType.h`) lists
  its topics as tiles with a string each [open: the tile names and the list's layout in
  `menus/dialog/dialog_menu.xml` - read them from the BSA, the way the book menu's were read
  for the tablet's section 25]. The list is read when the menu opens and whenever its count
  changes, never every frame.
- **The grammar.** One entry per topic in the menu's language, as the game spells it,
  normalised (case, punctuation, the "<Persuasion>"-style markers removed). Add "option 1"
  to "option N", "goodbye", and the topic's first three words alone when the topic is
  longer than five words [G: people shorten long lines].
- **Listening the whole conversation.**
  - The recogniser starts as the dialogue menu opens and stops as it closes, with the grammar
    above and nothing else. That makes it the safest window of all: a few short lines, and
    the player is talking anyway.
  - **The one hazard is the other one's voice.** With speakers or open-ear headphones (an
    Index, a Quest's built-in audio), the microphone hears the NPC's line. A line that
    contains a topic's name, or a "goodbye" at its end, would pick it.
  - So while the speaker's voice plays, nothing is taken [open: how to read that a dialogue
    voice is playing - the actor's lip-sync or its sound handle, or the menu's state while
    the subtitle runs]. A result whose speech began while the voice played is dropped too:
    SAPI gives each result's start in the audio stream [R: SAPI's `SPEVENT` stream position;
    the mapping to game time is derived].
  - The player can cut the line short as in vanilla (a click skips it). Talking over it is
    not taken until it ends [D: talking over the NPC is too close to the echo to tell apart].
  - If the echo still slips through in the headset, the fallback is A held in the
    conversation too, as a setting (`[Voice] TopicsNeedA`, off) - not built unless needed.
- **The match.** The recogniser returns one of the grammar's entries and a confidence. Below
  the floor: nothing. Two topics that normalise to the same text: the upper one.
- **Picking it.** The same click the laser's trigger gives on that tile [R: the menus' click
  path, `ClickMenuButton "<tile>" <id>` as `ui/OptionsVrButtons.h` uses] - so the game picks
  it as a mouse click would, with its own sound.
- **Not in scope yet:** persuasion's wheel, barter, training and spell-making lines beyond
  their topic. Saying "barter" picks the Barter topic if it is listed; the menu after it is
  the laser's.

Pure logic, `game/VoiceMatch.h`: normalising, the grammar from a topic list, the match with
the floors and the tie rule. Tested with real topic lists (vanilla's Imperial City lines, a
German game's, a mod's long line).

## 6. The engine

- **Interface.** `voice::Recognizer`:
  - `SetPhrases(list)` - the current grammar;
  - `Start()` / `Stop()`;
  - `Poll(out result)` - the game thread, once a frame, no allocation;
  - one implementation per platform.
- **Windows: SAPI 5.4's in-process recogniser** (`CLSID_SpInprocRecognizer` with an
  `ISpRecoGrammar` built from the list, the default recording device as input). Its sources:
  - DSN's `System.Speech` is a .NET wrapper over the same engine [R: DSN's README];
  - from C++ the COM interfaces are used directly, with no .NET and no service process [D].
- **Language.** The recogniser must be installed for the language: DSN lists en-US, en-GB,
  en-CA, en-IN, en-AU, fr, de, ja and the zh variants [R: DSN's SAMPLE.ini]. A missing
  language: Voice switches itself off, logs why, and the guide's row says "Not available for
  your Windows language".
- **The headset's microphone.** SteamVR can make the headset's microphone Windows' default
  recording device while it runs (its Audio settings) [R: SteamVR's settings; whether it is
  on by default is not checked]. OBVR takes the default device and logs its name at the
  start, so a wrong microphone shows in the log.
- **Threads.** SAPI's events come on its own thread. Results go through a single-producer
  queue the game thread drains in `Poll`, without a lock, a file write or a console line per
  frame.
- **Linux, later.** Wine has no SAPI recogniser [G: not checked]. The second implementation is
  an offline library compiled into OBVR.dll: Vosk with a phrase list (small models, German
  among them) or whisper.cpp (better, dearer on the CPU). Proton's microphone through
  WASAPI/winmm is the risk to measure first.

## 7. Performance

- **Per frame:** the poll (one atomic read when nothing is queued). Nothing else.
- **On an event:**
  - the grammar rebuilt when the topic list changes;
  - the recogniser started and stopped at A's press and release, as a conversation opens
    and closes, and as the greeting's
    window opens and closes.
- **What the recogniser costs** on its thread while it listens, and whether starting it
  makes a long frame, is **to be measured** with the profiler ([Performance] Enabled=1): a
  conversation with Voice off against on, the mean and the worst frame.

## 8. Tests and evidence

- **Unit flows:** `VoiceTalk.h` (tap or talk), `VoiceMatch.h` (grammar and match), the
  greeting's gate (`VoiceGreetGate`: each condition alone false), the command table.
- **Harness, on Linux:** a hand-script line `say "<text>" [confidence]` hands a line to a
  scripted recogniser, the third implementation, with no audio.
  - `voice-greet.txt`: look at an NPC, say hello, and the dialogue menu opens. Look away and
    say it: nothing opens.
  - `voice-topic.txt`: in a conversation, say a topic, and the menu shows that topic's line
    (the topic's text read before and after). Say "option 2": the second topic. Say
    "goodbye": the menu closes.
  - `voice-command.txt`: "map" and "journal".
  - `voice-tap.txt`: A tapped still activates; held, no activation.
  - `voice-echo.txt`: a topic said while the NPC's line still plays is not taken; said after
    it, it is.
  - Each scenario runs once with the DLL before the feature (red) and once after.
- **Windows, the headset:** the real microphone, the hit rate on topics, false greetings
  while the room is noisy, and whether Activate on the release is felt.

## 9. Cost

**Medium.** The work splits into three:

- **Reused:**
  - the activate path;
  - the menus' click;
  - the button bindings and the hold ring;
  - the tile reading of the tablet.
- **New:**
  - the SAPI recogniser with its thread and queue;
  - the topic reading;
  - matching;
  - push to talk;
  - the greeting's gate;
  - the settings and guide row;
  - the scripted recogniser for the harness.
- **Risk:**
  - the dialogue menu's tiles (not read yet);
  - SAPI's hit rate with headset microphones;
  - Activate moved to the release.

Model and effort: Opus 5.5 high (COM threading and the dialogue menu's unread tiles).

## 10. Open

- The dialogue menu's topic tiles and their click ids (to read first).
- How to read that the NPC's voice is playing (the echo gate, section 5).
- Activating the greeted actor directly instead of the Activate control.
- `TalkHoldSeconds`, `GreetMetres` and the confidence floors (guesses until the headset).
- Voice on by default or off (spec: off, the onboarding asks).
- Linux's engine and the microphone under Proton.
