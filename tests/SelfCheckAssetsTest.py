"""One list of OBVR's files, four places: the in-game self-check
(core/SelfCheckLogic.h kExpectedFiles), the two doctor scripts
(tools/obvr-doctor.py, tools/OBVR-doctor.ps1) and what the release zip ships
(assets/ and the two INIs). A file added to the zip that the checks do not know,
or a check for a file the zip does not ship, fails here. Also: both release
scripts pack the doctor, and the .bat runs the .ps1 past the execution policy."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def norm(path: str) -> str:
    return path.replace("\\", "/").lower()


# The C++ list.
header = (ROOT / "src/core/SelfCheckLogic.h").read_text(encoding="utf-8")
block = header.split("kExpectedFiles[] = {", 1)[1].split("};", 1)[0]
cpp = {("plugin" if p == "true" else "data", norm(path.replace("\\\\", "\\")))
       for p, path in re.findall(r'\{(true|false), "([^"]+)"\}', block)}

# The Python doctor's lists.
py_text = (ROOT / "tools/obvr-doctor.py").read_text(encoding="utf-8")


def py_list(name):
    body = py_text.split(f"{name} = [", 1)[1].split("]", 1)[0]
    return {norm(p) for p in re.findall(r'"([^"]+)"', body)}


py = {("plugin", p) for p in py_list("PLUGIN_FILES")} | {("data", p) for p in py_list("DATA_FILES")}

# The PowerShell doctor's lists.
ps_text = (ROOT / "tools/OBVR-doctor.ps1").read_text(encoding="utf-8")


def ps_list(name):
    body = ps_text.split(f"${name} = @(", 1)[1].split(")", 1)[0]
    return {norm(p) for p in re.findall(r'"([^"]+)"', body)}


ps = {("plugin", p) for p in ps_list("PluginFiles")} | {("data", p) for p in ps_list("DataFiles")}

# What the zip ships that OBVR reads (tools/package-release.py's layout):
# the INIs, OBVR_Input, OBVR_Sounds and OBVR_Stage next to OBVR.dll, and
# assets/menus, textures and meshes under Data. Text files nobody reads
# (the sounds' README) are not checked.
shipped = {("plugin", "obvr.ini"), ("plugin", "obvr_controls.ini")}
for folder, prefix in (("input", "obvr_input"), ("sounds", "obvr_sounds"), ("stage", "obvr_stage")):
    for f in (ROOT / "assets" / folder).rglob("*"):
        if f.is_file() and f.suffix.lower() != ".txt":
            shipped.add(("plugin", f"{prefix}/{norm(str(f.relative_to(ROOT / 'assets' / folder)))}"))
for folder, prefix in (("menus", "menus"), ("textures", "textures"), ("meshes", "meshes")):
    for f in (ROOT / "assets" / folder).rglob("*"):
        if f.is_file():
            shipped.add(("data", f"{prefix}/{norm(str(f.relative_to(ROOT / 'assets' / folder)))}"))

assert cpp == shipped, f"self-check vs zip: only in the check {sorted(cpp - shipped)}, only in the zip {sorted(shipped - cpp)}"
assert py == cpp, f"obvr-doctor.py vs self-check: {sorted(py ^ cpp)}"
assert ps == cpp, f"OBVR-doctor.ps1 vs self-check: {sorted(ps ^ cpp)}"
print(f"Expected files: {len(cpp)} in the self-check, both doctors and the zip alike")

# The release zip carries the doctor, both ways of building it.
package_py = (ROOT / "tools/package-release.py").read_text(encoding="utf-8")
package_ps = (ROOT / "tools/package-release.ps1").read_text(encoding="utf-8")
for name in ("obvr-doctor.py", "OBVR-doctor.ps1", "OBVR-doctor.bat"):
    assert name in package_py, f"package-release.py does not pack {name}"
    assert name in package_ps, f"package-release.ps1 does not pack {name}"
print("Both release scripts pack the doctor")

# The .bat: a double-click runs the .ps1 past the execution policy, and the
# window stays open at the end.
bat = (ROOT / "tools/OBVR-doctor.bat").read_bytes().decode("utf-8")
assert "-ExecutionPolicy Bypass" in bat and '-File "%~dp0OBVR-doctor.ps1"' in bat, bat
assert re.search(r"^pause\s*$", bat, re.MULTILINE), "the window must stay open"
assert "\r\n" in bat, "a .bat keeps Windows line ends"
print("OBVR-doctor.bat: Bypass, its own folder, pause")

# The panel's menu: an OK with its own closing ID and five lines.
menu = (ROOT / "assets/menus/generic/OBVR_SelfCheck.xml").read_text(encoding="utf-8")
assert "<id>9402</id>" in menu, "the self-check panel closes with 9402"
for i in range(5):
    assert f'trait="user{i}"' in menu, f"line user{i} missing"
print("OBVR_SelfCheck.xml: OK 9402, five lines")
