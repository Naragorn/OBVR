# OBVR VR Modding Knowledge Base

**This knowledge base is primarily designed for coding agents** (Claude Code, Codex,
Cursor agents and their successors) working on OBVR and on future VR modding projects.
It serves as persistent technical memory across agents and development sessions: the
engine facts, the architecture decisions, the experiments, the dead ends and the reusable
VR-porting knowledge that would otherwise live only in one session's conversation.

Human developers are welcome here too, but the writing is optimised for a reader who
enters the repository with no prior context and has to decide, quickly and safely, what
is known, what is suspected, and what has already been tried and failed.

Before changing stereo rendering, the camera architecture, compositor timing, aiming, UI
integration or any other core VR system, read the relevant documents here first. The goal
is to avoid rediscovering solved problems and repeating failed experiments.

Written 2026-09-05 against OBVR 0.1.1 (commit `43f619c` on `main`). The code is the source
of truth for the *current* implementation; this knowledge base is the source of truth for
*why* it is that way and for what was tried before.

## How this differs from the other documents

| Document | What it is | Audience |
| --- | --- | --- |
| `README.md` (repository root) | User manual: install, configure, report | players |
| `HANDOFF.md` | The operational session handover, written as the work happened (2026-08-25 onwards), Linux to Windows. Chronological, first person, with retractions in place. Keep it as it is. | the next agent continuing *the same work* |
| `docs/hand-tracked-mode.md` | The ladder for the standing experience, rung by rung | whoever tests the hand-tracked mode |
| `docs/verification/` | Logs and screenshots from the runs that confirmed each milestone | anyone checking a claim |
| `src/game/GameAddresses.h` | Every reverse-engineered address with its disassembled evidence | anyone touching an address |
| **`docs/vr-modding/`** (this directory) | The consolidated, classified, cross-referenced engineering memory | future agents, on this and on other games |

`HANDOFF.md` is not replaced by this directory. Where a finding here rests on a HANDOFF
passage, the passage is cited by its heading. Where the two disagree, this knowledge base
says so and says which one the code supports.

## The documents

Read in this order for a full picture; jump to a single file for a single question.

1. **[architecture.md](architecture.md)** - what OBVR is today: the process, the hooks,
   the frame lifecycle, the module map, the configuration, build, tests, tools, and the
   fragile areas. Start here.
2. **[rendering-and-stereo.md](rendering-and-stereo.md)** - the stereo taxonomy and where
   OBVR sits in it; the dual pass, the frame clock, the bone lock, the shared culling; AER;
   projection, frustum and eye-sized frames; DXVK, Vulkan and the OpenVR compositor;
   simulation versus rendering.
3. **[camera-tracking-and-aiming.md](camera-tracking-and-aiming.md)** - the camera chain,
   coordinate conventions, head tracking (6DoF, recenter, tracking space), the separation
   of body, camera, head and aim, the aiming history and its current solution, the
   hand-tracked mode.
4. **[ui-and-hud.md](ui-and-hud.md)** - the 2D layer redirect, overlays, cinema versus
   world menus, held and live menu backgrounds, the depth crosshair, the 2D screen size
   problem, cursor alignment, OBVR's own menus.
5. **[engine-behavior.md](engine-behavior.md)** - Oblivion and Gamebryo facts established
   by measurement or disassembly, the reverse-engineering method, the engine structures,
   version sensitivity, and notes on transfer to other Bethesda games.
6. **[debugging.md](debugging.md)** - the log, the probes, the headless harness, the
   watchdog, the device-lost hunt, and the general techniques.
7. **[failed-approaches.md](failed-approaches.md)** - every dead end with its evidence and
   its "do not retry unless".
8. **[experiments.md](experiments.md)** - the measurements that decided things, with
   hypothesis, result and conclusion.
9. **[open-questions-and-known-issues.md](open-questions-and-known-issues.md)** - current
   bugs, unverified claims and research tasks for future agents.
10. **[vr-porting-playbook.md](vr-porting-playbook.md)** - the reusable procedure for
    bringing another flat game into VR, corrected by what OBVR taught.
11. **[ecosystem-and-prior-art.md](ecosystem-and-prior-art.md)** - the VR modding
    landscape as reviewed on 2026-09-05, with a taxonomy and a comparison table.
12. **[glossary.md](glossary.md)** - the vocabulary, OBVR's included.

## Status labels

Every important finding carries one of these. A future agent must be able to tell a proven
fact from a suspicion at a glance, and nothing here may promote a guess to a fact.

| Label | Meaning |
| --- | --- |
| `CONFIRMED` | Measured in the running game, read out of the binary, or reproduced by a test; the evidence is named. |
| `LIKELY` | Strongly supported, one source short of confirmed, or confirmed on one machine only. |
| `EXPERIMENTAL` | Built and switchable, not yet seen working in a headset. |
| `HYPOTHESIS` | A candidate explanation nobody has measured. |
| `DEAD END` | Tried, failed, evidence recorded; see failed-approaches.md. |
| `UNVERIFIED` | Repeated in comments or documents without evidence found in code, history or a source. Treat as folklore until checked. |
| `UNKNOWN` | Nobody knows, and the record says so. |

