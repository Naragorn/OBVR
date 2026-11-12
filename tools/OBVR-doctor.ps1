# OBVR doctor for Windows: checks that OBVR and what it needs are installed right.
#
# Double-click OBVR-doctor.bat next to this file (it runs this script with
# -ExecutionPolicy Bypass and keeps the window open). Or from PowerShell:
#
#   .\OBVR-doctor.ps1                        finds the game by itself
#   .\OBVR-doctor.ps1 -Game "D:\Oblivion"    or tell it where the game is
#   .\OBVR-doctor.ps1 -Mo2 "D:\MO2"          Mod Organizer 2: its folder or ModOrganizer.ini
#
# It only reads files and never changes anything. The same checks as
# obvr-doctor.py (Linux); both follow docs/self-check-spec.md. Written for
# Windows PowerShell 5.1, which every Windows 10 and 11 has.
param(
	[string]$Game = "",
	[string]$Mo2 = ""
)

$ErrorActionPreference = "Continue"

# ------------------------------------------------------------------ facts
# Each value has its evidence in docs/self-check-spec.md.
$OblivionAppId = "22330"
$MenuQueV16bSha256 = "B1F904137A1CDF888F1E4A2F24BD293CB17DD9EFFB1502021256C25FD5CFBC91"

# As core/SelfCheckLogic.h's kExpectedFiles (tests/SelfCheckAssetsTest.py keeps the lists the same).
$PluginFiles = @(
	"OBVR.ini",
	"OBVR_Controls.ini",
	"OBVR_Input\actions.json",
	"OBVR_Input\knuckles.json",
	"OBVR_Input\oculus_touch.json",
	"OBVR_Sounds\slap.wav",
	"OBVR_Stage\loading_dome_dark.obj",
	"OBVR_Stage\loading_dome_dark.mtl",
	"OBVR_Stage\loading_dome_dark.png"
)
$DataFiles = @(
	"Menus\generic\OBVR_AdjustHands.xml",
	"Menus\generic\OBVR_Controls.xml",
	"Menus\generic\OBVR_Onboarding.xml",
	"Menus\generic\OBVR_SelfCheck.xml",
	"Menus\generic\OBVR_Settings.xml",
	"Menus\generic\OBVR_Update.xml",
	"Menus\prefabs\OBVR\button_highlight.xml",
	"Textures\Effects\OBVR\CutCloth.dds",
	"Textures\Effects\OBVR\CutLeather.dds",
	"Textures\Effects\OBVR\CutSkin.dds",
	"Textures\Menus\OBVR\controls_1_left.dds",
	"Textures\Menus\OBVR\controls_1_left_index.dds",
	"Textures\Menus\OBVR\controls_1_right.dds",
	"Textures\Menus\OBVR\controls_1_right_index.dds",
	"Textures\Menus\OBVR\controls_2_left.dds",
	"Textures\Menus\OBVR\controls_2_right.dds",
	"Textures\Menus\OBVR\controls_3_left.dds",
	"Textures\Menus\OBVR\controls_3_left_index.dds",
	"Textures\Menus\OBVR\controls_3_right.dds",
	"Textures\Menus\OBVR\controls_3_right_index.dds",
	"Textures\Menus\OBVR\controls_4_left.dds",
	"Textures\Menus\OBVR\controls_4_right.dds",
	"Textures\Menus\OBVR\controls_5_left.dds",
	"Textures\Menus\OBVR\controls_5_left_index.dds",
	"Textures\Menus\OBVR\controls_5_right.dds",
	"Textures\Menus\OBVR\controls_5_right_index.dds",
	"Textures\Menus\OBVR\tpose.dds",
	"Meshes\characters\_male\specialanims\HandToHandStagger_OBVRMirror.kf",
	"Meshes\characters\_male\specialanims\OneHandStagger_OBVRMirror.kf",
	"Meshes\characters\_male\specialanims\StaffStagger_OBVRMirror.kf",
	"Meshes\characters\_male\specialanims\TwoHandStagger_OBVRMirror.kf"
)

$script:Results = New-Object System.Collections.ArrayList
function Add-Result([string]$State, [string]$Text, [string]$Fix = "") {
	[void]$script:Results.Add([pscustomobject]@{ State = $State; Text = $Text; Fix = $Fix })
}

