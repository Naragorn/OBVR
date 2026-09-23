$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
. (Join-Path $root 'tools/native-menu-lifecycle-logic.ps1')

$passed = 0
$failed = 0
function Check([string]$name, [string]$actual, [string]$expected) {
	if ($actual -eq $expected) {
		$script:passed++
		Write-Output "ok $name"
	} else {
		$script:failed++
		Write-Output "FAIL $name expected=$expected actual=$actual"
	}
}

$stamp = [datetime]'2026-09-21T16:00:00Z'
foreach ($exited in @($false, $true)) {
	foreach ($signal in @($false, $true)) {
		foreach ($expired in @($false, $true)) {
			$expected = if ($exited) { 'Exited' } elseif ($signal) { 'Finished' } elseif ($expired) { 'Timeout' } else { 'Wait' }
			Check "external driver exit=$exited signal=$signal timeout=$expired" (Get-ExternalInputDecision $exited $signal $expired) $expected
		}
	}
}
$baseline = @{ Hash = 'same'; LastWriteTimeUtc = $stamp }
Check 'missing log is marked' (Get-LogEvidenceState $baseline $false '' $stamp) 'Missing'
Check 'new log is fresh' (Get-LogEvidenceState $null $true 'new' $stamp) 'Fresh'
Check 'replaced log is fresh' (Get-LogEvidenceState $baseline $true 'new' $stamp) 'Fresh'
Check 'rewritten same log is fresh' (Get-LogEvidenceState $baseline $true 'same' $stamp.AddSeconds(1)) 'Fresh'
Check 'unchanged log is stale' (Get-LogEvidenceState $baseline $true 'same' $stamp) 'Stale'
Check 'missing marker source is missing' (Get-LogMarkerDecision $false $false $false) 'Missing'
Check 'unchanged marker source is stale' (Get-LogMarkerDecision $true $false $true) 'Stale'
Check 'changed source without marker is not found' (Get-LogMarkerDecision $true $true $false) 'NotFound'
Check 'changed source with marker is fresh' (Get-LogMarkerDecision $true $true $true) 'Fresh'

Check 'baseline needs fresh plugin load' (Get-BaselineReadinessDecision $false $false $false $false $false) 'PluginInitNotFresh'
Check 'baseline needs fresh camera config' (Get-BaselineReadinessDecision $true $false $false $false $true) 'CameraConfigNotFresh'
Check 'baseline detects early exit' (Get-BaselineReadinessDecision $true $true $false $false $false) 'ExitedBeforeStableInterval'
Check 'baseline needs game window' (Get-BaselineReadinessDecision $true $true $false $false $true) 'GameWindowNotReady'
Check 'baseline needs OBVR module' (Get-BaselineReadinessDecision $true $true $true $false $true) 'ObvrModuleNotObserved'
Check 'baseline needs alive interval' (Get-BaselineReadinessDecision $true $true $true $true $false) 'ExitedBeforeStableInterval'
Check 'baseline is ready' (Get-BaselineReadinessDecision $true $true $true $true $true) 'Ready'

Check 'legacy load sequence is skipped while alive' (Get-LoadSequenceDecision $false $false) 'Skipped'
Check 'legacy load sequence is skipped after exit' (Get-LoadSequenceDecision $false $true) 'Skipped'
Check 'legacy load sequence detects exit' (Get-LoadSequenceDecision $true $true) 'Exited'
Check 'legacy load sequence keeps load unverified while alive' (Get-LoadSequenceDecision $true $false) 'ProcessAliveLoadUnverified'

Check 'no-OBVR control detects early exit' (Get-NoObvrControlDecision $true $false) 'Exited'
Check 'no-OBVR control keeps missing window unverified' (Get-NoObvrControlDecision $false $false) 'WindowUnverified'
Check 'no-OBVR control accepts visible window' (Get-NoObvrControlDecision $false $true) 'Ready'

