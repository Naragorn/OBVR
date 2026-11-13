"""The controls matrix: every action of OBVR_Controls.ini [Buttons] put on
another input at least once, in two runs, and what the log has to say.

Read by tools/gen-controls-matrix.py (writes the scenarios) and
tools/controls-matrix.py (reads the runs' logs into a table). The log lines
are OBVR's own: "Buttons: <Action> from the <input>" (vr::StepBoundFireLine,
camera/hands/ControlsOut.cpp), "Config: buttons ..." and its notes
(core/Config.cpp ReadButtonBindingsStep), "Hands: jump sent (...)".
"""

# The input phrases as vr::ControlSourcePhrase says them.
PHRASE = {
    "RightA": "right A",
    "RightAHold": "right A held",
    "RightB": "right B",
    "RightBHold": "right B held",
    "LeftA": "left A",
    "LeftAHold": "left A held",
    "LeftB": "left B",
    "LeftBHold": "left B held",
    "RightStickUp": "right stick flicked up",
    "RightStickDown": "right stick flicked down",
    "RightStickClick": "right stick's button held",
    "LeftStickClick": "left stick's button",
    "LeftTrackpad": "left trackpad",
    "LeftABHold": "left A and B held together",
}

# The script lines that press an input once (hand-script commands, src/test/HandScript.h).
# MenuHoldSeconds 1.25 for the holds, StickHoldSeconds 0.4 for the stick buttons.
PRESS = {
    "RightA": ["right a 1", "wait 0.1", "right a 0"],
    "RightB": ["right b 1", "wait 0.1", "right b 0"],
    "LeftA": ["left a 1", "wait 0.1", "left a 0"],
    "LeftB": ["left b 1", "wait 0.1", "left b 0"],
    "RightAHold": ["right a 1", "wait 1.6", "right a 0"],
    "RightBHold": ["right b 1", "wait 1.6", "right b 0"],
    "LeftAHold": ["left a 1", "wait 1.6", "left a 0"],
    "LeftBHold": ["left b 1", "wait 1.6", "left b 0"],
    "RightStickUp": ["right stick 0 0.9", "wait 0.08", "right stick 0 0"],
    "RightStickDown": ["right stick 0 -0.9", "wait 0.08", "right stick 0 0"],
    "RightStickClick": ["right click 1", "wait 1.0", "right click 0"],
    "LeftStickClick": ["left click 1", "wait 1.0", "left click 0"],
    "LeftTrackpad": ["left trackpad 1", "wait 0.1", "left trackpad 0"],
}

# One run: its [Buttons] and [Hands] lines, the binding line and notes the
# log must show, and its steps. A step presses one input; `fires` is the action
# it must fire (None: none), `not_fires` the actions it must not fire from that
# input (the input's default owner, moved away), `close` closes the menus after.
RUNS = [
    {
        "name": "controls-matrix-1",
        "about": "the world's actions swapped among the buttons, sticks and trackpad",
        "hands": ["Holsters=0"],
        "buttons": [
            "Jump=RightA",
            "Activate=RightB",
            "Kick=LeftA",
            "Sneak=RightStickUp",
            "Run=RightStickDown",
            "ReadyWeapon=LeftStickClick",
            "TogglePov=LeftTrackpad",
        ],
        "config": "Config: buttons Jump=RightA Kick=LeftA PauseMenu=LeftBHold Menus=LeftB Activate=RightB "
        "Sneak=RightStickUp ReadyWeapon=LeftStickClick Run=RightStickDown Journal=None TogglePov=LeftTrackpad "
        "QuickSave=LeftABHold",
        "notes": ["Config: buttons - LeftTrackpad went from Journal to TogglePov"],
        "steps": [
            {"input": "RightA", "fires": "Jump", "not_fires": ["Activate"], "extra": ["Hands: jump sent (right A)"]},
            {"input": "RightB", "fires": "Activate", "not_fires": ["Jump"]},
            {"input": "LeftA", "fires": "Kick", "not_fires": []},
            {"input": "RightStickUp", "fires": "Sneak", "not_fires": ["Kick", "Jump"]},
            {"input": "RightStickDown", "fires": "Run", "not_fires": ["Sneak"]},
            {"input": "LeftStickClick", "fires": "ReadyWeapon", "not_fires": ["Run"]},
            {"input": "RightStickClick", "fires": None, "not_fires": ["ReadyWeapon"]},
            {"input": "LeftTrackpad", "fires": "TogglePov", "not_fires": ["Journal"]},
            {"input": "LeftTrackpad", "fires": "TogglePov", "not_fires": ["Journal"]},
            {"input": "RightStickUp", "fires": "Sneak", "not_fires": ["Kick"]},
            {"input": "LeftB", "fires": "Menus", "not_fires": [], "close": True},
        ],
    },
    {
        "name": "controls-matrix-2",
        "about": "the menus, the holds and the quick save swapped; a typo, None and two keys on one input",
        "hands": [],
        "buttons": [
            "Menus=RightAHold",
            "PauseMenu=RightBHold",
            "Journal=RightStickClick",
            "QuickSave=LeftAHold",
            "Jump=None",
            "Sneak=Bogus",
            "Kick=LeftB",
            "Activate=LeftB",
        ],
        "config": "Config: buttons Jump=None Kick=LeftB PauseMenu=RightBHold Menus=RightAHold Activate=None "
        "Sneak=RightStickDown ReadyWeapon=None Run=LeftStickClick Journal=RightStickClick TogglePov=None "
        "QuickSave=LeftAHold",
        "notes": [
            "Config: OBVR-test.ini [Buttons] Sneak=Bogus not understood - the default kept",
            "Config: buttons - LeftB is Kick's already, not Activate's too",
            "Config: buttons - RightStickClick went from ReadyWeapon to Journal",
        ],
        "steps": [
            {"input": "RightAHold", "fires": "Menus", "not_fires": [], "close": True},
            {"input": "RightA", "fires": None, "not_fires": ["Activate"]},
            {"input": "RightBHold", "fires": "PauseMenu", "not_fires": [], "close": True},
            {"input": "RightB", "fires": None, "not_fires": ["Jump"]},
            {"input": "LeftB", "fires": "Kick", "not_fires": ["Menus", "Activate"]},
            {"input": "LeftBHold", "fires": "Kick", "not_fires": ["PauseMenu"]},
            {"input": "RightStickClick", "fires": "Journal", "not_fires": ["ReadyWeapon"], "close": True},
            {"input": "RightStickDown", "fires": "Sneak", "not_fires": []},
            {"input": "RightStickUp", "fires": None, "not_fires": ["Kick", "Jump"]},
            {"input": "LeftAHold", "fires": "QuickSave", "not_fires": []},
        ],
    },
]


def fire_line(action, source):
    return "Buttons: %s from the %s" % (action, PHRASE[source])
