# Finite, reversible runtime proof for the opt-in native-menu lifecycle probe.
# This script uses only diagnostic F9/F10 requests plus ordinary keyboard input;
# it never saves, changes a save, or claims controller/gesture coverage.

param(
	[int]$MenuWaitSec = 90,
	[int]$LoadWaitSec = 60,
	[int]$ScenarioWaitSec = 4,
	[ValidateSet('ObseLoader', 'Steam', 'SteamGame', 'SteamLauncher')]
	# Steam installs must use the Steam/steam-loader path. Keep ObseLoader as
	# an explicit option for retail/non-Steam diagnostics.
	[string]$LaunchMode = 'Steam',
	[ValidateSet('0', '1')]
	[string]$ProbeEnabled = '1',
	[ValidateSet('Built', 'Installed')]
	[string]$DllSource = 'Built',
	[int]$BaselineAliveSec = 30,
	# Optional legacy-menu load sequence for a vanilla-control comparison.
	# This is intentionally opt-in and is not part of the normal baseline.
	[switch]$RunLoadSequence,
	# Run the existing launcher/input driver without staging OBVR. This is a
	# bounded vanilla control and does not claim OBVR lifecycle telemetry.
	[switch]$NoObvr,
	# Observe native runtime diagnostics while the UI is driven externally.
	# Useful for the Computer Use runner; no synthetic keyboard input is sent.
	[switch]$ExternalInput,
	[int]$ExternalInputWaitSec = 300,
	[string]$ArtifactRoot = "$(Join-Path (Get-Location) 'artifacts/native-menu-runtime-20260921')"
)

$ErrorActionPreference = "Stop"
$logicPath = Join-Path (Get-Location) "tools/native-menu-lifecycle-logic.ps1"
if (-not (Test-Path -LiteralPath $logicPath)) { throw "Harness logic file not found: $logicPath" }
. $logicPath
$gameDir = "D:\SteamLibrary\steamapps\common\Oblivion"
$loaderPath = Join-Path $gameDir "obse_loader.exe"
$steamPath = 'C:\Steam\steam.exe'
$gamePath = Join-Path $gameDir "Oblivion.exe"
$pluginDir = Join-Path $gameDir "Data\OBSE\Plugins"
$installedDll = Join-Path $pluginDir "OBVR.dll"
$installedIni = Join-Path $pluginDir "OBVR.ini"
$installedLog = Join-Path $gameDir "OBVR.log"
$installedObseLog = Join-Path $gameDir "obse.log"
$installedSteamLog = Join-Path $gameDir "obse_steam_loader.log"
$installedLoaderLog = Join-Path $gameDir "obse_loader.log"
$builtDll = Join-Path (Get-Location) "build-msvc\Release\OBVR.dll"
$originalDllSha256 = '90007FC6A4FB432A5ADD857E1E66FBCA89A6A6D43F82CB3CF4EB897E01087175'