$readySnapshot = 'Native menu probe: remove token=0 phase=0 available=1 focused=1 gameplay=1 root=12345678 top=000'
Check 'scenario refuses exited game even with ready snapshot' (Get-NativeScenarioReadiness $true $readySnapshot) 'Exited'
Check 'scenario requires native evidence' (Get-NativeScenarioReadiness $false '') 'SnapshotMissing'
Check 'scenario refuses unavailable manager' (Get-NativeScenarioReadiness $false ($readySnapshot.Replace('available=1','available=0'))) 'Unavailable'
Check 'scenario requires focus' (Get-NativeScenarioReadiness $false ($readySnapshot.Replace('focused=1','focused=0'))) 'Unfocused'
Check 'scenario requires gameplay' (Get-NativeScenarioReadiness $false ($readySnapshot.Replace('gameplay=1','gameplay=0'))) 'GameplayNotObserved'
Check 'scenario refuses open menu' (Get-NativeScenarioReadiness $false ($readySnapshot.Replace('top=000','top=414'))) 'MenuStillOpen'
Check 'scenario accepts native gameplay without menus' (Get-NativeScenarioReadiness $false $readySnapshot) 'Ready'
Check 'scenario ignores unrelated later log lines' (Get-NativeScenarioReadiness $false ($readySnapshot + "`nother diagnostic")) 'Ready'
Check 'scenario rejects stale ready evidence after title menu' (Get-NativeScenarioReadiness $false ($readySnapshot + "`n" + $readySnapshot.Replace('gameplay=1','gameplay=0'))) 'GameplayNotObserved'
Check 'scenario permits transition from title to gameplay' (Get-NativeScenarioReadiness $false ($readySnapshot.Replace('gameplay=1','gameplay=0') + "`n" + $readySnapshot)) 'Ready'

Check 'plugin marker timeout' (Get-ProbeStartupDecision $false $false $false $null) 'PluginInitTimeout'
Check 'plugin marker process exit' (Get-ProbeStartupDecision $false $false $true 0) 'PluginInitExit'
Check 'first Update process exit' (Get-ProbeStartupDecision $true $false $true 0) 'FirstUpdateExit'
Check 'first Update timeout' (Get-ProbeStartupDecision $true $false $false $null) 'FirstUpdateTimeout'
Check 'first Update ready' (Get-ProbeStartupDecision $true $true $false $null) 'Ready'

Check 'exit evidence unavailable process' (Get-ProcessExitEvidence $false $true $true) 'Unavailable'
Check 'exit evidence process still running' (Get-ProcessExitEvidence $true $false $false) 'Running'
Check 'exit evidence code unavailable' (Get-ProcessExitEvidence $true $true $false) 'ExitedCodeUnavailable'
Check 'exit evidence code observed' (Get-ProcessExitEvidence $true $true $true) 'ExitedCodeObserved'

$originalHash = 'original'
Check 'built DLL source is accepted' (Get-DllSourceDecision 'Built' $true 'ignored' $originalHash) 'Built'
Check 'missing built DLL is refused' (Get-DllSourceDecision 'Built' $false 'ignored' $originalHash) 'BuiltMissing'
Check 'matching installed DLL source is accepted' (Get-DllSourceDecision 'Installed' $false $originalHash $originalHash) 'Installed'
Check 'mismatched installed DLL source is refused' (Get-DllSourceDecision 'Installed' $false 'wrong' $originalHash) 'InstalledHashMismatch'

Check 'unowned process is skipped' (Get-OwnedProcessCleanupAction $false $true $false $false) 'Skip'
Check 'foreign path is skipped' (Get-OwnedProcessCleanupAction $true $false $false $false) 'Skip'
Check 'already exited process is skipped' (Get-OwnedProcessCleanupAction $true $true $true $false) 'Skip'
Check 'owned graceful close' (Get-OwnedProcessCleanupAction $true $true $false $true) 'Closed'
Check 'owned close timeout forces stop' (Get-OwnedProcessCleanupAction $true $true $false $false) 'Force'

Check 'restore not needed' (Get-RestorationDecision $false $false $false) 'NotNeeded'
Check 'restore verified' (Get-RestorationDecision $true $true $true) 'Restored'
Check 'restore copy failure unsafe' (Get-RestorationDecision $true $false $false) 'Unsafe'
Check 'restore hash mismatch unsafe' (Get-RestorationDecision $true $true $false) 'Unsafe'

# Execute the real logger without running the harness or launching the game.
$runnerAst = [System.Management.Automation.Language.Parser]::ParseFile(
	(Join-Path $root 'tools/native-menu-lifecycle-run.ps1'), [ref]$null, [ref]$null)
