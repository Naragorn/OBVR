# OpenVR client library

`win32/openvr_api.dll` is Valve's 32-bit OpenVR client library, shipped in the release
archive next to `OBVR.dll` so nobody has to find the right one in their SteamVR folder.
It is the loader only: it finds the installed SteamVR runtime and hands out the
interfaces OBVR asks for by version string (`IVRSystem_026`, `IVRCompositor_029`,
`IVROverlay_028`, `IVRInput_011`), which the runtime serves. OBVR looks for it next to
`OBVR.dll` first.

- Source: https://github.com/ValveSoftware/openvr, tag `v2.15.6`, `bin/win32/openvr_api.dll`
- SHA-256: `ab696e4f218a95b3e396bc310f9fe6485df48c99c0969762083212b1e1f025a6`
- License: BSD-3-Clause, `LICENSE` beside this file (from the same tag). Redistribution in
  binary form has to carry the copyright notice and the license, so the release archive
  ships it as `OBSE/Plugins/openvr_api-LICENSE.txt`.

`tools/package-release.ps1` checks the hash before packaging. To update: take the file
from a newer tag's `bin/win32`, check that its header still lists the four interface
versions above, and change the tag and the hash here and in the script.
