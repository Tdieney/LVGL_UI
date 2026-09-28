[CmdletBinding()]
param(
    [string]$BuildDir = '',
    [string]$Compiler = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$verificationPassed = $false
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot 'sim_pc\build'
} elseif (-not [IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repoRoot $BuildDir
}

$script:SimulateCimDenial = $false

function Get-VerifiedProcessTree {
    param(
        [Parameter(Mandatory=$true)][System.Diagnostics.Process]$RootProcess,
        [Parameter(Mandatory=$true)][datetime]$RootStartTime
    )
    $verified = [System.Collections.Generic.List[hashtable]]::new()
    try {
        if ($RootProcess.HasExited) { return $verified }
        if ($RootProcess.StartTime -ne $RootStartTime) { return $verified }
    } catch {
        return $verified
    }

    $visited = [System.Collections.Generic.HashSet[int]]::new()
    [void]$visited.Add($RootProcess.Id)

    $queue = [System.Collections.Generic.Queue[hashtable]]::new()
    $queue.Enqueue(@{ Process = $RootProcess; Pid = $RootProcess.Id; StartTime = $RootStartTime })

    while ($queue.Count -gt 0) {
        # Revalidate root process liveness during traversal
        try {
            if ($RootProcess.HasExited -or $RootProcess.StartTime -ne $RootStartTime) {
                # Root process died or changed identity during traversal
                return $verified
            }
        } catch {
            return $verified
        }

        $parent = $queue.Dequeue()
        $parentId = [int]$parent.Pid
        $parentStart = [datetime]$parent.StartTime

        try {
            if ($script:SimulateCimDenial) {
                throw [System.UnauthorizedAccessException]::new("Simulated CIM access denial (controlled fault injection)")
            }
            $children = Get-CimInstance Win32_Process -Filter "ParentProcessId = $parentId" -ErrorAction Stop
        } catch {
            # Fail closed with explicit error: do not silently swallow CIM enumeration errors
            throw "Get-VerifiedProcessTree failed to enumerate child processes for parent PID $parentId : $_"
        }

        if ($children) {
            foreach ($c in $children) {
                $cPid = [int]$c.ProcessId
                if ($cPid -le 0 -or $visited.Contains($cPid)) { continue }
                try {
                    $childProc = [System.Diagnostics.Process]::GetProcessById($cPid)
                    if ($childProc.HasExited) { continue }
                    $childStart = $childProc.StartTime
                    # Descendant must have started at or after parent (2s margin for clock resolution)
                    if ($childStart -ge $parentStart.AddSeconds(-2)) {
                        [void]$visited.Add($cPid)
                        $node = @{
                            Process = $childProc
                            Pid = $cPid
                            StartTime = $childStart
                            ParentPid = $parentId
                        }
                        $verified.Add($node)
                        $queue.Enqueue($node)
                    }
                } catch {}
            }
        }
    }
    return $verified
}

function Terminate-OwnedProcessTree {
    param(
        [Parameter(Mandatory=$true)][int]$RootPid,
        [hashtable[]]$KnownDescendants = @(),
        [System.Diagnostics.Process]$RootProcess = $null,
        [datetime]$ExpectedStartTime = [datetime]::MinValue,
        [int]$WaitTimeoutMs = 5000,
        [switch]$SkipDiscovery = $false
    )
    # 1. Verify root process identity before inspecting or touching descendants
    # ExpectedStartTime is strictly mandatory to prevent unsafe raw PID termination
    if ($ExpectedStartTime -eq [datetime]::MinValue) {
        Write-Warning "Root PID $RootPid termination rejected: ExpectedStartTime is mandatory for verified task cleanup"
        return @()
    }

    $rootAlive = $false
    $rootStartTime = $ExpectedStartTime
    if ($null -ne $RootProcess) {
        try {
            if (-not $RootProcess.HasExited) {
                $rootStartTime = $RootProcess.StartTime
                if ($rootStartTime -eq $ExpectedStartTime) {
                    $rootAlive = $true
                } else {
                    Write-Warning "Root PID $RootPid StartTime mismatch ($rootStartTime vs expected $ExpectedStartTime): REJECTED as recycled PID"
                    return @()
                }
            }
        } catch {
            return @()
        }
    } elseif ($RootPid -gt 0) {
        try {
            $rp = [System.Diagnostics.Process]::GetProcessById($RootPid)
            if (-not $rp.HasExited) {
                $st = $rp.StartTime
                if ($st -eq $ExpectedStartTime) {
                    $RootProcess = $rp
                    $rootStartTime = $st
                    $rootAlive = $true
                } else {
                    Write-Warning "Root PID $RootPid StartTime mismatch ($st vs expected $ExpectedStartTime): REJECTED as recycled PID"
                    return @()
                }
            }
        } catch {
            return @()
        }
    }

    # If root is not alive and no valid known descendants were provided, clean no-op
    if (-not $rootAlive -and ($null -eq $KnownDescendants -or $KnownDescendants.Length -eq 0)) {
        return @()
    }

    # 2. Gather verified targets
    $toKill = [System.Collections.Generic.List[hashtable]]::new()
    if ($null -ne $KnownDescendants) {
        foreach ($kd in $KnownDescendants) {
            if ($null -ne $kd -and $kd.ContainsKey('Pid') -and $kd.ContainsKey('StartTime')) {
                $toKill.Add($kd)
            }
        }
    }

    if ($rootAlive -and $null -ne $RootProcess) {
        # Always register the verified root process first so it is guaranteed to be terminated
        # even if dynamic child discovery fails or is skipped.
        $already = $false
        foreach ($tk in $toKill) {
            if ($tk.Pid -eq $RootPid) { $already = $true; break }
        }
        if (-not $already) {
            $toKill.Add(@{
                Process = $RootProcess
                Pid = $RootPid
                StartTime = $rootStartTime
            })
        }

        # Dynamically discover descendants only if discovery is not explicitly skipped
        if (-not $SkipDiscovery) {
            try {
                $liveDesc = Get-VerifiedProcessTree -RootProcess $RootProcess -RootStartTime $rootStartTime
                foreach ($ld in $liveDesc) {
                    $alreadyDesc = $false
                    foreach ($tk in $toKill) {
                        if ($tk.Pid -eq $ld.Pid) { $alreadyDesc = $true; break }
                    }
                    if (-not $alreadyDesc) { $toKill.Insert(0, $ld) }
                }
            } catch {
                Write-Warning "Descendant discovery during cleanup failed closed: $_. Proceeding with known verified processes."
            }
        }
    }

    # 3. Terminate descendants bottom-up, strictly re-verifying StartTime before Kill
    $terminatedPids = [System.Collections.Generic.List[int]]::new()
    for ($i = $toKill.Count - 1; $i -ge 0; $i--) {
        $target = $toKill[$i]
        $tPid = [int]$target.Pid
        $tExpectedStart = [datetime]$target.StartTime
        $proc = $target.Process

        try {
            if ($null -eq $proc -or $proc.HasExited) {
                $proc = [System.Diagnostics.Process]::GetProcessById($tPid)
            }
            if (-not $proc.HasExited) {
                if ($tExpectedStart -ne [datetime]::MinValue -and $proc.StartTime -ne $tExpectedStart) {
                    Write-Warning "Refusing to kill PID $($tPid): StartTime mismatch. Recycled PID protected!"
                    continue
                }
                $proc.Kill()
                $terminatedPids.Add($tPid)
            }
        } catch [System.ArgumentException] {
            # Already exited
        } catch {
            Write-Warning "Failed to kill PID $($tPid): $_"
        }
    }

    # 4. Wait for termination
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $stillAlive = [System.Collections.Generic.List[int]]::new()
    while ($sw.ElapsedMilliseconds -lt $WaitTimeoutMs) {
        $stillAlive.Clear()
        foreach ($aliveCheckPid in $terminatedPids) {
            try {
                $chk = [System.Diagnostics.Process]::GetProcessById($aliveCheckPid)
                if (-not $chk.HasExited) {
                    $stillAlive.Add($aliveCheckPid)
                }
            } catch {}
        }
        if ($stillAlive.Count -eq 0) { break }
        Start-Sleep -Milliseconds 100
    }

    if ($stillAlive.Count -gt 0) {
        throw "Failed to terminate watchdog process tree (PIDs still running: $($stillAlive -join ', '))"
    }

    return [int[]]$terminatedPids
}

function Invoke-ProcessWithTimeout {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [string[]]$ArgumentList = @(),
        [string]$RawArguments = '',
        [string]$WorkingDirectory = '',
        [int]$TimeoutSeconds = 60,
        [string]$PhaseName = 'Process'
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    if ($RawArguments) {
        $psi.Arguments = $RawArguments
    } else {
        $escapedArgs = $ArgumentList | ForEach-Object {
            if ($_ -match '[\s"]') {
                "`"" + ($_ -replace '"', '\"') + "`""
            } else {
                $_
            }
        }
        $psi.Arguments = ($escapedArgs -join ' ')
    }
    if ($WorkingDirectory) { $psi.WorkingDirectory = $WorkingDirectory }
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $proc = [System.Diagnostics.Process]::Start($psi)
    if ($null -eq $proc) {
        throw "Failed to start process: $FilePath ($PhaseName)"
    }

    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()

    $rootStartTime = [datetime]::MinValue
    try { if (-not $proc.HasExited) { $rootStartTime = $proc.StartTime } } catch {}

    # Track child and descendant processes with verified identity while alive
    $trackedDescendants = [System.Collections.Generic.List[hashtable]]::new()
    $trackedPids = [System.Collections.Generic.HashSet[int]]::new()
    [void]$trackedPids.Add($proc.Id)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $timeoutMs = $TimeoutSeconds * 1000

    $completed = $false
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        if ($proc.WaitForExit(100)) {
            $completed = $true
            break
        }
        try {
            if (-not $proc.HasExited) {
                $children = Get-VerifiedProcessTree -RootProcess $proc -RootStartTime $rootStartTime
                foreach ($c in $children) {
                    $cPid = [int]$c.Pid
                    if ($trackedPids.Add($cPid)) {
                        $trackedDescendants.Add($c)
                    }
                }
            }
        } catch {
            # Fail closed on enumeration errors: do not silently swallow CIM errors or lose descendant visibility
            $enumErr = $_
            $termList = Terminate-OwnedProcessTree -RootPid $proc.Id -KnownDescendants ([hashtable[]]$trackedDescendants.ToArray()) -RootProcess $proc -ExpectedStartTime $rootStartTime -SkipDiscovery
            try { $proc.Dispose() } catch {}
            throw "Watchdog process tree enumeration failed closed during $($PhaseName): $enumErr"
        }
    }

    if (-not $completed) {
        $childProcId = $proc.Id
        $termList = Terminate-OwnedProcessTree -RootPid $childProcId -KnownDescendants ([hashtable[]]$trackedDescendants.ToArray()) -RootProcess $proc -ExpectedStartTime $rootStartTime
        try { $proc.Dispose() } catch {}
        $pidsStr = ($termList -join ',')
        throw "Watchdog timeout ($TimeoutSeconds s) exceeded during $PhaseName (PIDs $pidsStr terminated)"
    }

    $stdoutDone = $stdoutTask.Wait(2000)
    $stderrDone = $stderrTask.Wait(2000)
    return [PSCustomObject]@{
        ExitCode = $proc.ExitCode
        Stdout   = if ($stdoutDone) { $stdoutTask.Result } else { "" }
        Stderr   = if ($stderrDone) { $stderrTask.Result } else { "" }
    }
}

function Invoke-ConcurrentWorkers {
    param(
        [Parameter(Mandatory=$true)][System.Diagnostics.ProcessStartInfo[]]$StartInfos,
        [int]$TimeoutSeconds = 60,
        [string]$PhaseName = 'Concurrent Workers'
    )
    $workers = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
    $workersStartTime = [System.Collections.Generic.List[datetime]]::new()
    $stdoutTasks = [System.Collections.Generic.List[System.Threading.Tasks.Task[string]]]::new()
    $stderrTasks = [System.Collections.Generic.List[System.Threading.Tasks.Task[string]]]::new()
    $trackedPidsPerWorker = [System.Collections.Generic.List[System.Collections.Generic.HashSet[int]]]::new()
    $trackedDescendantsPerWorker = [System.Collections.Generic.List[System.Collections.Generic.List[hashtable]]]::new()

    try {
        foreach ($psi in $StartInfos) {
            $psi.UseShellExecute = $false
            $psi.CreateNoWindow = $true
            $psi.RedirectStandardOutput = $true
            $psi.RedirectStandardError = $true
            $p = [System.Diagnostics.Process]::Start($psi)
            if ($null -eq $p) { throw "Failed to start worker in $PhaseName" }
            $workers.Add($p)
            $pStartTime = [datetime]::MinValue
            try { if (-not $p.HasExited) { $pStartTime = $p.StartTime } } catch {}
            $workersStartTime.Add($pStartTime)
            $stdoutTasks.Add($p.StandardOutput.ReadToEndAsync())
            $stderrTasks.Add($p.StandardError.ReadToEndAsync())
            $set = [System.Collections.Generic.HashSet[int]]::new()
            [void]$set.Add($p.Id)
            $trackedPidsPerWorker.Add($set)
            $descList = [System.Collections.Generic.List[hashtable]]::new()
            $trackedDescendantsPerWorker.Add($descList)
        }
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $timeoutMs = $TimeoutSeconds * 1000

        # Wait loop tracking child and descendant processes with verified StartTime
        while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
            $allDone = $true
            for ($i = 0; $i -lt $workers.Count; $i++) {
                if (-not $workers[$i].HasExited) {
                    $allDone = $false
                    try {
                        $children = Get-VerifiedProcessTree -RootProcess $workers[$i] -RootStartTime $workersStartTime[$i]
                        foreach ($c in $children) {
                            $cPid = [int]$c.Pid
                            if ($trackedPidsPerWorker[$i].Add($cPid)) {
                                $trackedDescendantsPerWorker[$i].Add($c)
                            }
                        }
                    } catch {
                        $enumErr = $_
                        for ($k = 0; $k -lt $workers.Count; $k++) {
                            try {
                                if (-not $workers[$k].HasExited) {
                                    [void](Terminate-OwnedProcessTree -RootPid $workers[$k].Id -KnownDescendants ([hashtable[]]$trackedDescendantsPerWorker[$k].ToArray()) -RootProcess $workers[$k] -ExpectedStartTime $workersStartTime[$k] -SkipDiscovery)
                                }
                            } catch {}
                        }
                        throw "Watchdog concurrent process tree enumeration failed closed during $($PhaseName): $enumErr"
                    }
                }
            }
            if ($allDone) { break }
            Start-Sleep -Milliseconds 100
        }

        # Check if any worker timed out
        $timedOutWorkerIdx = -1
        for ($i = 0; $i -lt $workers.Count; $i++) {
            if (-not $workers[$i].HasExited) {
                $timedOutWorkerIdx = $i
                break
            }
        }

        if ($timedOutWorkerIdx -ge 0) {
            # Collect all tracked PIDs across all workers
            $allTracked = [System.Collections.Generic.HashSet[int]]::new()
            for ($i = 0; $i -lt $workers.Count; $i++) {
                foreach ($tp in $trackedPidsPerWorker[$i]) { [void]$allTracked.Add($tp) }
            }
            $pidsStr = ($allTracked -join ',')
            throw "Watchdog timeout ($TimeoutSeconds s) exceeded in $PhaseName (worker $timedOutWorkerIdx, PID $($workers[$timedOutWorkerIdx].Id), all PIDs $pidsStr)"
        }

        $results = @()
        for ($i = 0; $i -lt $workers.Count; $i++) {
            $stdoutDone = $stdoutTasks[$i].Wait(2000)
            $stderrDone = $stderrTasks[$i].Wait(2000)
            $results += [PSCustomObject]@{
                ExitCode = $workers[$i].ExitCode
                Stdout   = if ($stdoutDone) { $stdoutTasks[$i].Result } else { "" }
                Stderr   = if ($stderrDone) { $stderrTasks[$i].Result } else { "" }
                Pid      = $workers[$i].Id
            }
        }
        return $results
    } finally {
        # Only terminate workers that have not exited yet (skip clean normal completions)
        $allTerminated = [System.Collections.Generic.HashSet[int]]::new()
        for ($i = 0; $i -lt $workers.Count; $i++) {
            $p = $workers[$i]
            try {
                if (-not $p.HasExited) {
                    $knownDesc = if ($i -lt $trackedDescendantsPerWorker.Count) { [hashtable[]]$trackedDescendantsPerWorker[$i].ToArray() } else { @() }
                    $term = Terminate-OwnedProcessTree -RootPid $p.Id -KnownDescendants $knownDesc -RootProcess $p -ExpectedStartTime $workersStartTime[$i] -WaitTimeoutMs 5000
                    foreach ($t in $term) { [void]$allTerminated.Add($t) }
                }
            } catch {
                Write-Warning "Failed to cleanly reap worker process tree for PID $($p.Id): $_"
                throw
            }
            try { $p.Dispose() } catch {}
        }
    }
}

function Assert-ZipEntriesSha256 {
    param(
        [Parameter(Mandatory=$true)][string]$ZipPath,
        [Parameter(Mandatory=$true)][string]$SourceDir,
        [Parameter(Mandatory=$true)][string[]]$RequiredFiles,
        [Parameter(Mandatory=$true)][string[]]$RequiredDocs
    )
    Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
    $zipArchive = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $expectedEntries = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
        foreach ($rf in $RequiredFiles) { [void]$expectedEntries.Add($rf.Replace('\', '/')) }
        foreach ($df in $RequiredDocs) { [void]$expectedEntries.Add("docs/$df".Replace('\', '/')) }

        $seenEntries = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
        foreach ($entry in $zipArchive.Entries) {
            $fullName = $entry.FullName.Replace('\', '/')
            if ($fullName.EndsWith('/')) { continue }
            if ($seenEntries.Contains($fullName)) {
                throw "Archive $ZipPath contains duplicate entry: $fullName"
            }
            [void]$seenEntries.Add($fullName)
            if (-not $expectedEntries.Contains($fullName)) {
                throw "Archive $ZipPath contains unexpected entry: $fullName"
            }
        }
        if ($seenEntries.Count -ne $expectedEntries.Count) {
            throw "Archive $ZipPath entry count mismatch: found $($seenEntries.Count), expected $($expectedEntries.Count)"
        }

        foreach ($rf in $RequiredFiles) {
            $entry = $zipArchive.Entries | Where-Object { $_.FullName.Replace('\', '/') -eq $rf } | Select-Object -First 1
            if ($null -eq $entry) {
                throw "Archive $ZipPath missing required production file $rf"
            }
            $srcPath = Join-Path $SourceDir $rf
            $stream = $entry.Open()
            $zipHashBytes = try { $sha256.ComputeHash($stream) } finally { $stream.Dispose() }
            $zipHash = [BitConverter]::ToString($zipHashBytes).Replace('-', '').ToUpperInvariant()

            $srcBytes = [IO.File]::ReadAllBytes($srcPath)
            $srcHashBytes = $sha256.ComputeHash($srcBytes)
            $srcHash = [BitConverter]::ToString($srcHashBytes).Replace('-', '').ToUpperInvariant()

            if ($zipHash -ne $srcHash) {
                throw "SHA256 mismatch for entry $rf in $ZipPath (ZIP: $zipHash, Source: $srcHash)"
            }
        }
        foreach ($df in $RequiredDocs) {
            $docEntry = "docs/$df"
            $entry = $zipArchive.Entries | Where-Object { $_.FullName.Replace('\', '/') -eq $docEntry } | Select-Object -First 1
            if ($null -eq $entry) {
                throw "Archive $ZipPath missing required doc $docEntry"
            }
            $srcPath = Join-Path "$SourceDir\docs" $df
            $stream = $entry.Open()
            $zipHashBytes = try { $sha256.ComputeHash($stream) } finally { $stream.Dispose() }
            $zipHash = [BitConverter]::ToString($zipHashBytes).Replace('-', '').ToUpperInvariant()

            $srcBytes = [IO.File]::ReadAllBytes($srcPath)
            $srcHashBytes = $sha256.ComputeHash($srcBytes)
            $srcHash = [BitConverter]::ToString($srcHashBytes).Replace('-', '').ToUpperInvariant()

            if ($zipHash -ne $srcHash) {
                throw "SHA256 mismatch for entry $docEntry in $ZipPath (ZIP: $zipHash, Source: $srcHash)"
            }
        }
    } finally {
        $sha256.Dispose()
        $zipArchive.Dispose()
    }
}

function Invoke-ProcessSafetyFixtures {
    Write-Host "`nTesting process ownership safety fixtures..."

    # 1. Normal worker completion clean exit (no kill attempt on exited process)
    $normalProc = [System.Diagnostics.Process]::Start((New-Object System.Diagnostics.ProcessStartInfo -Property @{
        FileName = 'cmd.exe'
        Arguments = '/c exit 0'
        UseShellExecute = $false
        CreateNoWindow = $true
    }))
    [void]$normalProc.WaitForExit()
    $normalRes = Terminate-OwnedProcessTree -RootPid $normalProc.Id -RootProcess $normalProc -ExpectedStartTime $normalProc.StartTime
    if (@($normalRes).Count -ne 0) {
        throw "Process safety fixture failed: attempted termination on already exited worker!"
    }

    # 2. Timeout watchdog termination (verifies parent + descendant termination)
    $timeoutProc = [System.Diagnostics.Process]::Start((New-Object System.Diagnostics.ProcessStartInfo -Property @{
        FileName = 'cmd.exe'
        Arguments = '/c powershell -NoProfile -Command "Start-Sleep -Seconds 20"'
        UseShellExecute = $false
        CreateNoWindow = $true
    }))
    $timeoutStart = $timeoutProc.StartTime
    Start-Sleep -Milliseconds 800
    $termPids = Terminate-OwnedProcessTree -RootPid $timeoutProc.Id -RootProcess $timeoutProc -ExpectedStartTime $timeoutStart
    if (@($termPids).Count -lt 2) {
        throw "Process safety fixture failed: expected at least 2 PIDs killed (found $(@($termPids).Count))"
    }
    if (-not $timeoutProc.HasExited) {
        throw "Process safety fixture failed: timeout worker not terminated!"
    }

    # 3. Recycled root protection (StartTime mismatch must reject termination)
    $currProc = [System.Diagnostics.Process]::GetCurrentProcess()
    $fakeStartTime = $currProc.StartTime.AddMinutes(-45)
    $recycledRes = Terminate-OwnedProcessTree -RootPid $currProc.Id -RootProcess $currProc -ExpectedStartTime $fakeStartTime
    if (@($recycledRes).Count -ne 0) {
        throw "Process safety fixture failed: terminated process with mismatched StartTime (recycled PID protection broken)!"
    }
    if ($currProc.HasExited) {
        throw "Process safety fixture failed: current process was terminated by recycled root check!"
    }

    # 4. Recycled descendant protection (descendant with invalid StartTime must not be killed)
    $fakeDesc = @{
        Pid = $currProc.Id
        StartTime = $fakeStartTime
        Process = $currProc
    }
    $dummyParent = [System.Diagnostics.Process]::Start((New-Object System.Diagnostics.ProcessStartInfo -Property @{
        FileName = 'cmd.exe'
        Arguments = '/c ping 127.0.0.1 -n 10 >nul'
        UseShellExecute = $false
        CreateNoWindow = $true
    }))
    $resDesc = Terminate-OwnedProcessTree -RootPid $dummyParent.Id -RootProcess $dummyParent -ExpectedStartTime $dummyParent.StartTime -KnownDescendants @($fakeDesc)
    if (@($resDesc) -contains $currProc.Id) {
        throw "Process safety fixture failed: killed fake descendant!"
    }
    if ($currProc.HasExited) {
        throw "Process safety fixture failed: current process killed as fake descendant!"
    }
    if (-not $dummyParent.HasExited) {
        throw "Process safety fixture failed: dummy parent was not terminated!"
    }

    # 5. Unrelated owned test process protection (do not touch unrelated processes)
    $unrelatedProc = [System.Diagnostics.Process]::Start((New-Object System.Diagnostics.ProcessStartInfo -Property @{
        FileName = 'cmd.exe'
        Arguments = '/c ping 127.0.0.1 -n 10 >nul'
        UseShellExecute = $false
        CreateNoWindow = $true
    }))
    try {
        $unrelatedStart = $unrelatedProc.StartTime
        # Target with mismatched expected start time
        $unrelatedRes = Terminate-OwnedProcessTree -RootPid $unrelatedProc.Id -RootProcess $unrelatedProc -ExpectedStartTime ($unrelatedStart.AddHours(-1))
        if (@($unrelatedRes).Count -ne 0 -or $unrelatedProc.HasExited) {
            throw "Process safety fixture failed: unrelated process was affected by termination!"
        }
    } finally {
        # Cleanly terminate the test process
        try {
            if (-not $unrelatedProc.HasExited) {
                $unrelatedProc.Kill()
                [void]$unrelatedProc.WaitForExit(3000)
            }
        } catch {}
    }

    # 6. Fail-closed enumeration and denial handling verification
    # 6a. Verify that Terminate-OwnedProcessTree strictly refuses termination when ExpectedStartTime is missing
    $mockRaw = [System.Diagnostics.Process]::Start((New-Object System.Diagnostics.ProcessStartInfo -Property @{
        FileName = 'powershell.exe'
        Arguments = '-NoProfile -Command "Start-Sleep -Seconds 10"'
        UseShellExecute = $false
        CreateNoWindow = $true
    }))
    try {
        $rawRes = Terminate-OwnedProcessTree -RootPid $mockRaw.Id -RootProcess $mockRaw
        if (@($rawRes).Count -ne 0 -or $mockRaw.HasExited) {
            throw "Process safety fixture failed: Terminate-OwnedProcessTree did not refuse raw PID without ExpectedStartTime!"
        }
    } finally {
        try {
            if (-not $mockRaw.HasExited) {
                $mockRaw.Kill()
                [void]$mockRaw.WaitForExit(3000)
            }
        } catch {}
    }

    # 6b. Verify that calling Get-VerifiedProcessTree with an already exited process returns empty safely
    $mockExited = [System.Diagnostics.Process]::Start((New-Object System.Diagnostics.ProcessStartInfo -Property @{
        FileName = 'cmd.exe'
        Arguments = '/c exit 0'
        UseShellExecute = $false
        CreateNoWindow = $true
    }))
    [void]$mockExited.WaitForExit()
    $exitedTree = Get-VerifiedProcessTree -RootProcess $mockExited -RootStartTime $mockExited.StartTime
    if (@($exitedTree).Count -ne 0) {
        throw "Process safety fixture failed: Get-VerifiedProcessTree did not fail closed on exited process!"
    }

    # 6c. Verify controlled live CIM-denial fault injection, cleanup propagation, and unrelated process protection
    $unrelatedForCim = [System.Diagnostics.Process]::Start((New-Object System.Diagnostics.ProcessStartInfo -Property @{
        FileName = 'cmd.exe'
        Arguments = '/c ping 127.0.0.1 -n 15 >nul'
        UseShellExecute = $false
        CreateNoWindow = $true
    }))
    $ownedWorker = [System.Diagnostics.Process]::Start((New-Object System.Diagnostics.ProcessStartInfo -Property @{
        FileName = 'cmd.exe'
        Arguments = '/c ping 127.0.0.1 -n 15 >nul'
        UseShellExecute = $false
        CreateNoWindow = $true
    }))
    $ownedChildWorker = [System.Diagnostics.Process]::Start((New-Object System.Diagnostics.ProcessStartInfo -Property @{
        FileName = 'cmd.exe'
        Arguments = '/c ping 127.0.0.1 -n 15 >nul'
        UseShellExecute = $false
        CreateNoWindow = $true
    }))

    $script:SimulateCimDenial = $true
    try {
        $ownedStart = $ownedWorker.StartTime
        $ownedChildStart = $ownedChildWorker.StartTime
        $unrelatedStart = $unrelatedForCim.StartTime

        # Direct test (N2): Terminate-OwnedProcessTree with -SkipDiscovery must safely terminate
        # both the owned root and all registered known descendants without calling CIM, while
        # strictly protecting unrelated processes.
        $knownDescList = @(
            @{
                Process   = $ownedChildWorker
                Pid       = $ownedChildWorker.Id
                StartTime = $ownedChildStart
            }
        )
        $termList = Terminate-OwnedProcessTree -RootPid $ownedWorker.Id -RootProcess $ownedWorker -ExpectedStartTime $ownedStart -KnownDescendants $knownDescList -SkipDiscovery
        if (-not $ownedWorker.HasExited) {
            throw "Process safety fixture failed: owned worker survived termination under -SkipDiscovery!"
        }
        if (-not $ownedChildWorker.HasExited) {
            throw "Process safety fixture failed: owned child worker survived termination under -SkipDiscovery with known descendants!"
        }
        if ($unrelatedForCim.HasExited) {
            throw "Process safety fixture failed: unrelated process was killed during CIM denial cleanup!"
        }

        # End-to-end test: Invoke-ProcessWithTimeout fails closed under live CIM denial and cleans up the launched worker
        $e2eDeniedThrown = $false
        try {
            Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments '/c ping 127.0.0.1 -n 15 >nul' -TimeoutSeconds 5 -PhaseName 'Live CIM Denial Fixture' | Out-Null
        } catch {
            if ($_ -match 'Simulated CIM access denial') {
                $e2eDeniedThrown = $true
            } else {
                throw
            }
        }
        if (-not $e2eDeniedThrown) {
            throw "Process safety fixture failed: Invoke-ProcessWithTimeout did not fail closed on live CIM denial!"
        }
        if ($unrelatedForCim.HasExited) {
            throw "Process safety fixture failed: unrelated process died during end-to-end CIM denial test!"
        }
    } finally {
        $script:SimulateCimDenial = $false
        try { if (-not $ownedWorker.HasExited) { $ownedWorker.Kill() } } catch {}
        try { if (-not $ownedChildWorker.HasExited) { $ownedChildWorker.Kill() } } catch {}
        try { if (-not $unrelatedForCim.HasExited) { $unrelatedForCim.Kill() } } catch {}
    }

    Write-Host "Process ownership safety fixtures: PASS (normal completion, timeout, recycled root, recycled descendant, unrelated protection, fail-closed and live CIM denial verified)"
}

Write-Host "========================================================"
Write-Host " Phase 1: Verifying MCU Export Manifest & Exporting Bundle"
Write-Host "========================================================"
$exportScript = Join-Path $repoRoot 'tools\export_mcu.bat'

# Verify manifest check
$manifestRes = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c call `"$exportScript`" --verify" -TimeoutSeconds 30 -PhaseName 'Manifest Verification'
if ($manifestRes.ExitCode -ne 0) {
    if ($manifestRes.Stdout) { Write-Host $manifestRes.Stdout }
    if ($manifestRes.Stderr) { Write-Error $manifestRes.Stderr }
    throw "MCU export manifest verification failed with exit code $($manifestRes.ExitCode)"
}
Write-Host "MCU export manifest: PASS"

# Run process ownership safety fixtures
Invoke-ProcessSafetyFixtures

# -------------------------------------------------------------
# -------------------------------------------------------------
# X1/Y2 Collision Safety & Ownership Unit Tests (Disposable Fixture)
# -------------------------------------------------------------
Write-Host "`nTesting X1/Y2 export collision safety and artifact ownership..."
$fixtureGuid = [System.Guid]::NewGuid().ToString("N")
$fixtureDir = Join-Path $env:TEMP "smart_hub_ui_x1_fixture_$fixtureGuid"
New-Item -ItemType Directory -Path "$fixtureDir\tools", "$fixtureDir\docs", "$fixtureDir\dest" -Force | Out-Null
$fixtureSuccess = $false
try {
    # 1. Exact-destination collision refusal: export to an EXISTING explicit destination MUST fail and preserve sentinel
    $sentinelZip = Join-Path $fixtureDir 'collision_sentinel.zip'
    [IO.File]::WriteAllBytes($sentinelZip, [byte[]]@(0x50, 0x4B, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00))
    $sentinelHashBefore = (Get-FileHash -LiteralPath $sentinelZip -Algorithm SHA256).Hash

    $collisionRes = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c call `"$exportScript`" `"$fixtureDir\dest`" `"$sentinelZip`"" -TimeoutSeconds 60 -PhaseName 'X1 Collision Refusal'
    if ($collisionRes.ExitCode -eq 0) {
        throw "Export succeeded (exit 0) when targeting an existing explicit destination! It must fail with nonzero exit code."
    }
    $sentinelHashAfter = (Get-FileHash -LiteralPath $sentinelZip -Algorithm SHA256).Hash
    if ($sentinelHashBefore -ne $sentinelHashAfter) {
        throw "Sentinel ZIP was modified or overwritten by collided export!"
    }

    # 2. Non-colliding explicit export succeeds
    $customZip = Join-Path $fixtureDir 'custom_export.zip'
    $expRes = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c call `"$exportScript`" `"$fixtureDir\dest`" `"$customZip`"" -TimeoutSeconds 60 -PhaseName 'X1 Custom Zip Export'
    if ($expRes.ExitCode -ne 0) { throw "Custom zip export failed with code $($expRes.ExitCode)" }
    if (-not (Test-Path -LiteralPath $customZip -PathType Leaf)) { throw "Expected custom zip not created: $customZip" }

    # 3. Y2 Secondary destination ZIP collision refusal: existing secondary zip must NOT be overwritten
    $secondaryDestZip = Join-Path "$fixtureDir\dest" 'secondary_test.zip'
    [IO.File]::WriteAllBytes($secondaryDestZip, [byte[]]@(0x50, 0x4B, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00))
    $secondaryHashBefore = (Get-FileHash -LiteralPath $secondaryDestZip -Algorithm SHA256).Hash
    $newPrimaryZip = Join-Path $fixtureDir 'secondary_test.zip'
    $secRes = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c call `"$exportScript`" `"$fixtureDir\dest`" `"$newPrimaryZip`"" -TimeoutSeconds 60 -PhaseName 'Y2 Secondary Destination Collision Refusal'
    if ($secRes.ExitCode -eq 0) {
        throw "Export succeeded (exit 0) when copying over an existing secondary destination ZIP! It must refuse with nonzero exit code."
    }
    $secondaryHashAfter = (Get-FileHash -LiteralPath $secondaryDestZip -Algorithm SHA256).Hash
    if ($secondaryHashBefore -ne $secondaryHashAfter) {
        throw "Secondary destination ZIP sentinel was modified or overwritten!"
    }
    if (-not (Test-Path -LiteralPath $newPrimaryZip -PathType Leaf)) {
        throw "Newly created primary export artifact was not retained after secondary destination refusal!"
    }

    # 4. Default export with timestamp in working directory
    # Copy exporter inputs into fixture directory to test isolated default export
    Copy-Item -LiteralPath $exportScript -Destination "$fixtureDir\tools\export_mcu.bat"
    $reqFiles = 'lora_comm.h','lora_hub_link.h','LORA_PROTOCOL.md','ui.c','ui.h','ui_auto.h','ui_auto.c','ui_internal.h','ui_mcu_profile.h','ui_types.h','ui_theme.h','ui_theme.c','ui_icons.h','ui_icons.c','ui_fonts.h','ui_fonts.c','ui_splash_logo.c'
    foreach ($f in $reqFiles) { Copy-Item -LiteralPath (Join-Path $repoRoot $f) -Destination (Join-Path $fixtureDir $f) }
    foreach ($f in @('HARDWARE_OVERVIEW.md','MCU_BASELINE.md','README.md','UI_DESIGN_BRIEF.md')) {
        Copy-Item -LiteralPath "$repoRoot\docs\$f" -Destination "$fixtureDir\docs\$f"
    }

    $fixtureExpScript = "$fixtureDir\tools\export_mcu.bat"
    $defaultRes1 = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c cd /d `"$fixtureDir`" && call `"$fixtureExpScript`"" -TimeoutSeconds 60 -PhaseName 'X1 Default Export 1'
    if ($defaultRes1.ExitCode -ne 0) { throw "First default export failed with code $($defaultRes1.ExitCode)" }

    $defaultZips1 = @(Get-ChildItem -LiteralPath $fixtureDir -Filter 'smart_hub_ui_*.zip')
    if ($defaultZips1.Count -ne 1) { throw "Expected exactly 1 default zip, found $($defaultZips1.Count)" }
    $zip1Name = $defaultZips1[0].Name
    if ($zip1Name -notmatch '^smart_hub_ui_\d{8}_\d{4}\.zip$') {
        throw "Default export filename '$zip1Name' does not contain valid timestamp yyyyMMdd_HHmm"
    }
    $zip1Hash = (Get-FileHash -LiteralPath $defaultZips1[0].FullName -Algorithm SHA256).Hash

    # 5. Same-minute collision handling: sequential second default export must suffix _1 and preserve original
    $defaultRes2 = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c cd /d `"$fixtureDir`" && call `"$fixtureExpScript`"" -TimeoutSeconds 60 -PhaseName 'X1 Default Export 2'
    if ($defaultRes2.ExitCode -ne 0) { throw "Second default export failed with code $($defaultRes2.ExitCode)" }

    $defaultZips2 = @(Get-ChildItem -LiteralPath $fixtureDir -Filter 'smart_hub_ui_*.zip')
    if ($defaultZips2.Count -ne 2) { throw "Expected 2 default zips after second export, found $($defaultZips2.Count)" }
    $zip1HashAfter = (Get-FileHash -LiteralPath $defaultZips1[0].FullName -Algorithm SHA256).Hash
    if ($zip1Hash -ne $zip1HashAfter) {
        throw "First default export was modified by second export!"
    }

    # 6. Y2 Inherited unrelated TS environment variable isolation
    $unrelatedRes = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c set TS=UNRELATED_VALUE&& cd /d `"$fixtureDir`" && call `"$fixtureExpScript`"" -TimeoutSeconds 60 -PhaseName 'Y2 Unrelated TS Export'
    if ($unrelatedRes.ExitCode -ne 0) { throw "Default export under inherited TS failed with code $($unrelatedRes.ExitCode)" }
    if (Test-Path -LiteralPath (Join-Path $fixtureDir 'smart_hub_ui_UNRELATED_VALUE.zip') -PathType Leaf) {
        throw "Exproter accepted unrelated TS environment variable verbatim ('smart_hub_ui_UNRELATED_VALUE.zip')!"
    }

    # 7. Y2/R3 Concurrent default exports: inspect exact returned paths, distinct artifacts, production membership & source identity
    $cDir = Join-Path $fixtureDir 'concurrent'
    New-Item -ItemType Directory -Path "$cDir\tools", "$cDir\docs" -Force | Out-Null
    Copy-Item -LiteralPath $fixtureExpScript -Destination "$cDir\tools\export_mcu.bat"
    foreach ($f in $reqFiles) { Copy-Item -LiteralPath (Join-Path $repoRoot $f) -Destination (Join-Path $cDir $f) }
    foreach ($f in @('HARDWARE_OVERVIEW.md','MCU_BASELINE.md','README.md','UI_DESIGN_BRIEF.md')) {
        Copy-Item -LiteralPath "$repoRoot\docs\$f" -Destination "$cDir\docs\$f"
    }
    # Sentinel in concurrent directory
    $cSentinel = Join-Path $cDir 'existing_sentinel.txt'
    [IO.File]::WriteAllText($cSentinel, "SENTINEL_CONCURRENT_CHECK_20260917")
    $cSentinelHashBefore = (Get-FileHash -LiteralPath $cSentinel -Algorithm SHA256).Hash

    $cScript = "$cDir\tools\export_mcu.bat"
    $cStartInfos = @(1, 2 | ForEach-Object {
        $pi = New-Object System.Diagnostics.ProcessStartInfo
        $pi.FileName = 'cmd.exe'
        $pi.Arguments = "/c set TS=20260916_1200&& cd /d `"$cDir`" && call `"$cScript`""
        $pi
    })
    $cResults = Invoke-ConcurrentWorkers -StartInfos $cStartInfos -TimeoutSeconds 60 -PhaseName 'Y2/R3 Concurrent Default Exports'
    foreach ($res in $cResults) {
        if ($res.ExitCode -ne 0) {
            throw "Concurrent default export worker failed with exit code $($res.ExitCode): $($res.Stderr)"
        }
    }
    # Extract exact returned paths from stdout
    $returnedPaths = @()
    foreach ($res in $cResults) {
        $m = [regex]::Match($res.Stdout, 'Created:\s*(.+smart_hub_ui_20260916_1200[^\r\n]*)')
        if (-not $m.Success) {
            throw "Could not parse returned archive path from worker stdout: $($res.Stdout)"
        }
        $returnedPaths += $m.Groups[1].Value.Trim()
    }
    if ($returnedPaths.Count -ne 2 -or ($returnedPaths[0] -ieq $returnedPaths[1])) {
        throw "Concurrent default exports did not return 2 distinct paths! Returned: $($returnedPaths -join ', ')"
    }
    $reqDocs = @('HARDWARE_OVERVIEW.md','MCU_BASELINE.md','README.md','UI_DESIGN_BRIEF.md')
    foreach ($rp in $returnedPaths) {
        if (-not (Test-Path -LiteralPath $rp -PathType Leaf)) {
            throw "Returned archive path does not exist on disk: $rp"
        }
        # Streamed SHA256 equality verification for EVERY manifest entry
        Assert-ZipEntriesSha256 -ZipPath $rp -SourceDir $cDir -RequiredFiles $reqFiles -RequiredDocs $reqDocs
    }
    # Same-length tampering negative test: mutating 1 byte with unchanged length must be detected
    $tamperDetected = $false
    $tamperFile = Join-Path $cDir 'ui_types.h'
    $origBytes = [IO.File]::ReadAllBytes($tamperFile)
    $tamperedBytes = [byte[]]$origBytes.Clone()
    $tamperedBytes[0] = ($tamperedBytes[0] -bxor 0xFF)
    try {
        [IO.File]::WriteAllBytes($tamperFile, $tamperedBytes)
        try {
            Assert-ZipEntriesSha256 -ZipPath $returnedPaths[0] -SourceDir $cDir -RequiredFiles $reqFiles -RequiredDocs $reqDocs
        } catch {
            if ($_ -match 'SHA256 mismatch for entry ui_types.h') {
                $tamperDetected = $true
            } else {
                throw
            }
        }
    } finally {
        [IO.File]::WriteAllBytes($tamperFile, $origBytes)
    }
    if (-not $tamperDetected) {
        throw "Same-length tampering negative test failed: modified source byte with unchanged length did not trigger SHA256 mismatch!"
    }

    # Negative test: unexpected entry in ZIP must be rejected
    $unexpectedZip = Join-Path $cDir 'unexpected_entry_test.zip'
    Copy-Item -LiteralPath $returnedPaths[0] -Destination $unexpectedZip
    $uZipArchive = [System.IO.Compression.ZipFile]::Open($unexpectedZip, [System.IO.Compression.ZipArchiveMode]::Update)
    try {
        $uEntry = $uZipArchive.CreateEntry('unexpected_file.txt')
        $uStream = $uEntry.Open()
        try {
            $dummyBytes = [System.Text.Encoding]::UTF8.GetBytes("UNEXPECTED")
            $uStream.Write($dummyBytes, 0, $dummyBytes.Length)
        } finally { $uStream.Dispose() }
    } finally { $uZipArchive.Dispose() }

    $unexpectedDetected = $false
    try {
        Assert-ZipEntriesSha256 -ZipPath $unexpectedZip -SourceDir $cDir -RequiredFiles $reqFiles -RequiredDocs $reqDocs
    } catch {
        if ($_ -match 'contains unexpected entry: unexpected_file\.txt') {
            $unexpectedDetected = $true
        } else {
            throw
        }
    }
    if (-not $unexpectedDetected) {
        throw "Unexpected entry negative test failed: extra zip entry was not rejected!"
    }
    Remove-Item -LiteralPath $unexpectedZip -Force -ErrorAction SilentlyContinue

    # Negative test: duplicate entry in ZIP must be rejected
    $duplicateZip = Join-Path $cDir 'duplicate_entry_test.zip'
    Copy-Item -LiteralPath $returnedPaths[0] -Destination $duplicateZip
    $dZipArchive = [System.IO.Compression.ZipFile]::Open($duplicateZip, [System.IO.Compression.ZipArchiveMode]::Update)
    try {
        $dEntry = $dZipArchive.CreateEntry('ui.c')
        $dStream = $dEntry.Open()
        try {
            $dummyBytes = [System.Text.Encoding]::UTF8.GetBytes("DUPLICATE")
            $dStream.Write($dummyBytes, 0, $dummyBytes.Length)
        } finally { $dStream.Dispose() }
    } finally { $dZipArchive.Dispose() }

    $duplicateDetected = $false
    try {
        Assert-ZipEntriesSha256 -ZipPath $duplicateZip -SourceDir $cDir -RequiredFiles $reqFiles -RequiredDocs $reqDocs
    } catch {
        if ($_ -match 'contains duplicate entry: ui\.c') {
            $duplicateDetected = $true
        } else {
            throw
        }
    }
    if (-not $duplicateDetected) {
        throw "Duplicate entry negative test failed: duplicate zip entry was not rejected!"
    }
    Remove-Item -LiteralPath $duplicateZip -Force -ErrorAction SilentlyContinue

    # Verify existing sentinel byte-identical
    $cSentinelHashAfter = (Get-FileHash -LiteralPath $cSentinel -Algorithm SHA256).Hash
    if ($cSentinelHashBefore -ne $cSentinelHashAfter) {
        throw "Concurrent default export modified existing sentinel in fixture directory!"
    }

    # 8. R3 Concurrent explicit destination race: two exports racing for the SAME explicit destination
    $raceDir = Join-Path $fixtureDir 'explicit_race'
    New-Item -ItemType Directory -Path "$raceDir\tools", "$raceDir\docs", "$raceDir\dest" -Force | Out-Null
    Copy-Item -LiteralPath $fixtureExpScript -Destination "$raceDir\tools\export_mcu.bat"
    foreach ($f in $reqFiles) { Copy-Item -LiteralPath (Join-Path $repoRoot $f) -Destination (Join-Path $raceDir $f) }
    foreach ($f in @('HARDWARE_OVERVIEW.md','MCU_BASELINE.md','README.md','UI_DESIGN_BRIEF.md')) {
        Copy-Item -LiteralPath "$repoRoot\docs\$f" -Destination "$raceDir\docs\$f"
    }
    $targetRaceZip = Join-Path $raceDir 'race_winner.zip'
    $rScript = "$raceDir\tools\export_mcu.bat"
    $rStartInfos = @(1, 2 | ForEach-Object {
        $pi = New-Object System.Diagnostics.ProcessStartInfo
        $pi.FileName = 'cmd.exe'
        $pi.Arguments = "/c cd /d `"$raceDir`" && call `"$rScript`" `"$raceDir\dest`" `"$targetRaceZip`""
        $pi
    })
    $rResults = Invoke-ConcurrentWorkers -StartInfos $rStartInfos -TimeoutSeconds 60 -PhaseName 'R3 Explicit Destination Race'
    $rWinners = @($rResults | Where-Object { $_.ExitCode -eq 0 })
    $rLosers = @($rResults | Where-Object { $_.ExitCode -ne 0 })
    if ($rWinners.Count -ne 1 -or $rLosers.Count -ne 1) {
        throw "Expected exactly 1 winner (exit 0) and 1 loser (exit != 0) in explicit destination race! Got $($rWinners.Count) winners, $($rLosers.Count) losers."
    }
    $loserDiag = $rLosers[0].Stderr + " " + $rLosers[0].Stdout
    if ($loserDiag -notmatch 'Destination already exists|Failed to move') {
        throw "Losing export worker did not report expected collision error! Output: $loserDiag"
    }
    if (-not (Test-Path -LiteralPath $targetRaceZip -PathType Leaf)) {
        throw "Winning explicit race archive was not created at $targetRaceZip"
    }
    # Verify winning archive is valid and complete with streamed SHA256 for all entries
    Assert-ZipEntriesSha256 -ZipPath $targetRaceZip -SourceDir $raceDir -RequiredFiles $reqFiles -RequiredDocs @('HARDWARE_OVERVIEW.md','MCU_BASELINE.md','README.md','UI_DESIGN_BRIEF.md')

    # 9. R3 Controlled failure and watchdog timeout paths
    $failDir = Join-Path $fixtureDir 'controlled_fail'
    New-Item -ItemType Directory -Path "$failDir\tools", "$failDir\docs" -Force | Out-Null
    Copy-Item -LiteralPath $fixtureExpScript -Destination "$failDir\tools\export_mcu.bat"
    $failSentinel = Join-Path $failDir 'unrelated_sentinel.dat'
    [IO.File]::WriteAllBytes($failSentinel, [byte[]]@(0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04))
    $failSentinelHashBefore = (Get-FileHash -LiteralPath $failSentinel -Algorithm SHA256).Hash

    # Intentionally omit required file (ui.c)
    foreach ($f in $reqFiles) {
        if ($f -ne 'ui.c') { Copy-Item -LiteralPath (Join-Path $repoRoot $f) -Destination (Join-Path $failDir $f) }
    }
    foreach ($f in @('HARDWARE_OVERVIEW.md','MCU_BASELINE.md','README.md','UI_DESIGN_BRIEF.md')) {
        Copy-Item -LiteralPath "$repoRoot\docs\$f" -Destination "$failDir\docs\$f"
    }
    $failScript = "$failDir\tools\export_mcu.bat"
    $failRes = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c cd /d `"$failDir`" && call `"$failScript`"" -TimeoutSeconds 30 -PhaseName 'R3 Controlled Failure'
    if ($failRes.ExitCode -eq 0) {
        throw "Controlled failure export unexpectedly succeeded (exit 0) despite missing ui.c!"
    }
    $failDiag = $failRes.Stdout + " " + $failRes.Stderr
    if ($failDiag -notmatch 'ERROR: missing ui\.c') {
        throw "Controlled failure did not report missing file diagnostic: $failDiag"
    }
    $failSentinelHashAfter = (Get-FileHash -LiteralPath $failSentinel -Algorithm SHA256).Hash
    if ($failSentinelHashBefore -ne $failSentinelHashAfter) {
        throw "Controlled failure modified unrelated sentinel file!"
    }

    # Watchdog timeout test for Invoke-ProcessWithTimeout
    $watchdogTriggered = $false
    $singleTerminatedPids = @()
    try {
        Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments '/c powershell -NoProfile -Command "Start-Sleep -Seconds 15"' -TimeoutSeconds 2 -PhaseName 'R3 Single Watchdog Test' | Out-Null
    } catch {
        if ($_ -match 'Watchdog timeout.*PIDs ([\d,]+) terminated') {
            $watchdogTriggered = $true
            $singleTerminatedPids = $Matches[1].Split(',') | ForEach-Object { [int]$_ }
        } elseif ($_ -match 'Watchdog timeout.*\(PID (\d+) terminated\)') {
            $watchdogTriggered = $true
            $singleTerminatedPids = @([int]$Matches[1])
        } else {
            throw
        }
    }
    if (-not $watchdogTriggered) {
        throw "Single watchdog timeout test failed to trigger watchdog exception!"
    }
    # Verify multiple PIDs (parent cmd AND child powershell) were tracked and terminated
    if ($singleTerminatedPids.Count -lt 2) {
        throw "Single watchdog timeout test expected parent cmd and child powershell PIDs (found $($singleTerminatedPids.Count) PIDs: $($singleTerminatedPids -join ', '))"
    }
    foreach ($pidToCheck in $singleTerminatedPids) {
        $pidDead = $false
        try {
            $pCheck = [System.Diagnostics.Process]::GetProcessById($pidToCheck)
            if ($pCheck.HasExited) { $pidDead = $true }
        } catch [System.ArgumentException] {
            $pidDead = $true
        }
        if (-not $pidDead) {
            throw "Single watchdog timeout test failed: process PID $pidToCheck is still alive!"
        }
    }

    # Watchdog timeout test for Invoke-ConcurrentWorkers
    $cWatchdogTriggered = $false
    $cTerminatedPids = @()
    $cTimeoutInfos = @(1, 2 | ForEach-Object {
        $pi = New-Object System.Diagnostics.ProcessStartInfo
        $pi.FileName = 'cmd.exe'
        $pi.Arguments = '/c powershell -NoProfile -Command "Start-Sleep -Seconds 15"'
        $pi
    })
    try {
        Invoke-ConcurrentWorkers -StartInfos $cTimeoutInfos -TimeoutSeconds 2 -PhaseName 'R3 Concurrent Watchdog Test' | Out-Null
    } catch {
        if ($_ -match 'Watchdog timeout.*all PIDs ([\d,]+)') {
            $cWatchdogTriggered = $true
            $cTerminatedPids = $Matches[1].Split(',') | ForEach-Object { [int]$_ }
        } elseif ($_ -match 'Watchdog timeout.*PID (\d+)') {
            $cWatchdogTriggered = $true
            $cTerminatedPids = @([int]$Matches[1])
        } else {
            throw
        }
    }
    if (-not $cWatchdogTriggered) {
        throw "Concurrent watchdog timeout test failed to trigger watchdog exception!"
    }
    # Verify that workers and their descendants were tracked and terminated (>= 4 processes: 2 cmd + 2 powershell)
    if ($cTerminatedPids.Count -lt 4) {
        throw "Concurrent watchdog timeout test expected at least 4 PIDs (2 cmd parents + 2 child powershells), found $($cTerminatedPids.Count) PIDs: $($cTerminatedPids -join ', ')"
    }
    foreach ($cPid in $cTerminatedPids) {
        $cPidDead = $false
        try {
            $pCheck = [System.Diagnostics.Process]::GetProcessById($cPid)
            if ($pCheck.HasExited) { $cPidDead = $true }
        } catch [System.ArgumentException] {
            $cPidDead = $true
        }
        if (-not $cPidDead) {
            throw "Concurrent watchdog timeout test failed: child PID $cPid is still alive!"
        }
    }

    $fixtureSuccess = $true
    Write-Host "X1/Y2/R3 collision safety & failure paths: PASS (explicit refusal, secondary destination protection, valid timestamp fallback, atomic suffixing, concurrent isolation, explicit race, controlled failure & watchdog verified)"
} catch {
    Write-Host "Diagnostic evidence retained at: $fixtureDir"
    throw
} finally {
    if ($fixtureSuccess) {
        $canFixture = [IO.Path]::GetFullPath($fixtureDir)
        $canTemp = [IO.Path]::GetFullPath($env:TEMP).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
        $tempPrefix = $canTemp + [IO.Path]::DirectorySeparatorChar
        if ($canFixture.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
            $canFixture -ne $canTemp -and
            $canFixture -ne $repoRoot -and
            (Test-Path -LiteralPath $canFixture)) {
            Remove-Item -LiteralPath $canFixture -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

# Locate cached offline LVGL source tree
$lvglSrc = Join-Path $BuildDir '_deps\lvgl-src'
if (-not (Test-Path -LiteralPath (Join-Path $lvglSrc 'lvgl.h') -PathType Leaf)) {
    throw "Cached offline LVGL source tree not found at $lvglSrc. Please run tools/build_sim.bat first."
}

# Locate C compiler and CMake
if (-not $Compiler) {
    $bundled = 'C:\Toolchains\w64devkit\bin\gcc.exe'
    if (Test-Path -LiteralPath $bundled -PathType Leaf) {
        $Compiler = $bundled
    } else {
        $Compiler = (Get-Command gcc -ErrorAction Stop).Source
    }
}
$Compiler = [IO.Path]::GetFullPath($Compiler)
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "C compiler not found: $Compiler"
}
$compilerDir = Split-Path -Parent $Compiler
if (($env:PATH -split ';') -notcontains $compilerDir) {
    $env:PATH = $compilerDir + ';' + $env:PATH
}
$cmake = (Get-Command cmake -ErrorAction Stop).Source

# Create validated temporary staging directory
$guid = [System.Guid]::NewGuid().ToString("N")
$stageDir = Join-Path $env:TEMP "smart_hub_ui_isolated_verify_$guid"
$stageExportDest = Join-Path $stageDir 'export_dest'
New-Item -ItemType Directory -Path $stageExportDest -Force | Out-Null

$createdZipPath = $null

try {
    # Define exact unique task-owned zip path inside the stage directory
    $taskZipPath = Join-Path $stageDir "smart_hub_ui_task_$guid.zip"
    $createdZipPath = $taskZipPath

    # Export MCU package directly into staging export destination with exact task-owned zip
    Write-Host "`nExporting MCU bundle to staging directory..."
    $exportRes = Invoke-ProcessWithTimeout -FilePath 'cmd.exe' -RawArguments "/c call `"$exportScript`" `"$stageExportDest`" `"$taskZipPath`"" -TimeoutSeconds 60 -PhaseName 'MCU Export'
    if ($exportRes.ExitCode -ne 0) {
        if ($exportRes.Stdout) { Write-Host $exportRes.Stdout }
        if ($exportRes.Stderr) { Write-Error $exportRes.Stderr }
        throw "MCU export execution failed with exit code $($exportRes.ExitCode)"
    }

    if (-not (Test-Path -LiteralPath $taskZipPath -PathType Leaf)) {
        throw "Task-owned export archive not found at $taskZipPath"
    }

    $stageSrcDir = Join-Path $stageExportDest 'smart_hub_ui'
    if (-not (Test-Path -LiteralPath $stageSrcDir -PathType Container)) {
        throw "Exported smart_hub_ui directory not found at $stageSrcDir"
    }

    Write-Host "`n========================================================"
    Write-Host " Phase 2: Isolated Host Build from Exported Artifacts"
    Write-Host "========================================================"
    Write-Host "  Exported Staging: $stageSrcDir"
    Write-Host "  LVGL Source:      $lvglSrc"
    Write-Host "  Compiler:         $Compiler"

    # Verify key exported files exist in the artifact directory
    $expectedArtifactFiles = @(
        'ui.c', 'ui.h', 'ui_auto.c', 'ui_auto.h',
        'ui_theme.c', 'ui_theme.h', 'ui_icons.c', 'ui_icons.h',
        'ui_fonts.c', 'ui_fonts.h', 'ui_splash_logo.c',
        'ui_internal.h', 'ui_mcu_profile.h', 'ui_types.h',
        'lora_comm.h', 'lora_hub_link.h'
    )
    foreach ($f in $expectedArtifactFiles) {
        $p = Join-Path $stageSrcDir $f
        if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
            throw "Exported artifact verification failed: missing expected file $f in $stageSrcDir"
        }
    }
    Write-Host "Export artifact membership: PASS ($($expectedArtifactFiles.Count) verified files)"

    # Copy production smoke runner into the staged source directory
    $smokeSrc = Join-Path $repoRoot 'sim_pc\smoke_production.c'
    if (-not (Test-Path -LiteralPath $smokeSrc -PathType Leaf)) {
        throw "smoke_production.c not found at $smokeSrc"
    }
    Copy-Item -LiteralPath $smokeSrc -Destination (Join-Path $stageSrcDir 'smoke_production.c') -Force

    # Write isolated lv_conf.h
    $lvConfContent = @"
#ifndef LV_CONF_H
#define LV_CONF_H

#include "ui_mcu_profile.h"

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE UI_LVGL_HEAP_BYTES

/* Study 12: all surfaces are flat fills; gradient cache disabled */
#define LV_GRAD_CACHE_DEF_SIZE 0

#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM 0
#define LV_DPI_DEF 130
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Production fonts */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif /* LV_CONF_H */
"@
    Set-Content -Path (Join-Path $stageSrcDir 'lv_conf.h') -Value $lvConfContent -Encoding Ascii

    # Write isolated CMakeLists.txt
    $lvglSrcCmake = $lvglSrc.Replace('\', '/')
    $cmakeContent = @"
cmake_minimum_required(VERSION 3.20)
project(smart_hub_ui_isolated_smoke C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LV_CONF_PATH "`${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE FILEPATH "" FORCE)
add_subdirectory("$lvglSrcCmake" lvgl EXCLUDE_FROM_ALL)

add_executable(smoke_production
    smoke_production.c
    ui.c
    ui_auto.c
    ui_theme.c
    ui_icons.c
    ui_fonts.c
    ui_splash_logo.c
)

target_include_directories(smoke_production PRIVATE
    `${CMAKE_CURRENT_SOURCE_DIR}
    "$lvglSrcCmake"
)

target_compile_definitions(smoke_production PRIVATE
    NDEBUG
    LV_CONF_INCLUDE_SIMPLE
)

target_link_libraries(smoke_production PRIVATE lvgl)

if(MSVC)
    target_compile_options(smoke_production PRIVATE /W4 /WX /O2)
else()
    target_compile_options(smoke_production PRIVATE -Wall -Wextra -Werror -O2)
endif()
"@
    Set-Content -Path (Join-Path $stageSrcDir 'CMakeLists.txt') -Value $cmakeContent -Encoding Ascii

    # Configure isolated CMake with watchdog
    $stageBuildDir = Join-Path $stageSrcDir 'build'
    Write-Host "`nConfiguring isolated build with CMake (watchdog: 60s)..."
    $configArgs = @(
        '-S', $stageSrcDir,
        '-B', $stageBuildDir,
        '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release',
        "-DCMAKE_C_COMPILER=$($Compiler.Replace('\', '/'))"
    )
    $cfgRes = Invoke-ProcessWithTimeout -FilePath $cmake -ArgumentList $configArgs -TimeoutSeconds 60 -PhaseName 'CMake Configure'
    if ($cfgRes.ExitCode -ne 0) {
        if ($cfgRes.Stdout) { Write-Host $cfgRes.Stdout }
        if ($cfgRes.Stderr) { Write-Error $cfgRes.Stderr }
        throw "Isolated CMake configuration failed with exit code $($cfgRes.ExitCode)"
    }

    # Build isolated smoke executable with watchdog
    Write-Host "`nCompiling isolated smoke_production (Release, -Wall -Wextra -Werror, watchdog: 60s)..."
    $buildArgs = @('--build', $stageBuildDir, '--target', 'smoke_production', '--config', 'Release')
    $bldRes = Invoke-ProcessWithTimeout -FilePath $cmake -ArgumentList $buildArgs -TimeoutSeconds 60 -PhaseName 'CMake Build'
    if ($bldRes.ExitCode -ne 0) {
        if ($bldRes.Stdout) { Write-Host $bldRes.Stdout }
        if ($bldRes.Stderr) { Write-Error $bldRes.Stderr }
        throw "Isolated CMake build failed with exit code $($bldRes.ExitCode)"
    }

    $smokeExe = Join-Path $stageBuildDir 'smoke_production.exe'
    if (-not (Test-Path -LiteralPath $smokeExe -PathType Leaf)) {
        throw "Isolated smoke_production.exe not found at $smokeExe"
    }

    Write-Host "`n========================================================"
    Write-Host " Phase 3: Running Production Smoke Test (Clean Run, watchdog: 30s)"
    Write-Host "========================================================"
    $cleanRes = Invoke-ProcessWithTimeout -FilePath $smokeExe -ArgumentList @() -TimeoutSeconds 30 -PhaseName 'Clean Smoke Run'
    if ($cleanRes.ExitCode -ne 0) {
        if ($cleanRes.Stdout) { Write-Host $cleanRes.Stdout }
        if ($cleanRes.Stderr) { Write-Error $cleanRes.Stderr }
        throw "Isolated smoke_production clean run failed with exit code $($cleanRes.ExitCode) (expected 0)"
    }
    if ($cleanRes.Stdout) { Write-Host $cleanRes.Stdout }
    Write-Host "Clean run: PASS (exit code 0)"

    Write-Host "`n========================================================"
    Write-Host " Phase 4: Running Negative Control (--fail-check, watchdog: 30s)"
    Write-Host "========================================================"
    # Must launch successfully, exit with strictly 1, and output expected error to stderr.
    # No catch-all: launch failures, crashes, or timeouts throw and fail verification.
    $failRes = Invoke-ProcessWithTimeout -FilePath $smokeExe -ArgumentList @('--fail-check') -TimeoutSeconds 30 -PhaseName 'Negative Control'
    Write-Host "Process exit code: $($failRes.ExitCode)"
    if ($failRes.Stdout) { Write-Host "stdout:`n$($failRes.Stdout)" }
    if ($failRes.Stderr) { Write-Host "stderr:`n$($failRes.Stderr)" }

    if ($failRes.ExitCode -ne 1) {
        throw "Negative control check failed: expected exit code 1, but received $($failRes.ExitCode)"
    }
    if ($failRes.Stderr -notmatch '\[SMOKE ERROR\] Check failed: 1 == 2') {
        throw "Negative control check failed: stderr did not contain expected '[SMOKE ERROR] Check failed: 1 == 2'"
    }
    Write-Host "Negative control: PASS (strictly exit code 1 with verified error message)"

    Write-Host "`n========================================================"
    Write-Host " Production verification & Release checks: ALL PASSED "
    Write-Host "========================================================"
    $verificationPassed = $true
}
finally {
    # Guarded cleanup of staging directory: must be strictly rooted inside TEMP and not repo root
    $canonicalStage = [IO.Path]::GetFullPath($stageDir)
    $canonicalTemp = [IO.Path]::GetFullPath($env:TEMP).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $tempPrefix = $canonicalTemp + [IO.Path]::DirectorySeparatorChar

    if ($canonicalStage.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
        $canonicalStage -ne $canonicalTemp -and
        $canonicalStage -ne $repoRoot -and
        (Test-Path -LiteralPath $canonicalStage)) {
        if ($verificationPassed) {
            Write-Host "`nCleaning up isolated staging directory: $canonicalStage"
            Remove-Item -LiteralPath $canonicalStage -Recurse -Force -ErrorAction SilentlyContinue
        } else {
            Write-Host "`nVerification failed: retaining isolated staging directory for diagnostic evidence: $canonicalStage"
        }
    }
}