$builtExists = Test-Path -LiteralPath $builtDll
$installedSourceHash = if (Test-Path -LiteralPath $installedDll) {
	(Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash
} else { 'MISSING' }
$dllSourceDecision = Get-DllSourceDecision -Source $DllSource -BuiltExists $builtExists `
	-InstalledHash $installedSourceHash -ExpectedInstalledHash $originalDllSha256
if (-not $NoObvr -and $dllSourceDecision -eq 'BuiltMissing') { throw "Built probe DLL not found: $builtDll" }
if (-not $NoObvr -and $dllSourceDecision -eq 'InstalledHashMismatch') {
	throw "Installed DLL source hash mismatch before backup: expected=$originalDllSha256 observed=$installedSourceHash"
}
if ($NoObvr -and (Test-Path -LiteralPath $installedDll)) {
	throw "No-OBVR control requires OBVR.dll to be absent before launch"
}
if (-not (Test-Path -LiteralPath $loaderPath)) { throw "OBSE loader not found: $loaderPath" }
if ($LaunchMode -eq 'Steam' -and -not (Test-Path -LiteralPath $steamPath)) { throw "Steam executable not found: $steamPath" }
if (-not (Test-Path -LiteralPath $gamePath)) { throw "Oblivion executable not found: $gamePath" }

$existing = @(Get-Process Oblivion,OblivionLauncher,obse_loader -ErrorAction SilentlyContinue)
if ($existing.Count -ne 0) {
	throw "Oblivion or obse_loader is already running; this harness will not touch that session."
}

[IO.Directory]::CreateDirectory($ArtifactRoot) | Out-Null
$backupDir = Join-Path $ArtifactRoot "installed-backup"
[IO.Directory]::CreateDirectory($backupDir) | Out-Null
$backupDll = Join-Path $backupDir "OBVR.dll"
$backupIni = Join-Path $backupDir "OBVR.ini"
$backupLog = Join-Path $backupDir "OBVR.log"
$runLog = Join-Path $ArtifactRoot "run.log"
$evidenceLog = Join-Path $ArtifactRoot "OBVR-runtime.log"
$evidenceObseLog = Join-Path $ArtifactRoot "obse.log"
$evidenceSteamLog = Join-Path $ArtifactRoot "obse_steam_loader.log"
$evidenceLoaderLog = Join-Path $ArtifactRoot "obse_loader.log"
$logBaseline = @{}

function Hash-File([string]$path) {
	if (-not (Test-Path -LiteralPath $path)) { return "MISSING" }
	return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}

function Record([string]$line) {
	# Diagnostics must not become a caller's return value (for example, the
	# Boolean returned by Wait-Log after a transient sharing violation).
	$line | Tee-Object -FilePath $runLog -Append | Out-Host
}

function Image-Path([int]$id) {
	try {
		return (Get-CimInstance Win32_Process -Filter "ProcessId = $id").ExecutablePath
	} catch {
		return $null
	}
}

function Set-IniValue([string]$text, [string]$sectionName, [string]$key, [string]$value) {
	$lines = $text -split '\r?\n'
	$inSection = $false
	$found = $false
	$out = New-Object System.Collections.Generic.List[string]
	foreach ($line in $lines) {
		if ($line -match '^\s*\[([^]]+)\]\s*$') {
			if ($inSection -and -not $found) {
				$out.Add("$key=$value")
				$found = $true
			}
			$inSection = $Matches[1].Trim().Equals($sectionName, [StringComparison]::OrdinalIgnoreCase)
			$out.Add($line)
			if ($inSection -and -not $found) {
				$out.Add("$key=$value")
				$found = $true
			}
			continue
		}
		if ($inSection -and $line -match "^\s*$([regex]::Escape($key))\s*=") {
			$out.Add("$key=$value")
			$found = $true
		} else {
			$out.Add($line)
		}
	}
	if ($inSection -and -not $found) { $out.Add("$key=$value") }
	return ($out -join "`r`n")
}

function Focus-Game {
	$p = Get-Process Oblivion -ErrorAction SilentlyContinue
	if ($p -and $p.MainWindowHandle -ne 0) {
		[ObvrNativeMenuProbe]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
	}
}

function Wait-Log([string]$pattern, [int]$seconds) {
	$deadline = (Get-Date).AddSeconds($seconds)
	while ((Get-Date) -lt $deadline) {
		if (Record-StartedExit) { return $false }
		try {
			$exists = Test-Path -LiteralPath $installedLog
			if ($exists) {
				$current = Read-SharedLogSnapshot $installedLog
				$before = $logBaseline[$installedLog]
				$currentHash = $current.Hash
				$evidenceState = Get-LogEvidenceState -Before $before -Exists $true `
					-CurrentHash $currentHash -CurrentLastWriteTimeUtc $current.LastWriteTimeUtc
				$changedSinceRunStart = $evidenceState -eq 'Fresh'
				$hit = $changedSinceRunStart -and
					$current.Text.Contains($pattern)
				if ((Get-LogMarkerDecision -Exists $exists -ChangedSinceBaseline $changedSinceRunStart `
					-ContainsMarker $hit) -eq 'Fresh') { return -not (Record-StartedExit) }
			}
		} catch {
			# OBVR can hold its log with an exclusive write handle while flushing.
			# A transient read failure is not a missing marker; retry until the
			# bounded deadline and let the exit observation classify the process.
			Record "log read retry pattern=$pattern reason=$($_.Exception.Message)"
		}
		Start-Sleep -Milliseconds 500
	}
	return $false
}

function Capture-FreshLog([string]$source, [string]$destination) {
	$before = $logBaseline[$source]
	if (-not (Test-Path -LiteralPath $source)) {
		[IO.File]::WriteAllText($destination, "MISSING: $source`r`n", [Text.UTF8Encoding]::new($false))
		Record "log evidence missing: $source"
		return
	}
	$current = Get-Item -LiteralPath $source
	$currentHash = Hash-File $source
	$state = Get-LogEvidenceState -Before $before -Exists $true -CurrentHash $currentHash `
		-CurrentLastWriteTimeUtc $current.LastWriteTimeUtc
	if ($state -eq 'Fresh') {
		Copy-Item -LiteralPath $source -Destination $destination -Force
		Record "fresh log evidence copied: $source sha256=$currentHash lastWriteUtc=$($current.LastWriteTimeUtc.ToString('o'))"
	} else {
		[IO.File]::WriteAllText($destination,
			"STALE: no change from pre-run log; source=$source sha256=$currentHash lastWriteUtc=$($current.LastWriteTimeUtc.ToString('o'))`r`n",
			[Text.UTF8Encoding]::new($false))
		Record "stale log evidence marked: $source sha256=$currentHash"
	}
}

function Restore-TrackedFile([string]$source, [string]$destination, [string]$label) {
	try {
		Copy-Item -LiteralPath $source -Destination $destination -Force
		$matches = (Hash-File $source) -eq (Hash-File $destination)
		$decision = Get-RestorationDecision -Needed $true -CopySucceeded $true -HashMatches $matches
		if ($decision -eq 'Restored') {
			Record "restored $label sha256=$(Hash-File $destination)"
			return $true
		}
		Record "RESTORE UNSAFE ${label}: copy completed but hash verification failed"
		$script:restoreUnsafe = $true
		return $false
	} catch {
		Record "RESTORE UNSAFE ${label}: $($_.Exception.Message)"
		$script:restoreUnsafe = $true
		return $false
	}
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class ObvrNativeMenuProbe {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
    public static void Press(byte vk, byte scan) {
        keybd_event(vk, scan, 0, IntPtr.Zero);
        System.Threading.Thread.Sleep(90);
        keybd_event(vk, scan, 2, IntPtr.Zero);
    }
    public static void PressExtended(byte vk, byte scan) {
        keybd_event(vk, scan, 1, IntPtr.Zero);
        System.Threading.Thread.Sleep(90);
        keybd_event(vk, scan, 3, IntPtr.Zero);
    }
}
"@

$started = $null
$launchProcess = $null
$startedLoaderId = 0
$startedLauncherId = 0
$startedExitRecorded = $false
$restoreUnsafe = $false
$logQuarantined = $false
$restoreNeeded = $false

function Record-StartedExit {
	# Logging is one-shot, but an observed exit remains true for later callers.
	if ($script:startedExitRecorded) { return $true }
	if ($null -eq $started) { return $false }
	$observed = $started
	if ($null -ne $launchProcess -and $launchProcess.Id -eq $started.Id) { $observed = $launchProcess }
	try { $observed.Refresh() } catch { return $false }
	$hasExited = $false
	try { $hasExited = [bool]$observed.HasExited } catch { return $false }
	$exitCode = 'unavailable'
	$exitTime = 'unavailable'
	$exitCodeReadable = $false
	if ($hasExited) {
		try { $exitCode = $observed.ExitCode; $exitCodeReadable = $null -ne $exitCode } catch { $exitCode = "error:$($_.Exception.GetType().Name)" }
	}
	$exitEvidence = Get-ProcessExitEvidence -ProcessAvailable $true -HasExited $hasExited `
		-ExitCodeReadable $exitCodeReadable
	if ($exitEvidence -eq 'Running') { return $false }
	if ($exitEvidence -eq 'Unavailable') { return $false }
	try { $exitTime = $observed.ExitTime.ToString('o') } catch { $exitTime = "error:$($_.Exception.GetType().Name)" }
	Record "Oblivion exited before probe completion: pid=$($started.Id) exitEvidence=$exitEvidence exitCode=$exitCode exitTime=$exitTime observedAt=$(Get-Date -Format o)"
	$script:startedExitRecorded = $true
	return $true
}

function Close-OwnedProcess([int]$id, [string]$expectedPath, [string]$label, [int]$timeoutMs) {
	$p = if ($id -ne 0) { Get-Process -Id $id -ErrorAction SilentlyContinue } else { $null }
	$owned = $id -ne 0
	$pathMatches = $null -ne $p -and (Image-Path $p.Id) -eq $expectedPath
	$alreadyExited = $null -eq $p
	$initial = Get-OwnedProcessCleanupAction -Owned $owned -PathMatches $pathMatches `
		-AlreadyExited $alreadyExited -GracefulCloseSucceeded $false
	if ($initial -eq 'Skip') {
		Record "cleanup decision=Skip label=$label owned=$owned pathMatches=$pathMatches"
		return
	}
	Record "cleanup decision=Close label=$label pid=$id"
	$p.CloseMainWindow() | Out-Null
	$closed = $p.WaitForExit($timeoutMs)
	$action = Get-OwnedProcessCleanupAction -Owned $owned -PathMatches $pathMatches `
		-AlreadyExited $false -GracefulCloseSucceeded $closed
	if ($action -eq 'Closed') {
		Record "cleanup decision=Closed label=$label pid=$id"
		return
	}
	Record "cleanup decision=Force label=$label pid=$id"
	$p = Get-Process -Id $id -ErrorAction SilentlyContinue
	if ($p -and (Image-Path $p.Id) -eq $expectedPath) {
		Stop-Process -Id $id -Force
		Record "cleanup force-stopped owned label=$label pid=$id"
	}
}

function Wait-ExternalScenarios {
	$signal = Join-Path $ArtifactRoot 'external-input-complete.signal'
	if (Test-Path -LiteralPath $signal) { throw 'External-input completion signal already exists' }
	Record "external input ready; completion signal=$signal"
	$externalDeadline = (Get-Date).AddSeconds($ExternalInputWaitSec)
	while ($true) {
		$decision = Get-ExternalInputDecision -ProcessExited (Record-StartedExit) `
			-SignalPresent (Test-Path -LiteralPath $signal) -TimedOut ((Get-Date) -ge $externalDeadline)
		if ($decision -eq 'Exited') { throw 'Game exited during external-input scenarios' }
		if ($decision -eq 'Finished') {
			Record 'External-input scenario driver finished; inspect native logs for outcomes'
			return
		}
		if ($decision -eq 'Timeout') { throw 'External-input scenario driver timed out' }
		Start-Sleep -Milliseconds 500
	}
}
try {
	if (-not $NoObvr) {
		Copy-Item -LiteralPath $installedDll -Destination $backupDll -Force
		Copy-Item -LiteralPath $installedIni -Destination $backupIni -Force
		if (Test-Path -LiteralPath $installedLog) { Copy-Item -LiteralPath $installedLog -Destination $backupLog -Force }
		$restoreNeeded = $true
	}
	Record "run started $(Get-Date -Format o)"
	Record "control mode=$(if($NoObvr){'NoObvr'}else{'ObvrProbe'})"
	Record "game=$gamePath"
	Record "expected exe sha256=A8F313845C1545E9A60E1E995961EEF4C033115DA9443F6D756341DF3C2B7DC6"
	Record "observed exe sha256=$(Hash-File $gamePath)"
	Record "installed dll sha256=$(Hash-File $installedDll)"
	if (Test-Path -LiteralPath $builtDll) { Record "built dll sha256=$(Hash-File $builtDll)" }
	Record "installed ini sha256=$(Hash-File $installedIni)"
	foreach ($logPath in @($installedLog, $installedObseLog, $installedSteamLog, $installedLoaderLog)) {
		if (Test-Path -LiteralPath $logPath) {
			$logFile = Get-Item -LiteralPath $logPath
			$logBaseline[$logPath] = @{ Hash = (Hash-File $logPath); LastWriteTimeUtc = $logFile.LastWriteTimeUtc }
			Record "pre-run log $logPath sha256=$($logBaseline[$logPath].Hash) lastWriteUtc=$($logFile.LastWriteTimeUtc.ToString('o'))"
		} else {
			Record "pre-run log missing $logPath"
		}
	}
	if (-not $NoObvr -and (Test-Path -LiteralPath $installedLog)) {
		Remove-Item -LiteralPath $installedLog -Force
		$logQuarantined = $true
		Record "quarantined pre-run OBVR.log; stale markers cannot satisfy this run"
	}

	if (-not $NoObvr) {
		$ini = [IO.File]::ReadAllText($installedIni)
		$ini = Set-IniValue $ini "Camera" "HookEnabled" "0"
		$ini = Set-IniValue $ini "Debug" "NativeMenuLifecycleProbe" $ProbeEnabled
		$ini = Set-IniValue $ini "Debug" "MenuWorldProbe" "0"
		[IO.File]::WriteAllText($installedIni, $ini, [Text.UTF8Encoding]::new($false))
		$stagedDll = if ($DllSource -eq 'Installed') { $backupDll } else { $builtDll }
		$stagedSourceHash = Hash-File $stagedDll
		if ($DllSource -eq 'Installed' -and $stagedSourceHash -ne $originalDllSha256) {
			throw "Installed DLL source hash mismatch: expected=$originalDllSha256 observed=$stagedSourceHash"
		}
		Copy-Item -LiteralPath $stagedDll -Destination $installedDll -Force
		$stagedInstalledHash = Hash-File $installedDll
		if ($stagedInstalledHash -ne $stagedSourceHash) {
			throw "Staged DLL verification failed: source=$stagedSourceHash installed=$stagedInstalledHash"
		}
		Record "dll source=$DllSource sourceSha256=$stagedSourceHash stagedSha256=$stagedInstalledHash"
		$stagedProbeCount = @(Select-String -LiteralPath $installedIni -Pattern '^NativeMenuLifecycleProbe=1$').Count
		Record "probe config staged: Camera.HookEnabled=0 Debug.NativeMenuLifecycleProbe=$ProbeEnabled keyCount=$stagedProbeCount"
	} else {
		Record "no-OBVR control: OBVR.dll was absent before launch; no plugin or ini was staged"
	}

	$beforeIds = @(Get-Process Oblivion -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
	if ($LaunchMode -eq 'Steam') {
		Record "starting Steam app 22330 through $steamPath"
		Start-Process -FilePath $steamPath -ArgumentList '-applaunch', '22330' -WorkingDirectory $gameDir | Out-Null
	} elseif ($LaunchMode -eq 'SteamLauncher') {
		Record 'starting installed launcher with inherited diagnostic environment'
		$launcherPath = Join-Path $gameDir 'OblivionLauncher.exe'
		$launchProcess = Start-Process -FilePath $launcherPath -WorkingDirectory $gameDir -PassThru
		$startedLauncherId = $launchProcess.Id
	} elseif ($LaunchMode -eq 'SteamGame') {
		Record "starting Oblivion.exe directly with Steam resident for obse_steam_loader.dll"
		$launchProcess = Start-Process -FilePath $gamePath -WorkingDirectory $gameDir -PassThru
	} else {
		Record "starting obse_loader"
		$launchProcess = Start-Process -FilePath $loaderPath -WorkingDirectory $gameDir -PassThru
	}
	$deadline = (Get-Date).AddSeconds($MenuWaitSec)
	while ((Get-Date) -lt $deadline -and $null -eq $started) {
		$p = Get-Process Oblivion -ErrorAction SilentlyContinue |
			Where-Object { $beforeIds -notcontains $_.Id } | Select-Object -First 1
		if ($p) {
			$started = $p
			# Retain an OS handle while alive so exit status remains observable.
			try { $null = $started.Handle } catch { Record "process handle unavailable: $($_.Exception.Message)" }
			$loader = Get-Process obse_loader -ErrorAction SilentlyContinue | Select-Object -First 1
			if ($loader) { $startedLoaderId = $loader.Id }
			$startStamp = "unavailable"
			try { if ($null -ne $p.StartTime) { $startStamp = $p.StartTime.ToString('o') } } catch {}
			$pathStamp = "unavailable"
			try { if ($null -ne $p.Path) { $pathStamp = $p.Path } } catch {}
			Record "started Oblivion pid=$($p.Id) start=$startStamp path=$pathStamp"
			break
		}
		if ($LaunchMode -eq 'Steam') {
			$launcher = Get-Process OblivionLauncher -ErrorAction SilentlyContinue |
				Where-Object { (Image-Path $_.Id) -eq (Join-Path $gameDir 'OblivionLauncher.exe') } |
				Select-Object -First 1
			if ($launcher -and $startedLauncherId -eq 0) {
				$startedLauncherId = $launcher.Id
				Record "Steam launch opened OblivionLauncher pid=$startedLauncherId; Play requires an interactive launcher action"
			}
		}
		Start-Sleep -Seconds 1
	}
	if ($null -eq $started) { throw "Oblivion did not start within $MenuWaitSec seconds" }

	if ($NoObvr) {
		$windowVisible = $false
		try { $windowVisible = $started.MainWindowHandle -ne 0 } catch {}
		$processExited = Record-StartedExit
		$decision = Get-NoObvrControlDecision -ProcessExited $processExited -WindowVisible $windowVisible
		Record "no-OBVR startup decision=$decision windowVisible=$windowVisible"
		if ($decision -eq 'Exited') { throw 'Oblivion exited before the no-OBVR control became interactive' }
		if ($ExternalInput) { Wait-ExternalScenarios; return }
		if ($RunLoadSequence) {
			# Leave a bounded visual checkpoint at the real main menu before the
			# existing 90-ms input driver sends the non-saving sequence.
			Record 'no-OBVR load sequence checkpoint delay=5s'
			Start-Sleep -Seconds 5
			Record 'no-OBVR load sequence enabled; sending the existing non-saving Continue path'
			Focus-Game
			[ObvrNativeMenuProbe]::PressExtended(0x28, 0x50)
			Start-Sleep -Milliseconds 800
			[ObvrNativeMenuProbe]::Press(0x0D, 0x1C)
			Start-Sleep -Seconds 3
			[ObvrNativeMenuProbe]::Press(0x0D, 0x1C)
			Record "no-OBVR load sequence sent; waiting $LoadWaitSec seconds"
			Start-Sleep -Seconds $LoadWaitSec
			try { $started.Refresh() } catch {}
			$loadDecision = Get-LoadSequenceDecision -Enabled $true -ProcessExited ([bool]$started.HasExited)
			if ($loadDecision -eq 'Exited') { throw 'Oblivion exited during the no-OBVR load sequence' }
			Record 'no-OBVR load-input sequence ended with process alive; Continue activation and save/world load are NOT verified'
		}
		return
	}

	if ($ProbeEnabled -eq '0') {
		Record "default-off comparison: lifecycle probe disabled; no Update hook marker or scenario input expected"
		# Startup and post-start stability are separate intervals. A short
		# stability observation must not truncate the allowed plugin startup.
		$pluginLoaded = Wait-Log "OBVR 1 - Load" $MenuWaitSec
		if (-not $pluginLoaded) {
			$startup = Get-ProbeStartupDecision -PluginMarker $false -UpdateMarker $false `
				-ProcessExited (Record-StartedExit) -ExitCode $null
			throw "Default-off plugin startup failed: $startup"
		}
		# The current candidate emits a dedicated disabled-hook marker. The
		# original installed DLL predates that line, so its verified config dump
		# is the compatible provenance marker for the same HookEnabled=0 setting.
		$cameraMarker = if ($DllSource -eq 'Installed') { "Config: HookEnabled=0" } `
			else { "Camera hook disabled by configuration" }
		$cameraDisabled = Wait-Log $cameraMarker $MenuWaitSec
		if (Record-StartedExit) { throw 'Oblivion exited during default-off startup' }
		Record "baseline markers: plugin='OBVR 1 - Load' config='$cameraMarker' source=$DllSource"
		$windowVisible = $false
		$moduleLoaded = $false
		$baselineDeadline = (Get-Date).AddSeconds($BaselineAliveSec)
		while ((Get-Date) -lt $baselineDeadline) {
			$p = Get-Process -Id $started.Id -ErrorAction SilentlyContinue
			if ($null -eq $p) { break }
			try { $p.Refresh() } catch {}
			$windowVisible = $p.MainWindowHandle -ne 0
			try { $moduleLoaded = @($p.Modules | Where-Object { $_.ModuleName -ieq 'OBVR.dll' }).Count -gt 0 } catch {
				Record "baseline module enumeration unavailable: $($_.Exception.Message)"
			}
			Start-Sleep -Seconds 1
		}
		# Capture the real process exit status before readiness is classified.  The
		# baseline intentionally has no Update marker, so this is the only bounded
		# exit observation available when the game dies before a stable window.
		$processExited = Record-StartedExit
		$aliveAfterInterval = $null -ne (Get-Process -Id $started.Id -ErrorAction SilentlyContinue)
		$decision = Get-BaselineReadinessDecision -PluginLoadMarker $pluginLoaded `
			-CameraDisabledMarker $cameraDisabled -WindowVisible $windowVisible `
			-ModuleLoaded $moduleLoaded -AliveAfterInterval $aliveAfterInterval
		Record "baseline decision=$decision pluginLoaded=$pluginLoaded cameraDisabled=$cameraDisabled " +
			"windowVisible=$windowVisible moduleLoaded=$moduleLoaded aliveAfterInterval=$aliveAfterInterval"
		if ($decision -ne 'Ready') { throw "Default-off baseline was not ready: $decision" }
		if ($RunLoadSequence) {
			Record "baseline load sequence enabled; sending the existing non-saving Continue path"
			Focus-Game
			[ObvrNativeMenuProbe]::PressExtended(0x28, 0x50)
			Start-Sleep -Milliseconds 800
			[ObvrNativeMenuProbe]::Press(0x0D, 0x1C)
			Start-Sleep -Seconds 3
			[ObvrNativeMenuProbe]::Press(0x0D, 0x1C)
			Record "baseline load sequence sent; waiting $LoadWaitSec seconds"
			Start-Sleep -Seconds $LoadWaitSec
			try { $started.Refresh() } catch {}
			$loadDecision = Get-LoadSequenceDecision -Enabled $true -ProcessExited ([bool]$started.HasExited)
			if ($loadDecision -eq 'Exited') { throw "Oblivion exited during the baseline load sequence" }
			Record "baseline load-input sequence ended with process alive; Continue activation and save/world load are NOT verified"
		}
		if ($ExternalInput) { Wait-ExternalScenarios }
		return
	}
	$probeInstalledMarker = "Native menu probe: guarded lifecycle and query seams installed"
	$pluginObserved = Wait-Log $probeInstalledMarker $MenuWaitSec
	if (-not $pluginObserved) {
		$processExited = Record-StartedExit
		$decision = Get-ProbeStartupDecision -PluginMarker $false -UpdateMarker $false `
			-ProcessExited $processExited -ExitCode $null
		Record "startup decision=$decision"
		Record "BLOCKED: probe installation marker not observed"
		throw "Probe did not initialize after OBSE loaded the plugin"
	}
	Record "OBVR probe/plugin initialization observed"
	$processExited = Record-StartedExit
	if ($processExited -or -not (Get-Process -Id $started.Id -ErrorAction SilentlyContinue)) {
		Record "startup decision=$(Get-ProbeStartupDecision -PluginMarker $true -UpdateMarker $false -ProcessExited $true -ExitCode $null)"
		Record "BLOCKED: Oblivion exited before the first game Update"
		throw "Oblivion exited before the probe could establish its game Update thread"
	}
	$updateObserved = Wait-Log "Native menu probe: game Update thread established" $MenuWaitSec
	if (-not $updateObserved) {
		$processExited = Record-StartedExit
		Record "startup decision=$(Get-ProbeStartupDecision -PluginMarker $true -UpdateMarker $false -ProcessExited $processExited -ExitCode $null)"
		Record "BLOCKED: Update thread establishment marker not observed"
		throw "Probe did not establish its game Update thread"
	}
	Record "Update thread establishment observed"
	if ($ExternalInput) {
		Wait-ExternalScenarios
		return
	}
	Start-Sleep -Seconds 5

	# The historical Down/Enter/Enter sequence has never established a causal
	# Continue selection on this installation. Keep it explicit and experimental.
	if ($RunLoadSequence) {
		Focus-Game
		[ObvrNativeMenuProbe]::PressExtended(0x28, 0x50)
		Start-Sleep -Milliseconds 800
		[ObvrNativeMenuProbe]::Press(0x0D, 0x1C)
		Start-Sleep -Seconds 3
		[ObvrNativeMenuProbe]::Press(0x0D, 0x1C)
		Record 'Experimental load inputs sent; Continue activation is NOT verified'
	}
	Record "Waiting up to $LoadWaitSec seconds for native gameplay evidence; load a save through the game UI"
	$gameplayDeadline = (Get-Date).AddSeconds($LoadWaitSec)
	do {
		$liveSnapshot = Read-SharedLogSnapshot $installedLog
		$readiness = Get-NativeScenarioReadiness (Record-StartedExit) $liveSnapshot.Text
		if ($readiness -eq 'Exited') { throw 'Game exited before native gameplay was observed' }
		if ($readiness -eq 'Ready') { break }
		Start-Sleep -Milliseconds 250
	} while ((Get-Date) -lt $gameplayDeadline)
	if ($readiness -ne 'Ready') { throw "Native scenarios blocked: $readiness; no scenario keys sent" }
	Record 'Native gameplay with an empty menu stack observed; starting lifecycle scenarios'

	# Refusal before ownership, then diagnostic open, duplicate open refusal,
	# close, reopen (generation exercise), and close. All results are in OBVR.log.
	Focus-Game; [ObvrNativeMenuProbe]::Press(0x79, 0x44)
	Start-Sleep -Seconds $ScenarioWaitSec
	Record "scenario close-before-open sent"
	Focus-Game; [ObvrNativeMenuProbe]::Press(0x78, 0x43)
	Start-Sleep -Seconds $ScenarioWaitSec
	Record "scenario diagnostic-open sent"
	Focus-Game; [ObvrNativeMenuProbe]::Press(0x78, 0x43)
	Start-Sleep -Seconds $ScenarioWaitSec
	Record "scenario duplicate-open sent"
	Focus-Game; [ObvrNativeMenuProbe]::Press(0x79, 0x44)
	Start-Sleep -Seconds $ScenarioWaitSec
	Record "scenario diagnostic-close sent"
	Focus-Game; [ObvrNativeMenuProbe]::Press(0x78, 0x43)
	Start-Sleep -Seconds $ScenarioWaitSec
	Record "scenario diagnostic-reopen sent"
	Focus-Game; [ObvrNativeMenuProbe]::Press(0x79, 0x44)
	Start-Sleep -Seconds $ScenarioWaitSec
	Record "scenario diagnostic-reclose sent"

	# Ordinary Tab is deliberately attempted only after the probe-owned session
	# is closed. F9/F10 refusal while it is visible distinguishes a foreign
	# legacy session without sending a close request that could affect it.
	Focus-Game; [ObvrNativeMenuProbe]::Press(0x09, 0x0F)
	Start-Sleep -Seconds $ScenarioWaitSec
	Record "scenario legacy-tab sent"
	Focus-Game; [ObvrNativeMenuProbe]::Press(0x78, 0x43)
	Start-Sleep -Seconds $ScenarioWaitSec
	Focus-Game; [ObvrNativeMenuProbe]::Press(0x79, 0x44)
	Start-Sleep -Seconds $ScenarioWaitSec
	Record "scenario foreign-session F9/F10 refusal probes sent"
	Focus-Game; [ObvrNativeMenuProbe]::Press(0x1B, 0x01)
	Start-Sleep -Seconds 2

	if ($started.HasExited) { throw "Oblivion exited during the probe scenario" }
	Record "runtime scenario sequence completed"
} catch {
	Record "runtime failure: $($_.Exception.Message)"
	throw
} finally {
	# Preserve exit evidence even when a live-log read or readiness check throws.
	# This runs before cleanup so an already-exited owned process can still expose
	# its code/time; a running process remains subject to the ownership guard.
	if ($null -ne $started) { [void](Record-StartedExit) }
	if ($null -ne $started) { Close-OwnedProcess $started.Id $gamePath 'Oblivion' 15000 }
	Close-OwnedProcess $startedLoaderId $loaderPath 'obse_loader' 5000
	Close-OwnedProcess $startedLauncherId (Join-Path $gameDir 'OblivionLauncher.exe') 'OblivionLauncher' 5000
	Start-Sleep -Seconds 2
	# Evidence collection must never prevent restoration of installed files.
	foreach ($pair in @(@($installedLog, $evidenceLog), @($installedObseLog, $evidenceObseLog),
		@($installedSteamLog, $evidenceSteamLog), @($installedLoaderLog, $evidenceLoaderLog))) {
		try { Capture-FreshLog $pair[0] $pair[1] }
		catch { Record "EVIDENCE UNAVAILABLE: $($pair[0]): $($_.Exception.Message)" }
	}
	if ($logQuarantined) { [void](Restore-TrackedFile $backupLog $installedLog 'OBVR.log') }
	if ($restoreNeeded) {
		[void](Restore-TrackedFile $backupDll $installedDll 'dll')
		[void](Restore-TrackedFile $backupIni $installedIni 'ini')
		if ($restoreUnsafe) { Record "RESTORE UNSAFE: one or more staged files require manual verification" }
	}
	Record "run ended $(Get-Date -Format o)"
}

Write-Host "Runtime evidence: $ArtifactRoot"