# ------------------------------------------------------------------ readers

function Read-Bytes([string]$Path) {
	if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
	try { return [IO.File]::ReadAllBytes($Path) } catch { return $null }
}

function Read-Text([string]$Path) {
	$bytes = Read-Bytes $Path
	if ($null -eq $bytes) { return $null }
	return [Text.Encoding]::UTF8.GetString($bytes)
}

# The four numbers of a file's FileVersion (VS_FIXEDFILEINFO), or $null.
function Get-FileVersionParts([string]$Path) {
	try {
		$info = [Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
		if ($info.FileMajorPart -eq 0 -and $info.FileMinorPart -eq 0 -and $info.FileBuildPart -eq 0 -and $info.FilePrivatePart -eq 0) { return $null }
		return @($info.FileMajorPart, $info.FileMinorPart, $info.FileBuildPart, $info.FilePrivatePart)
	} catch { return $null }
}

# IMAGE_FILE_HEADER.Machine: 0x14C is 32-bit; $null for no PE file.
function Get-PeMachine([byte[]]$Bytes) {
	if ($null -eq $Bytes -or $Bytes.Length -lt 0x40 -or $Bytes[0] -ne 0x4D -or $Bytes[1] -ne 0x5A) { return $null }
	$pe = [BitConverter]::ToUInt32($Bytes, 0x3C)
	if ($pe + 6 -gt $Bytes.Length) { return $null }
	if ($Bytes[$pe] -ne 0x50 -or $Bytes[$pe + 1] -ne 0x45 -or $Bytes[$pe + 2] -ne 0 -or $Bytes[$pe + 3] -ne 0) { return $null }
	return [BitConverter]::ToUInt16($Bytes, $pe + 4)
}

function Test-BytesContain([byte[]]$Bytes, [string]$Ascii) {
	if ($null -eq $Bytes) { return $false }
	$text = [Text.Encoding]::GetEncoding(28591).GetString($Bytes)
	return $text.Contains($Ascii)
}

# dxvk.conf the way DXVK reads it: "key = value", '#' a comment, true/false in
# any case, the last line for a key wins.
function Read-DxvkConf([string]$Text) {
	$result = @{ DefragOff = $false; CachedOn = $false }
	foreach ($raw in ($Text -split "`n")) {
		$line = ($raw -split "#", 2)[0]
		if ($line -notmatch "=") { continue }
		$parts = $line -split "=", 2
		$key = $parts[0].Trim(); $value = $parts[1].Trim().ToLowerInvariant()
		if ($key -ceq "dxvk.enableMemoryDefrag") { $result.DefragOff = ($value -eq "false") }
		elseif ($key -ceq "d3d9.cachedWriteOnlyBuffers") { $result.CachedOn = ($value -eq "true") }
	}
	return $result
}

function Test-TrifleShadowOn([string]$Text) {
	foreach ($raw in ($Text -split "`n")) {
		$line = $raw.Trim()
		if ($line.StartsWith(";")) { continue }
		if ($line -match "^EnableFirstPersonShadow\s*=\s*(\S*)") { return $Matches[1].StartsWith("1") }
	}
	return $false
}

# A value as GetPrivateProfileIntA finds it: section and key in any case, the
# first line for the key, ';' lines skipped, a trailing comment cut; $null for none
# (obvr-doctor.py's ini_flag).
function Get-IniFlag([string]$Text, [string]$Section, [string]$Key) {
	$current = $null
	foreach ($raw in ($Text -split "`n")) {
		$line = $raw.Trim()
		if ($line -eq "" -or $line.StartsWith(";")) { continue }
		if ($line.StartsWith("[") -and $line.Contains("]")) {
			$current = $line.Substring(1, $line.IndexOf("]") - 1).Trim().ToLower()
			continue
		}
		if ($current -eq $Section.ToLower() -and $line.Contains("=")) {
			$at = $line.IndexOf("=")
			if ($line.Substring(0, $at).Trim().ToLower() -eq $Key.ToLower()) {
				return ($line.Substring($at + 1) -split ";")[0].Trim()
			}
		}
	}
	return $null
}

# Steam's text VDF into nested hashtables (case-insensitive keys, as PowerShell's are).
function ConvertFrom-Vdf([string]$Text) {
	$root = @{}
	$stack = New-Object System.Collections.Stack
	$stack.Push($root)
	$key = $null
	foreach ($m in [regex]::Matches($Text, '"((?:[^"\\]|\\.)*)"|([{}])')) {
		if ($m.Groups[2].Value -eq "{") {
			$child = @{}
			$name = if ($null -ne $key) { $key } else { "" }
			$stack.Peek()[$name] = $child
			$stack.Push($child)
			$key = $null
		} elseif ($m.Groups[2].Value -eq "}") {
			if ($stack.Count -gt 1) { [void]$stack.Pop() }
			$key = $null
		} elseif ($null -eq $key) {
			$key = $m.Groups[1].Value
		} else {
			$stack.Peek()[$key] = $m.Groups[1].Value.Replace("\\", "\")
			$key = $null
		}
	}
	return $root
}

# "off-all", "off-game" or "on": UserLocalConfigStore/system/EnableGameOverlay "0"
# is off for every game, UserLocalConfigStore/apps/<id>/OverlayAppEnable "0" off for this one.
function Get-OverlayState($LocalConfig, [string]$AppId) {
	$store = $LocalConfig["UserLocalConfigStore"]
	if ($null -eq $store) { return "on" }
	$system = $store["system"]
	if ($null -ne $system -and $system["EnableGameOverlay"] -eq "0") { return "off-all" }
	$apps = $store["apps"]
	if ($null -ne $apps -and $null -ne $apps[$AppId] -and $apps[$AppId]["OverlayAppEnable"] -eq "0") { return "off-game" }
	return "on"
}

function Get-Mo2Value([string]$Raw) {
	$value = $Raw.Trim()
	if ($value -match '^@ByteArray\((.*)\)$') { $value = $Matches[1] }
	if ($value.Length -ge 2 -and $value.StartsWith('"') -and $value.EndsWith('"')) { $value = $value.Substring(1, $value.Length - 2) }
	return $value.Replace("\\", "\")
}

function ConvertFrom-Mo2Ini([string]$Text) {
	$sections = @{ "" = @{} }
	$current = $sections[""]
	foreach ($raw in ($Text -split "`n")) {
		$line = $raw.Trim()
		if (-not $line -or $line.StartsWith(";") -or $line.StartsWith("#")) { continue }
		if ($line -match '^\[(.+)\]$') {
			if (-not $sections.ContainsKey($Matches[1])) { $sections[$Matches[1]] = @{} }
			$current = $sections[$Matches[1]]
			continue
		}
		$at = $line.IndexOf("=")
		if ($at -gt 0) { $current[$line.Substring(0, $at).Trim()] = Get-Mo2Value $line.Substring($at + 1) }
	}
	return $sections
}

# ------------------------------------------------------------------ the install

$script:Mo2Mods = @()

function Get-SteamRoots {
	$roots = @()
	try {
		$path = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam" -Name SteamPath -ErrorAction Stop).SteamPath
		if ($path) { $roots += $path.Replace("/", "\") }
	} catch { }
	foreach ($envName in @("ProgramFiles(x86)", "ProgramFiles")) {
		$base = [Environment]::GetEnvironmentVariable($envName)
		if ($base) { $roots += (Join-Path $base "Steam") }
	}
	return @($roots | Where-Object { Test-Path -LiteralPath $_ -PathType Container } | Select-Object -Unique)
}

function Get-SteamLibraries([string]$Root) {
	$libraries = @($Root)
	$vdf = Read-Text (Join-Path $Root "steamapps\libraryfolders.vdf")
	if ($null -ne $vdf) {
		$folders = (ConvertFrom-Vdf $vdf)["libraryfolders"]
		if ($null -ne $folders) {
			foreach ($entry in $folders.Values) {
				if ($entry -is [hashtable] -and $entry["path"]) { $libraries += $entry["path"] }
			}
		}
	}
	return @($libraries | Select-Object -Unique)
}

function Test-GameFolder([string]$Path) {
	return $Path -and (Test-Path -LiteralPath (Join-Path $Path "Oblivion.exe") -PathType Leaf)
}

function Get-DataFile([string]$GameDir, [string]$Relative) {
	$real = Join-Path (Join-Path $GameDir "Data") $Relative
	if (Test-Path -LiteralPath $real -PathType Leaf) { return $real }
	foreach ($mod in $script:Mo2Mods) {
		$candidate = Join-Path $mod $Relative
		if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
	}
	return $null
}

function Find-Mo2Ini([string]$ScriptDir) {
	if ($Mo2) {
		if (Test-Path -LiteralPath $Mo2 -PathType Leaf) { return @($Mo2, "the file you gave") }
		return @((Join-Path $Mo2 "ModOrganizer.ini"), "the folder you gave")
	}
	$dir = Split-Path -Parent $ScriptDir
	while ($dir) {
		$ini = Join-Path $dir "ModOrganizer.ini"
		if (Test-Path -LiteralPath $ini -PathType Leaf) { return @($ini, "a portable MO2 above the doctor") }
		$parent = Split-Path -Parent $dir
		if ($parent -eq $dir) { break }
		$dir = $parent
	}
	$local = [Environment]::GetEnvironmentVariable("LOCALAPPDATA")
	if ($local -and (Test-Path -LiteralPath (Join-Path $local "ModOrganizer") -PathType Container)) {
		foreach ($instance in (Get-ChildItem -LiteralPath (Join-Path $local "ModOrganizer") -Directory | Sort-Object Name)) {
			$ini = Join-Path $instance.FullName "ModOrganizer.ini"
			$text = Read-Text $ini
			if ($null -eq $text) { continue }
			$settings = ConvertFrom-Mo2Ini $text
			if ($settings.ContainsKey("General") -and $settings["General"]["gameName"] -eq "Oblivion") {
				return @($ini, "the MO2 instance $($instance.Name)")
			}
		}
	}
	return @($null, "")
}

# The selected profile's enabled mods, top priority first, after the overwrite folder.
function Read-Mo2([string]$Ini) {
	$text = Read-Text $Ini
	if ($null -eq $text) { return @{ Mods = @(); GamePath = $null; Note = "could not read $Ini" } }
	$settings = ConvertFrom-Mo2Ini $text
	$general = if ($settings.ContainsKey("General")) { $settings["General"] } else { @{} }
	$paths = if ($settings.ContainsKey("Settings")) { $settings["Settings"] } else { @{} }
	$base = if ($paths["base_directory"]) { $paths["base_directory"] } else { Split-Path -Parent $Ini }
	$expand = {
		param($Key, $Default)
		$value = if ($paths[$Key]) { $paths[$Key] } else { $Default }
		return $value.Replace("%BASE_DIR%", $base).Replace("/", "\")
	}
	$modsDir = & $expand "mod_directory" "%BASE_DIR%\mods"
	$profilesDir = & $expand "profiles_directory" "%BASE_DIR%\profiles"
	$overwrite = & $expand "overwrite_directory" "%BASE_DIR%\overwrite"
	$profile = if ($general["selected_profile"]) { $general["selected_profile"] } else { "Default" }
	$modlist = Read-Text (Join-Path (Join-Path $profilesDir $profile) "modlist.txt")
	if ($null -eq $modlist) {
		return @{ Mods = @(); GamePath = $general["gamePath"]; Note = "no modlist.txt for the profile '$profile' in $profilesDir" }
	}
	$mods = @()
	if (Test-Path -LiteralPath $overwrite -PathType Container) { $mods += $overwrite }
	foreach ($raw in ($modlist -split "`n")) {
		$line = $raw.Trim()
		if ($line.StartsWith("+")) {
			$folder = Join-Path $modsDir $line.Substring(1)
			if (Test-Path -LiteralPath $folder -PathType Container) { $mods += $folder }
		}
	}
	return @{ Mods = $mods; GamePath = $general["gamePath"]; Note = "MO2 profile '$profile', $($mods.Count) enabled mod folder(s) searched as Data" }
}

function Find-Game([string]$ScriptDir) {
	if ($Game) {
		$p = $Game
		if (Test-Path -LiteralPath $p -PathType Leaf) { $p = Split-Path -Parent $p }
		if (Test-GameFolder $p) { return @($p, "the folder you gave") }
		return @($null, "no Oblivion.exe in $p")
	}
	$dir = $ScriptDir
	for ($i = 0; $i -lt 5 -and $dir; $i++) {
		if (Test-GameFolder $dir) { return @($dir, "the doctor's own place") }
		$dir = Split-Path -Parent $dir
	}
	if (Test-GameFolder (Get-Location).Path) { return @((Get-Location).Path, "the current folder") }
	foreach ($root in Get-SteamRoots) {
		foreach ($library in Get-SteamLibraries $root) {
			$candidate = Join-Path $library "steamapps\common\Oblivion"
			if (Test-GameFolder $candidate) { return @($candidate, "the Steam library $library") }
		}
	}
	return @($null, "not found")
}

# ------------------------------------------------------------------ the checks

function Invoke-Checks([string]$GameDir, [string]$SteamRoot) {
	$exe = Join-Path $GameDir "Oblivion.exe"
	$v = Get-FileVersionParts $exe
	if ($null -eq $v) { Add-Result "INFO" "Oblivion.exe has no version to read - could not verify it is 1.2.0.416" }
	elseif ($v[0] -eq 1 -and $v[1] -eq 2 -and $v[2] -eq 0 -and $v[3] -eq 416) { Add-Result "OK" "Oblivion 1.2.0.416" }
	else { Add-Result "PROBLEM" "Oblivion.exe is version $($v -join '.'), OBVR needs 1.2.0.416" "Install the last official patch (the Steam version already has it)." }

	$xobse = Join-Path $GameDir "obse_1_2_416.dll"
	$loader = Join-Path $GameDir "obse_loader.exe"
	$missing = @()
	if (-not (Test-Path -LiteralPath $xobse -PathType Leaf)) { $missing += "obse_1_2_416.dll" }
	if (-not (Test-Path -LiteralPath $loader -PathType Leaf)) { $missing += "obse_loader.exe" }
	if ($missing.Count -gt 0) {
		Add-Result "PROBLEM" "xOBSE is missing ($($missing -join ', '))" "Copy xOBSE's files into the Oblivion folder: https://github.com/llde/xOBSE/releases/latest"
	} else {
		$xv = Get-FileVersionParts $xobse
		if ($null -eq $xv) { Add-Result "INFO" "xOBSE is there - could not read its version" }
		elseif ($xv[1] -gt 22 -or ($xv[1] -eq 22 -and $xv[2] -ge 13)) { Add-Result "OK" "xOBSE $($xv[1]).$($xv[2])" }
		else { Add-Result "PROBLEM" "xOBSE $($xv[1]).$($xv[2]) is too old" "Install xOBSE 22.13 or newer: https://github.com/llde/xOBSE/releases/latest" }
	}

	$d3d9 = Read-Bytes (Join-Path $GameDir "d3d9.dll")
	if ($null -eq $d3d9) {
		Add-Result "PROBLEM" "DXVK is missing (no d3d9.dll next to Oblivion.exe)" "From the DXVK gplasync zip's x32 folder, copy d3d9.dll next to Oblivion.exe."
	} elseif (Test-BytesContain $d3d9 "-gplasync") {
		Add-Result "OK" "DXVK gplasync"
	} elseif (Test-BytesContain $d3d9 "DXVK") {
		Add-Result "PROBLEM" "DXVK is there, but not the gplasync build" "Use d3d9.dll from the gplasync zip's x32 folder: https://gitlab.com/Ph42oN/dxvk-gplasync/-/releases"
	} else {
		Add-Result "PROBLEM" "the d3d9.dll next to Oblivion.exe is not DXVK" "Replace it with d3d9.dll from the gplasync zip's x32 folder."
	}
	if ($null -ne $d3d9) {
		$machine = Get-PeMachine $d3d9
		if ($null -ne $machine -and $machine -ne 0x14C) { Add-Result "PROBLEM" "d3d9.dll is the 64-bit one" "Take d3d9.dll from the zip's x32 folder." }
	}

	$conf = Read-Text (Join-Path $GameDir "dxvk.conf")
	if ($null -eq $conf) {
		Add-Result "PROBLEM" "dxvk.conf is missing" "Make a text file dxvk.conf next to Oblivion.exe with two lines: dxvk.enableMemoryDefrag = False  and  d3d9.cachedWriteOnlyBuffers = True"
	} else {
		$c = Read-DxvkConf $conf
		if ($c.DefragOff -and $c.CachedOn) { Add-Result "OK" "dxvk.conf has both lines" }
		else {
			$lacking = @()
			if (-not $c.DefragOff) { $lacking += "dxvk.enableMemoryDefrag = False" }
			if (-not $c.CachedOn) { $lacking += "d3d9.cachedWriteOnlyBuffers = True" }
			$what = if ($lacking.Count -gt 1) { "these lines" } else { "this line" }
			Add-Result "PROBLEM" "dxvk.conf lacks $($lacking -join ' and ')" "Add $what to dxvk.conf."
		}
	}
	if ($null -eq $conf -and (Test-Path -LiteralPath (Join-Path $GameDir "dxvk.conf.txt") -PathType Leaf)) {
		Add-Result "INFO" "found dxvk.conf.txt - rename it to dxvk.conf (Windows hides the .txt ending)"
	}

	$menuQue = Get-DataFile $GameDir "OBSE\Plugins\MenuQue.dll"
	$sub = Get-DataFile $GameDir "OBSE\Plugins\MenuQue\Submodule.Game.dll"
	if ($null -eq $menuQue) { Add-Result "PROBLEM" "MenuQue is not installed" "Install MenuQue v16b: https://www.nexusmods.com/oblivion/mods/32200" }
	elseif ($null -eq $sub) { Add-Result "PROBLEM" "MenuQue is incomplete (no MenuQue\Submodule.Game.dll)" "Install MenuQue v16b again, with its MenuQue folder." }
	elseif ((Get-FileHash -LiteralPath $sub -Algorithm SHA256).Hash -eq $MenuQueV16bSha256) { Add-Result "OK" "MenuQue v16b" }
	else { Add-Result "INFO" "MenuQue is there, but not the v16b file OBVR was tested with - could not verify its version" }

	$dll = Get-DataFile $GameDir "OBSE\Plugins\OBVR.dll"
	if ($null -eq $dll) { Add-Result "PROBLEM" "OBVR.dll is missing" "Unpack the OBVR zip into Oblivion's Data folder (or install it as a mod in MO2)." }
	else { Add-Result "OK" "OBVR.dll" }
	$missingFiles = @()
	foreach ($f in $PluginFiles) { if ($null -eq (Get-DataFile $GameDir ("OBSE\Plugins\" + $f))) { $missingFiles += $f } }
	foreach ($f in $DataFiles) { if ($null -eq (Get-DataFile $GameDir $f)) { $missingFiles += $f } }
	$total = $PluginFiles.Count + $DataFiles.Count
	if ($missingFiles.Count -gt 0) { Add-Result "PROBLEM" "$($missingFiles.Count) of OBVR's $total files missing, e.g. $($missingFiles[0])" "Unpack the OBVR zip into Data again, keeping its folders." }
	else { Add-Result "OK" "all $total OBVR files" }
	$openvr = Read-Bytes (Get-DataFile $GameDir "OBSE\Plugins\openvr_api.dll")
	if ($null -eq $openvr) { Add-Result "PROBLEM" "openvr_api.dll is missing next to OBVR.dll" "Unpack the OBVR zip again." }
	elseif ((Get-PeMachine $openvr) -ne 0x14C) { Add-Result "PROBLEM" "openvr_api.dll is not the 32-bit one" "Take openvr_api.dll from the OBVR zip." }
	else { Add-Result "OK" "openvr_api.dll, 32-bit" }

	# OBNG.dll comes with OBVR (2026-11-12): with it and its stereo on OBVR draws
	# both eyes in one pass; without, two passes - slower, still working.
	$slower = "VR still works, but draws the world twice (two passes) - slower"
	$obng = Read-Bytes (Get-DataFile $GameDir "OBSE\Plugins\OBNG.dll")
	if ($null -eq $obng) {
		Add-Result "PROBLEM" "OBNG.dll is missing next to OBVR.dll - $slower" "Unpack the OBVR zip again - OBNG.dll and OBNG.ini come with it."
	} elseif ((Get-PeMachine $obng) -ne 0x14C) {
		Add-Result "PROBLEM" "OBNG.dll is not the 32-bit one - $slower" "Take OBNG.dll from the OBVR zip."
	} else {
		$obngIni = Read-Text (Get-DataFile $GameDir "OBSE\Plugins\OBNG.ini")
		if ($null -eq $obngIni) {
			Add-Result "PROBLEM" "OBNG.ini is missing next to OBNG.dll - $slower" "Unpack the OBVR zip again (it brings OBNG.ini)."
		} else {
			$draw = Get-IniFlag $obngIni "Route" "StereoDraw"
			$eyes = Get-IniFlag $obngIni "Route" "StereoEyes"
			if ($draw -eq "1" -and $eyes -eq "2") {
				Add-Result "OK" "Single pass: OBNG.dll with StereoDraw=1, StereoEyes=2"
			} else {
				$d = if ($null -ne $draw) { $draw } else { "(none)" }
				$e = if ($null -ne $eyes) { $eyes } else { "(none)" }
				Add-Result "PROBLEM" "OBNG.ini has StereoDraw=$d, StereoEyes=$e - $slower" "In Data/OBSE/Plugins/OBNG.ini under [Route] set StereoDraw=1 and StereoEyes=2 (or take OBNG.ini from the OBVR zip)."
			}
		}
		$obvrIni = Read-Text (Get-DataFile $GameDir "OBSE\Plugins\OBVR.ini")
		if ($obvrIni) {
			$switch = Get-IniFlag $obvrIni "Render" "SinglePassStereo"
			if ($null -ne $switch -and $switch -ne "1") {
				Add-Result "INFO" "OBVR.ini has SinglePassStereo=${switch}: OBVR draws two passes on purpose (1, the default, uses OBNG's single pass)"
			}
		}
	}

	$conflicts = @(); $files = @()
	if ($null -ne (Get-DataFile $GameDir "OBSE\Plugins\OBSE_EnhancedCamera.dll")) { $conflicts += "Enhanced Camera"; $files += "OBSE_EnhancedCamera.dll" }
	if ($null -ne (Get-DataFile $GameDir "OBSE\Plugins\OblivionReloaded.dll")) { $conflicts += "Oblivion Reloaded"; $files += "OblivionReloaded.dll" }
	if ($conflicts.Count -gt 0) {
		$verb = if ($conflicts.Count -gt 1) { "do" } else { "does" }
		Add-Result "PROBLEM" "$($conflicts -join ' and ') $verb not work with OBVR" "Remove $($files -join ' and ') from Data\OBSE\Plugins."
	} else { Add-Result "OK" "no Enhanced Camera, no Oblivion Reloaded" }
	$trifle = Read-Text (Get-DataFile $GameDir "OBSE\Plugins\Trifle.ini")
	if ($null -ne $trifle -and (Test-TrifleShadowOn $trifle)) { Add-Result "PROBLEM" "Trifle hides your body in VR" "Set EnableFirstPersonShadow=0 in Data\OBSE\Plugins\Trifle.ini." }

	if (-not $SteamRoot) {
		Add-Result "INFO" "the game is not in a Steam library - Steam overlay and SteamVR not checked"
	} else {
		$configs = @(Get-ChildItem -LiteralPath (Join-Path $SteamRoot "userdata") -Directory -ErrorAction SilentlyContinue |
			ForEach-Object { Join-Path $_.FullName "config\localconfig.vdf" } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
		if ($configs.Count -eq 0) { Add-Result "INFO" "could not find Steam's localconfig.vdf - check the Steam overlay by hand" }
		else {
			$newest = $configs | Sort-Object { (Get-Item -LiteralPath $_).LastWriteTime } | Select-Object -Last 1
			$state = Get-OverlayState (ConvertFrom-Vdf (Read-Text $newest)) $OblivionAppId
			if ($state -eq "on") { Add-Result "PROBLEM" "the Steam overlay is on for Oblivion" "In Steam: right-click Oblivion > Properties > General, switch the overlay off." }
			elseif ($state -eq "off-all") { Add-Result "OK" "Steam overlay off (for all games)" }
			else { Add-Result "OK" "Steam overlay off" }
		}
		$steamVr = $null
		foreach ($library in Get-SteamLibraries $SteamRoot) {
			$candidate = Join-Path $library "steamapps\common\SteamVR"
			if (Test-Path -LiteralPath $candidate -PathType Container) { $steamVr = $candidate }
		}
		if ($steamVr) { Add-Result "OK" "SteamVR installed" } else { Add-Result "PROBLEM" "SteamVR is not installed" "Install SteamVR from Steam." }
	}

	$log = Join-Path $GameDir "OBVR.log"
	if (-not (Test-Path -LiteralPath $log -PathType Leaf)) {
		Add-Result "INFO" "OBVR has not run yet (no OBVR.log) - that's fine before the first start"
	} else {
		$text = Read-Text $log
		$age = (Get-Date) - (Get-Item -LiteralPath $log).LastWriteTime
		$when = if ($age.TotalDays -lt 1) { "today" } else { "$([int][Math]::Floor($age.TotalDays)) day(s) ago" }
		if ($text -and $text.Contains("OBVR ready")) { Add-Result "INFO" "OBVR.log: the last start ($when) loaded OBVR" }
		else { Add-Result "INFO" "OBVR.log: the last start ($when) did not get OBVR ready - see OBVR.log" }
		if ($text) {
			$path = $null
			foreach ($m in [regex]::Matches($text, "Single-pass stereo: OBNG took the eyes[^\r\n]*")) {
				$path = if ($m.Value.Contains("- single pass through OBNG")) { "single" } else { "dual" }
			}
			if ($path -eq "single") { Add-Result "INFO" "OBVR.log: the last start ($when) drew both eyes in one pass (OBNG)" }
			elseif ($path -eq "dual") { Add-Result "INFO" "OBVR.log: the last start ($when) drew the two passes (no single pass)" }
			foreach ($m in [regex]::Matches($text, "Self-check: (\S+) (PROBLEM|HINT) - ([^\r\n]*)")) {
				Add-Result "INFO" "in the game OBVR found: $($m.Groups[3].Value)"
			}
		}
	}
}

# ------------------------------------------------------------------ main

Write-Host "OBVR doctor - checks your install, changes nothing."
Write-Host ""
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$mo2Found = Find-Mo2Ini $scriptDir
$mo2Ini = $mo2Found[0]
$mo2 = $null
if ($mo2Ini -and (Test-Path -LiteralPath $mo2Ini -PathType Leaf)) {
	$mo2 = Read-Mo2 $mo2Ini
	$script:Mo2Mods = @($mo2.Mods)
}
$found = Find-Game $scriptDir
$gameDir = $found[0]; $how = $found[1]
if (-not $gameDir -and $null -ne $mo2 -and $mo2.GamePath -and (Test-GameFolder $mo2.GamePath)) {
	$gameDir = $mo2.GamePath; $how = "MO2's game path"
}
if (-not $gameDir) {
	Write-Host "[ PROBLEM ] Could not find the Oblivion folder ($how)."
	Write-Host "            Fix: run it from PowerShell with the folder: .\OBVR-doctor.ps1 -Game `"C:\path\to\Oblivion`""
	Write-Host ""
	Write-Host "1 problem found - fix it, then run the doctor again."
	exit 1
}
$steamRoot = $null
$gameFull = [IO.Path]::GetFullPath($gameDir).TrimEnd("\") + "\"
foreach ($root in Get-SteamRoots) {
	foreach ($library in Get-SteamLibraries $root) {
		$libraryFull = [IO.Path]::GetFullPath($library).TrimEnd("\") + "\"
		if ($gameFull.StartsWith($libraryFull, [StringComparison]::OrdinalIgnoreCase)) { $steamRoot = $root }
	}
}
Write-Host "Game folder: $gameDir  (found by $how)"
if ($null -ne $mo2) {
	Write-Host "Mod Organizer 2: $mo2Ini  ($($mo2Found[1])) - $($mo2.Note)"
	if ($script:Mo2Mods.Count -eq 0) {
		Write-Host "[ INFO    ] Could not read MO2's mod list - files inside MO2 mods may show as missing."
		Write-Host "            Run it with -Mo2 `"C:\path\to\your\MO2\instance`""
	}
}
Write-Host ""
Invoke-Checks $gameDir $steamRoot
foreach ($r in $script:Results) {
	Write-Host ("[ {0,-7} ] {1}" -f $r.State, $r.Text)
	if ($r.Fix) { Write-Host "            Fix: $($r.Fix)" }
}
$problems = @($script:Results | Where-Object { $_.State -eq "PROBLEM" }).Count
Write-Host ""
if ($problems -eq 0) { Write-Host "All good - start the game!" }
elseif ($problems -eq 1) { Write-Host "1 problem found - fix it, then run the doctor again." }
else { Write-Host "$problems problems found - fix them, then run the doctor again." }
if ($problems -gt 0) { exit 1 } else { exit 0 }
