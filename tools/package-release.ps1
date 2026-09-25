# Builds the release archive.
#
# Reads the version from the project() line of CMakeLists.txt, takes the
# release DLL from build/ (or the directory given) and OBVR.ini from the
# repository root, and writes dist/OBVR-<version>.zip laid out so that it
# extracts into Oblivion's Data folder and installs as an ordinary Mod
# Organizer 2 mod alike:
#
#   OBSE/Plugins/OBVR.dll
#   Menus/Generic/OBVR_Onboarding.xml
#   Menus/Generic/OBVR_Settings.xml
#   Menus/Generic/OBVR_Update.xml
#   Menus/Prefabs/OBVR/button_highlight.xml
#   OBSE/Plugins/OBVR.ini
#   OBSE/Plugins/OBVR-LICENSE.txt   (the GPL-3.0, which travels with every copy)
#   OBSE/Plugins/OBVR_Input/        (the SteamVR action manifest and bindings)
#   OBSE/Plugins/openvr_api.dll     (Valve's 32-bit OpenVR client, third_party/openvr)
#   OBSE/Plugins/openvr_api-LICENSE.txt   (its BSD-3-Clause, which binary copies carry)
#
# The linker map is copied beside the archive as OBVR-<version>.map, for
# reading an OBVR.dll+offset from a report back to a function.
#
# Refuses to package a DLL that is older than any source file, so a stale
# build cannot become a release by accident. Build first:
#
#   cmake --build build            (Ninja)
#   cmake --build build --config Release   (Visual Studio generator; then
#                                           -BuildDir build/Release)

param(
	[string]$BuildDir = "build",
	[string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$cmake = Get-Content (Join-Path $root "CMakeLists.txt")
$line = $cmake | Where-Object { $_ -match '^\s*project\(OBVR\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)' } | Select-Object -First 1
if (-not $line) {
	throw "CMakeLists.txt has no project(OBVR VERSION x.y.z) line to read the version from"
}
$version = $Matches[1]

$dll = Join-Path $root (Join-Path $BuildDir "OBVR.dll")
$map = Join-Path $root (Join-Path $BuildDir "OBVR.map")
$ini = Join-Path $root "OBVR.ini"
if (-not (Test-Path $dll)) {
	throw "No DLL at $dll - build first (see the header of this script)"
}

# The OpenVR client, pinned: the hash is the one third_party/openvr/README.md
# records for the tag it came from, so a swapped or damaged file cannot ship.
$openvr = Join-Path $root "third_party\openvr\win32\openvr_api.dll"
$openvrLicense = Join-Path $root "third_party\openvr\LICENSE"
$openvrHash = "AB696E4F218A95B3E396BC310F9FE6485DF48C99C0969762083212B1E1F025A6"
if (-not (Test-Path $openvr) -or -not (Test-Path $openvrLicense)) {
	throw "third_party/openvr is incomplete - openvr_api.dll and LICENSE both belong there"
}
$found = (Get-FileHash -Algorithm SHA256 $openvr).Hash
if ($found -ne $openvrHash) {
	throw "third_party/openvr/win32/openvr_api.dll has SHA-256 $found, not the pinned $openvrHash"
}

$dllTime = (Get-Item $dll).LastWriteTimeUtc
$newer = Get-ChildItem (Join-Path $root "src") -Recurse -File |
	Where-Object { $_.LastWriteTimeUtc -gt $dllTime }
if ($newer) {
	$names = ($newer | Select-Object -First 5 | ForEach-Object { $_.FullName.Substring($root.Length + 1) }) -join ", "
	throw "The DLL is older than $($newer.Count) source file(s), e.g. $names - rebuild before packaging"
}

$stage = Join-Path $root (Join-Path $OutDir "OBVR-$version")
$plugins = Join-Path $stage "OBSE\Plugins"
if (Test-Path $stage) {
	Remove-Item -Recurse -Force $stage
}
New-Item -ItemType Directory -Force $plugins | Out-Null
Copy-Item $dll (Join-Path $plugins "OBVR.dll")
Copy-Item $ini (Join-Path $plugins "OBVR.ini")
Copy-Item -LiteralPath (Join-Path $root "assets\input") -Destination (Join-Path $plugins "OBVR_Input") -Recurse
Copy-Item (Join-Path $root "LICENSE") (Join-Path $plugins "OBVR-LICENSE.txt")
Copy-Item $openvr (Join-Path $plugins "openvr_api.dll")
Copy-Item $openvrLicense (Join-Path $plugins "openvr_api-LICENSE.txt")

Copy-Item -LiteralPath (Join-Path $root "assets\menus") -Destination (Join-Path $stage "Menus") -Recurse

$zip = Join-Path $root (Join-Path $OutDir "OBVR-$version.zip")
if (Test-Path $zip) {
	Remove-Item -Force $zip
}
Compress-Archive -Path @((Join-Path $stage "OBSE"), (Join-Path $stage "Menus")) -DestinationPath $zip
if (Test-Path $map) {
	Copy-Item $map (Join-Path $root (Join-Path $OutDir "OBVR-$version.map"))
}
Remove-Item -Recurse -Force $stage

$size = [math]::Round((Get-Item $zip).Length / 1KB)
Write-Output "OBVR $version packaged: $zip ($size KB)"
Write-Output "  contains OBSE/Plugins/OBVR.dll (built $($dllTime.ToLocalTime())), OBVR.ini, OBVR-LICENSE.txt, OBVR_Input/, openvr_api.dll with its license, and native Menus XML"
if (Test-Path $map) {
	Write-Output "  linker map beside it: OBVR-$version.map"
}
