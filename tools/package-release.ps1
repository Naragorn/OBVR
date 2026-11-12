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
#   Menus/Generic/OBVR_AdjustHands.xml
#   Menus/Generic/OBVR_Controls.xml
#   Menus/Generic/OBVR_SelfCheck.xml
#   Menus/Prefabs/OBVR/button_highlight.xml
#   Textures/Menus/OBVR/controls_*.dds   (the controls card's drawings)
#   OBSE/Plugins/OBVR.ini
#   OBSE/Plugins/OBVR_Controls.ini
#   OBSE/Plugins/OBVR-LICENSE.txt   (the GPL-3.0, which travels with every copy)
#   OBSE/Plugins/OBVR_Input/        (the SteamVR action manifest and bindings)
#   OBSE/Plugins/OBVR_Stage/        (the loading stage: the dome SteamVR shows while the game loads)
#   OBSE/Plugins/openvr_api.dll     (Valve's 32-bit OpenVR client, third_party/openvr)
#   OBSE/Plugins/openvr_api-LICENSE.txt   (its BSD-3-Clause, which binary copies carry)
#   OBSE/Plugins/OBVR-doctor.bat, OBVR-doctor.ps1, obvr-doctor.py   (the install check)
#   OBSE/Plugins/OBNG.dll, OBNG.ini, OBNG-LICENSE.txt   (OBNG: the single-pass stereo)
#
# OBNG comes from ..\OBNG (or -ObngDir): build-plugin\OBNG.dll,
# plugin\OBNG-player.ini (shipped as OBNG.ini, with StereoDraw=1 and
# StereoEyes=2 under [Route]) and LICENSE (GPL-3.0); the archive is refused
# while any is missing or wrong, unless -WithoutObng says it ships without on
# purpose (OBVR then draws the dual pass) - as package-release.py, whose
# obng_bundle tools/package-release-test.py covers.
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
	[string]$OutDir = "dist",
	# Appended to the version in the file names, e.g. -beta8 (as
	# package-release.py --tag).
	[string]$Tag = "",
	# OBNG's repository (default ..\OBNG beside this one).
	[string]$ObngDir = "",
	# Ship without OBNG.dll on purpose: OBVR then draws the dual pass.
	[switch]$WithoutObng
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$cmake = Get-Content (Join-Path $root "CMakeLists.txt")
$line = $cmake | Where-Object { $_ -match '^\s*project\(OBVR\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)' } | Select-Object -First 1
if (-not $line) {
	throw "CMakeLists.txt has no project(OBVR VERSION x.y.z) line to read the version from"
}
$version = $Matches[1] + $Tag

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

