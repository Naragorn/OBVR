# Pure decisions used by native-menu-lifecycle-run.ps1 and its focused tests.

# The game's writer allows readers, but readers must also allow that writer.
# Get-FileHash's default sharing mode cannot read this live log on Windows.
function Read-SharedLogSnapshot([string]$Path) {
	$stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
		([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
	try {
		$buffer = [IO.MemoryStream]::new()
		try {
			$stream.CopyTo($buffer)
			$bytes = $buffer.ToArray()
			$hash = [Security.Cryptography.SHA256]::Create()
			try { $digest = [BitConverter]::ToString($hash.ComputeHash($bytes)).Replace('-', '') }
			finally { $hash.Dispose() }
			return @{ Hash = $digest; Text = [Text.Encoding]::UTF8.GetString($bytes);
				LastWriteTimeUtc = [IO.File]::GetLastWriteTimeUtc($Path) }
		} finally { $buffer.Dispose() }
	} finally { $stream.Dispose() }
}

function Get-LogEvidenceState {
	param(
		[hashtable]$Before,
		[bool]$Exists,
		[string]$CurrentHash,
		[datetime]$CurrentLastWriteTimeUtc
	)
	if (-not $Exists) { return 'Missing' }
	if ($null -eq $Before) { return 'Fresh' }
	if ($CurrentHash -ne [string]$Before.Hash -or
		$CurrentLastWriteTimeUtc -gt [datetime]$Before.LastWriteTimeUtc) {
		return 'Fresh'
	}
	return 'Stale'
}

function Get-ProbeStartupDecision {
	param(
		[bool]$PluginMarker,
		[bool]$UpdateMarker,
		[bool]$ProcessExited,
		[Nullable[int]]$ExitCode
	)
	if (-not $PluginMarker) {
		if ($ProcessExited) { return 'PluginInitExit' }
		return 'PluginInitTimeout'
	}
	if ($ProcessExited) { return 'FirstUpdateExit' }
	if (-not $UpdateMarker) { return 'FirstUpdateTimeout' }
	return 'Ready'
}

function Get-ExternalInputDecision([bool]$ProcessExited, [bool]$SignalPresent, [bool]$TimedOut) {
	if ($ProcessExited) { return 'Exited' }
	if ($SignalPresent) { return 'Finished' }
	if ($TimedOut) { return 'Timeout' }
	return 'Wait'
}

function Get-ProcessExitEvidence {
	param(
		[bool]$ProcessAvailable,
		[bool]$HasExited,
		[bool]$ExitCodeReadable
	)
	if (-not $ProcessAvailable) { return 'Unavailable' }
	if (-not $HasExited) { return 'Running' }
	if (-not $ExitCodeReadable) { return 'ExitedCodeUnavailable' }
	return 'ExitedCodeObserved'
}

function Get-DllSourceDecision {
	param(
		[ValidateSet('Built', 'Installed')]
		[string]$Source,
		[bool]$BuiltExists,
		[string]$InstalledHash,
		[string]$ExpectedInstalledHash
	)
	if ($Source -eq 'Built' -and -not $BuiltExists) { return 'BuiltMissing' }
	if ($Source -eq 'Installed' -and $InstalledHash -ne $ExpectedInstalledHash) {
		return 'InstalledHashMismatch'
	}
	return $Source
}

function Get-LogMarkerDecision {
	param(
		[bool]$Exists,
		[bool]$ChangedSinceBaseline,
		[bool]$ContainsMarker
	)
	if (-not $Exists) { return 'Missing' }
	if (-not $ChangedSinceBaseline) { return 'Stale' }
	if (-not $ContainsMarker) { return 'NotFound' }
	return 'Fresh'
}

function Get-BaselineReadinessDecision {
	param(
		[bool]$PluginLoadMarker,
		[bool]$CameraDisabledMarker,
		[bool]$WindowVisible,
		[bool]$ModuleLoaded,
		[bool]$AliveAfterInterval
	)
	if (-not $PluginLoadMarker) { return 'PluginInitNotFresh' }
	if (-not $CameraDisabledMarker) { return 'CameraConfigNotFresh' }
	if (-not $AliveAfterInterval) { return 'ExitedBeforeStableInterval' }
	if (-not $WindowVisible) { return 'GameWindowNotReady' }
	if (-not $ModuleLoaded) { return 'ObvrModuleNotObserved' }
	return 'Ready'
}

function Get-LoadSequenceDecision([bool]$Enabled, [bool]$ProcessExited) {
	if (-not $Enabled) { return 'Skipped' }
	if ($ProcessExited) { return 'Exited' }
	# A surviving process proves only that the process survived the input
	# sequence. It does not prove that Continue was activated or that a save
	# or world load began.
	return 'ProcessAliveLoadUnverified'
}

function Get-NoObvrControlDecision([bool]$ProcessExited, [bool]$WindowVisible) {
	if ($ProcessExited) { return 'Exited' }
	if (-not $WindowVisible) { return 'WindowUnverified' }
	return 'Ready'
}

# Only the latest native snapshot describes the current menu state. A previous
# gameplay snapshot must not authorize inputs after a return to the title menu.
function Get-NativeScenarioReadiness([bool]$ProcessExited, [string]$LogText) {
	if ($ProcessExited) { return 'Exited' }
	$snapshots = [regex]::Matches($LogText,
		'Native menu probe: [^\r\n]*available=(\d+) focused=(\d+) gameplay=(\d+) [^\r\n]*top=([0-9A-Fa-f]+)')
	if ($snapshots.Count -eq 0) { return 'SnapshotMissing' }
	$latest = $snapshots[$snapshots.Count - 1]
	if ($latest.Groups[1].Value -ne '1') { return 'Unavailable' }
	if ($latest.Groups[2].Value -ne '1') { return 'Unfocused' }
	if ($latest.Groups[3].Value -ne '1') { return 'GameplayNotObserved' }
	if ([Convert]::ToUInt32($latest.Groups[4].Value, 16) -ne 0) { return 'MenuStillOpen' }
	return 'Ready'
}

function Get-OwnedProcessCleanupAction {
	param(
		[bool]$Owned,
		[bool]$PathMatches,
		[bool]$AlreadyExited,
		[bool]$GracefulCloseSucceeded
	)
	if (-not $Owned -or -not $PathMatches -or $AlreadyExited) { return 'Skip' }
	if ($GracefulCloseSucceeded) { return 'Closed' }
	return 'Force'
}

function Get-RestorationDecision {
	param(
		[bool]$Needed,
		[bool]$CopySucceeded,
		[bool]$HashMatches
	)
	if (-not $Needed) { return 'NotNeeded' }
	if (-not $CopySucceeded -or -not $HashMatches) { return 'Unsafe' }
	return 'Restored'
}