## Scope labels

Each finding also says how far it is expected to travel:

`Scope: Oblivion` (this binary, 1.2.0.416), `Scope: Gamebryo` (the NetImmerse/Gamebryo
scene graph and renderer as Bethesda used them, Morrowind to Fallout: New Vegas),
`Scope: Bethesda legacy` (Bethesda's gameplay layer on top of Gamebryo), `Scope: DirectX 9`,
`Scope: DXVK`, `Scope: OpenVR`, `Scope: OpenXR`, `Scope: General VR`, `Scope: Legacy games`
(any old flat game), `Scope: Unknown`.

A finding scoped to Oblivion is not a finding about Skyrim. A finding scoped to General VR
is one the next project should expect to meet again.

## The three things every new agent gets wrong at first

1. **OBVR talks OpenVR, not OpenXR.** There is no OpenXR path in the code. The reason is
   bitness (a 32-bit process needs a 32-bit runtime) and it is documented, with dates, in
   [architecture.md](architecture.md#why-openvr-and-not-openxr). Do not "port to OpenXR"
   without reading that section and the 2026 runtime table in
   [ecosystem-and-prior-art.md](ecosystem-and-prior-art.md#runtime-and-api-facts).
2. **Two sources for every address.** A reverse-engineered address is used only when the
   disassembly of the binary and an independent source agree. Every address in
   `src/game/GameAddresses.h` carries its evidence, and `mem::Verify` checks the bytes
   before any patch. Do not add an address on one source; do not patch without a verify.
3. **The second world render must not advance the world.** The dual pass calls the
   engine's own frame render twice per tick with the frame clock zeroed and the skinning
   palettes locked. Everything that touches the render path has to respect that; see
   [rendering-and-stereo.md](rendering-and-stereo.md#simulation-versus-rendering).

## Agent maintenance rules

Before modifying stereo rendering, camera architecture, compositor timing, aiming, UI
integration or other core VR systems:

1. Read the relevant knowledge-base document, and `HANDOFF.md`'s section on the same topic
   if one exists.
2. Check whether the approach has already been tested: grep
   [failed-approaches.md](failed-approaches.md) and [experiments.md](experiments.md).
3. Do not treat a `HYPOTHESIS` or `UNVERIFIED` entry as a fact. Measure it, or leave it
   labelled.
4. Respect the standing rules in the repository README ("Working on it"): two sources for
   every address, cause not workaround, everything in English.

When a task produces a meaningful new VR-specific finding:

1. Update the relevant knowledge-base document (not only `HANDOFF.md`, which is the
   chronological record; this directory is the consolidated one).
2. Record the root cause, not only the fix. "It works now" is not a finding.
3. Record meaningful failed approaches in [failed-approaches.md](failed-approaches.md)
   with the evidence and the conditions under which a retry might make sense.
4. Add evidence: a log under `docs/verification/`, a commit, a disassembly excerpt, a URL.
5. Set the status label and the scope label.
6. Update [open-questions-and-known-issues.md](open-questions-and-known-issues.md): close
   what was answered, add what was opened.
7. If the finding is about an engine address or structure, put the evidence in
   `src/game/GameAddresses.h` as well; that header is the address authority.

Do not update the knowledge base for trivial refactors or cosmetic changes. Do not turn it
into a diary: `HANDOFF.md` and the commit history already are one. Preserve history here
only where it explains why the current architecture exists, which approaches failed, or
which assumptions were disproven.

## A note on commit references

The repository history was rewritten before the first public build (2026-09-05, the
commit "Prepare the repository for its first public test build, 0.1.0") to strip commit
trailers. Every hash changed. Commit hashes quoted **inside source comments and
older commit bodies** (for example `7e57e69`, `a527686`, `40132d2`, `6ee61af`, `40d7e13`,
`a61f4e0`) refer to the pre-rewrite history and do not resolve any more. This knowledge
base quotes the current hashes and, where it matters, maps the old ones:

| Old hash in comments | Current commit | Subject |
| --- | --- | --- |
| `7e57e69` | `c79160e` | Give the aiming turn back once the arrow has left |
| `a527686` | `e334cac` | Revert "Give the aiming turn back once the arrow has left" |
| `40132d2` | `fcac8a3` | Refuse same-posed strangers when a bone row's pair sits off position |
| `6ee61af` | `6f4ccde` | Wait for poses under DXVK's queue lock |
| `40d7e13` | `cb9bd1e` | Point the cursor sprite at the copy (later taken back in `4582c12`) |
| `a61f4e0` | unknown | A run that produced a spurious device loss before the dual pass existed; the commit could not be identified after the rewrite |

Use `git log --grep="<subject words>"` rather than an old hash.
