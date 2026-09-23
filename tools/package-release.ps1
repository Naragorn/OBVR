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
#   Menus/Prefabs/OBVR/button_highlight.xml
#   OBSE/Plugins/OBVR.ini
#   OBSE/Plugins/OBVR-LICENSE.txt   (the GPL-3.0, which travels with every copy)
#
# The linker map is copied beside the archive as OBVR-<version>.map, for
# reading an OBVR.dll+offset from a report back to a function. Nothing else
# goes in: openvr_api.dll is SteamVR's and the README says where it is.
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
Write-Output "  contains OBSE/Plugins/OBVR.dll (built $($dllTime.ToLocalTime())), OBSE/Plugins/OBVR.ini, OBSE/Plugins/OBVR-LICENSE.txt and native Menus XML"
if (Test-Path $map) {
	Write-Output "  linker map beside it: OBVR-$version.map"
}