# The loading stage's models the DLL names (src/vr/LoadingStage.h,
# "OBVR_Stage\\<name>.obj"), each with its material and its atlas - as
# package-release.py refuses an archive without them (the .obj files were
# lost to .gitignore's *.obj once, 2026-10-30).
$header = Get-Content -Raw (Join-Path $root "src\vr\LoadingStage.h")
$models = @([regex]::Matches($header, 'OBVR_Stage\\\\([A-Za-z0-9_]+)\.obj') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
if ($models.Count -eq 0) { throw "src/vr/LoadingStage.h names no model - the check cannot run" }
foreach ($m in $models) {
	foreach ($suffix in ".obj", ".mtl", ".png") {
		if (-not (Test-Path (Join-Path $root "assets\stage\$m$suffix"))) {
			throw "assets/stage lacks $m$suffix - the loading stage the DLL names (src/vr/LoadingStage.h)"
		}
	}
}

# OBNG, every problem at once (package-release.py's obng_bundle).
$obngFiles = @()
$obngSha = $null
if (-not $WithoutObng) {
	if (-not $ObngDir) { $ObngDir = Join-Path (Split-Path -Parent $root) "OBNG" }
	$problems = @()
	if (-not (Test-Path -PathType Container $ObngDir)) {
		throw "OBNG cannot be bundled: no OBNG folder at $ObngDir - give it with -ObngDir, or -WithoutObng"
	}
	$obngDll = Join-Path $ObngDir "build-plugin\OBNG.dll"
	$obngIni = Join-Path $ObngDir "plugin\OBNG-player.ini"
	$obngLicense = Join-Path $ObngDir "LICENSE"
	if (-not (Test-Path $obngDll)) {
		$problems += "no OBNG.dll at $obngDll - build it (OBNG's tools/build-plugin.sh --no-deploy)"
	} else {
		$obngSha = (Get-FileHash -Algorithm SHA256 $obngDll).Hash.ToLower()
		$bytes = [System.IO.File]::ReadAllBytes($obngDll)
		$machine = $null
		if ($bytes.Length -ge 0x40 -and $bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A) {
			$pe = [BitConverter]::ToInt32($bytes, 0x3C)
			if ($pe + 6 -le $bytes.Length -and $bytes[$pe] -eq 0x50 -and $bytes[$pe + 1] -eq 0x45) {
				$machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
			}
		}
		if ($machine -ne 0x14C) { $problems += "$obngDll is not a 32-bit Windows DLL" }
		$obngTime = (Get-Item $obngDll).LastWriteTimeUtc
		$newerObng = @(foreach ($folder in "plugin", "src") {
			$f = Join-Path $ObngDir $folder
			if (Test-Path $f) {
				Get-ChildItem $f -Recurse -File | Where-Object { $_.Extension -in ".cpp", ".h", ".inc", ".txt" -and $_.LastWriteTimeUtc -gt $obngTime }
			}
		})
		if ($newerObng.Count -gt 0) {
			$problems += "OBNG.dll is older than $($newerObng.Count) OBNG source file(s), e.g. $($newerObng[0].Name) - rebuild OBNG first"
		}
	}
	if (-not (Test-Path $obngIni)) {
		$problems += "no player INI at $obngIni - it ships as OBSE/Plugins/OBNG.ini"
	} else {
		# GetPrivateProfileIntA's way: the first line for a key in [Route].
		$section = ""
		$values = @{}
		foreach ($raw in Get-Content $obngIni) {
			$line = $raw.Trim()
			if ($line -eq "" -or $line.StartsWith(";")) { continue }
			if ($line -match '^\[([^\]]*)\]') { $section = $Matches[1].Trim().ToLower(); continue }
			if ($section -eq "route" -and $line -match '^([^=]+)=(.*)$') {
				$key = $Matches[1].Trim().ToLower()
				if (-not $values.ContainsKey($key)) { $values[$key] = ($Matches[2] -split ";")[0].Trim() }
			}
		}
		$wrong = @()
		foreach ($pair in @(@("StereoDraw", "1"), @("StereoEyes", "2"))) {
			$v = $values[$pair[0].ToLower()]
			if ($v -ne $pair[1]) { $wrong += "$($pair[0])=$(if ($v) { $v } else { '(none)' })" }
		}
		if ($wrong.Count -gt 0) {
			$problems += "$obngIni has $($wrong -join ', ') under [Route] - the single pass needs StereoDraw=1 and StereoEyes=2"
		}
	}
	if (-not (Test-Path $obngLicense)) {
		$problems += "no licence at $obngLicense - OBNG.dll ships only with its GPL-3.0 text"
	}
	if ($problems.Count -gt 0) {
		throw ("OBNG cannot be bundled:`n  " + ($problems -join "`n  "))
	}
	$obngFiles = @(@($obngDll, "OBNG.dll"), @($obngIni, "OBNG.ini"), @($obngLicense, "OBNG-LICENSE.txt"))
}

$stage = Join-Path $root (Join-Path $OutDir "OBVR-$version")
$plugins = Join-Path $stage "OBSE\Plugins"
if (Test-Path $stage) {
	Remove-Item -Recurse -Force $stage
if ($obngSha) {
	Set-Content -Encoding ascii -Path (Join-Path $root (Join-Path $OutDir "OBVR-$version.obng.sha256")) -Value "$obngSha  OBNG.dll"
}
}
New-Item -ItemType Directory -Force $plugins | Out-Null
Copy-Item $dll (Join-Path $plugins "OBVR.dll")
Copy-Item $ini (Join-Path $plugins "OBVR.ini")
Copy-Item (Join-Path $root "OBVR_Controls.ini") (Join-Path $plugins "OBVR_Controls.ini")
Copy-Item -LiteralPath (Join-Path $root "assets\input") -Destination (Join-Path $plugins "OBVR_Input") -Recurse
Copy-Item -LiteralPath (Join-Path $root "assets\sounds") -Destination (Join-Path $plugins "OBVR_Sounds") -Recurse
Copy-Item -LiteralPath (Join-Path $root "assets\stage") -Destination (Join-Path $plugins "OBVR_Stage") -Recurse
Copy-Item (Join-Path $root "LICENSE") (Join-Path $plugins "OBVR-LICENSE.txt")
Copy-Item $openvr (Join-Path $plugins "openvr_api.dll")
Copy-Item $openvrLicense (Join-Path $plugins "openvr_api-LICENSE.txt")
# The doctor: double-click OBVR-doctor.bat (Windows) or run obvr-doctor.py (Linux).
foreach ($doctor in @("OBVR-doctor.bat", "OBVR-doctor.ps1", "obvr-doctor.py")) {
	Copy-Item (Join-Path $root (Join-Path "tools" $doctor)) (Join-Path $plugins $doctor)
}

foreach ($pair in $obngFiles) {
	Copy-Item $pair[0] (Join-Path $plugins $pair[1])
}

Copy-Item -LiteralPath (Join-Path $root "assets\menus") -Destination (Join-Path $stage "Menus") -Recurse
# The controls card's drawings (Textures/Menus/OBVR).
Copy-Item -LiteralPath (Join-Path $root "assets\textures") -Destination (Join-Path $stage "Textures") -Recurse

$zip = Join-Path $root (Join-Path $OutDir "OBVR-$version.zip")
if (Test-Path $zip) {
	Remove-Item -Force $zip
}
# Written entry by entry rather than by Compress-Archive: Windows
# PowerShell 5.1's Compress-Archive names the entries with backslashes
# (OBSE\Plugins\OBVR.dll, seen 2026-11-01), which Linux's unzip - a Proton
# player's - takes as part of the file name. Forward slashes and files only,
# as package-release.py writes them.
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::Open($zip, [System.IO.Compression.ZipArchiveMode]::Create)
try {
	foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File) {
		$entry = $file.FullName.Substring($stage.Length + 1).Replace('\', '/')
		[System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entry,
			[System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
	}
} finally {
	$archive.Dispose()
}
if (Test-Path $map) {
	Copy-Item $map (Join-Path $root (Join-Path $OutDir "OBVR-$version.map"))
}
Remove-Item -Recurse -Force $stage

$size = [math]::Round((Get-Item $zip).Length / 1KB)
Write-Output "OBVR $version packaged: $zip ($size KB)"
Write-Output "  contains OBSE/Plugins/OBVR.dll (built $($dllTime.ToLocalTime())), OBVR.ini, OBVR_Controls.ini, OBVR-LICENSE.txt, OBVR_Input/, openvr_api.dll with its license, native Menus XML and the controls card's drawings"
if (Test-Path $map) {
	Write-Output "  linker map beside it: OBVR-$version.map"
}
if ($obngSha) {
	Write-Output "  OBNG.dll bundled with OBNG.ini and OBNG-LICENSE.txt, SHA-256 $obngSha (OBVR-$version.obng.sha256)"
} else {
	Write-Output "  without OBNG.dll (-WithoutObng): OBVR draws the dual pass"
}