$recordAst = $runnerAst.Find({ param($node)
	$node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Record'
}, $true)
Invoke-Expression $recordAst.Extent.Text
foreach ($functionName in @('Record-StartedExit', 'Wait-Log')) {
	$functionAst = $runnerAst.Find({ param($node)
		$node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $functionName
	}, $true)
	Invoke-Expression $functionAst.Extent.Text
}
$runLog = Join-Path ([IO.Path]::GetTempPath()) ('obvr-harness-log-' + [guid]::NewGuid() + '.txt')
try {
	$result = @(& { Record 'sharing violation retry'; return $true })
	Check 'real logger does not pollute Boolean result' $result.Count '1'
	Check 'real logger preserves Boolean type' ($result[0] -is [bool]) 'True'
	Check 'real logger persists diagnostic' ([IO.File]::ReadAllText($runLog).Trim()) 'sharing violation retry'
	$result = @(& { Record 'second retry'; return $false })
	Check 'real logger preserves false result' ($result.Count -eq 1 -and $result[0] -eq $false) 'True'
	Check 'real logger appends diagnostics' ([IO.File]::ReadAllLines($runLog).Count) '2'
	$started = $null
	$launchProcess = $null
	$script:startedExitRecorded = $false
	Check 'exit recorder handles no owned process' (Record-StartedExit) 'False'
	$started = [pscustomobject]@{ Id = 42; HasExited = $false; ExitCode = -1073741819; ExitTime = [datetime]'2026-09-22T13:13:25Z' }
	$started | Add-Member -MemberType ScriptMethod -Name Refresh -Value {}
	Check 'exit recorder keeps running process active' (Record-StartedExit) 'False'
	$started.HasExited = $true
	Check 'exit recorder observes first exit' (Record-StartedExit) 'True'
	$linesAfterExit = [IO.File]::ReadAllLines($runLog).Count
	Check 'exit recorder retains exit on repeated observation' (Record-StartedExit) 'True'
	Check 'exit recorder writes exit only once' ([IO.File]::ReadAllLines($runLog).Count) $linesAfterExit
	# A cached exit must end a real marker wait even when an old matching log exists.
	$installedLog = $runLog
	# The production reader consumes the game's UTF-8 log. Windows PowerShell
	# Tee-Object writes UTF-16, so normalize this fixture before marker tests.
	[IO.File]::WriteAllText($installedLog, [IO.File]::ReadAllText($installedLog), [Text.UTF8Encoding]::new($false))
	$logBaseline = @{}
	Check 'real marker wait refuses marker after process exit' (Wait-Log 'sharing violation retry' 60) 'False'
	$script:startedExitRecorded = $false
	$started.HasExited = $false
	Check 'real marker wait accepts fresh marker in running process' (Wait-Log 'sharing violation retry' 60) 'True'
	Check 'real marker wait respects expired deadline' (Wait-Log 'missing' 0) 'False'
	$script:exitRefreshCount = 0
	$started | Add-Member -MemberType ScriptMethod -Name Refresh -Value {
		$script:exitRefreshCount++
		$this.HasExited = $script:exitRefreshCount -ge 2
	} -Force
	Check 'real marker wait rejects exit while reading fresh marker' (Wait-Log 'sharing violation retry' 60) 'False'
	$script:startedExitRecorded = $false
	$started | Add-Member -MemberType ScriptMethod -Name Refresh -Value { throw 'process observation unavailable' } -Force
	Check 'exit recorder does not fabricate exit when observation fails' (Record-StartedExit) 'False'
} finally {
	if (Test-Path -LiteralPath $runLog) { Remove-Item -LiteralPath $runLog }
}

$liveLog = Join-Path ([IO.Path]::GetTempPath()) ('obvr-live-log-' + [guid]::NewGuid() + '.txt')
try {
	$writer = [IO.File]::Open($liveLog, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::Read)
	try {
		$empty = Read-SharedLogSnapshot $liveLog
		Check 'shared snapshot handles empty live log' $empty.Text ''
		$bytes = [Text.Encoding]::UTF8.GetBytes('fresh marker')
		$writer.Write($bytes, 0, $bytes.Length)
		$writer.Flush()
		$snapshot = Read-SharedLogSnapshot $liveLog
		Check 'shared snapshot reads active game writer' $snapshot.Text 'fresh marker'
		Check 'shared snapshot sees changed content' ($snapshot.Hash -ne $empty.Hash) 'True'
		$bytes = [Text.Encoding]::UTF8.GetBytes(' and update')
		$writer.Write($bytes, 0, $bytes.Length)
		$writer.Flush()
		Check 'shared snapshot reads subsequent append' (Read-SharedLogSnapshot $liveLog).Text 'fresh marker and update'
	} finally { $writer.Dispose() }
	Check 'shared snapshot hash matches closed file hash' (Read-SharedLogSnapshot $liveLog).Hash (Get-FileHash $liveLog).Hash
	Remove-Item -LiteralPath $liveLog
	$missingFailed = $false
	try { $null = Read-SharedLogSnapshot $liveLog } catch { $missingFailed = $true }
	Check 'missing shared log reports failure' $missingFailed 'True'
} finally {
	if (Test-Path -LiteralPath $liveLog) { Remove-Item -LiteralPath $liveLog }
}

Write-Output "Native menu lifecycle harness logic: $passed/$($passed + $failed) checks passed"
if ($failed -ne 0) { exit 1 }
