$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$fixture = Join-Path $repo ('.codex-tmp/export-review-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path "$fixture/tools", "$fixture/docs", "$fixture/temp" | Out-Null
$fixture = (Resolve-Path -LiteralPath $fixture).Path
# Copy only the exporter inputs; never test collisions against a user export.
Copy-Item -LiteralPath "$repo/tools/export_mcu.bat" -Destination "$fixture/tools/export_mcu.bat"
$files = 'lora_comm.h','lora_hub_link.h','LORA_PROTOCOL.md','ui.c','ui.h','ui_auto.h','ui_auto.c','ui_internal.h','ui_mcu_profile.h','ui_types.h','ui_theme.h','ui_theme.c','ui_icons.h','ui_icons.c','ui_fonts.h','ui_fonts.c','ui_splash_logo.c'
foreach ($f in $files) { Copy-Item -LiteralPath (Join-Path $repo $f) -Destination (Join-Path $fixture $f) }
foreach ($f in @('HARDWARE_OVERVIEW.md','MCU_BASELINE.md','README.md','UI_DESIGN_BRIEF.md')) {
    Copy-Item -LiteralPath "$repo/docs/$f" -Destination "$fixture/docs/$f"
}
$priorTemp = $env:TEMP
$priorTs = $env:TS
try {
    $env:TEMP = (Resolve-Path "$fixture/temp").Path
    Remove-Item Env:TS -ErrorAction SilentlyContinue
    $script = Join-Path $fixture 'tools\export_mcu.bat'
    $sentinel = Join-Path $fixture 'existing.zip'
    Copy-Item -LiteralPath "$PSScriptRoot/sentinel.txt" -Destination $sentinel
    $before = (Get-FileHash -LiteralPath $sentinel).Hash
    & $script (Join-Path $fixture 'dest') $sentinel
    $explicitCode = $LASTEXITCODE
    $after = (Get-FileHash -LiteralPath $sentinel).Hash
    Write-Output "EXPLICIT existing destination: exit=$explicitCode sentinel_preserved=$($before -eq $after)"
    & $script
    $firstCode = $LASTEXITCODE
    $first = @(Get-ChildItem -LiteralPath $fixture -Filter 'smart_hub_ui*.zip')
    Write-Output "DEFAULT first: exit=$firstCode names=$($first.Name -join ',')"
    if ($first.Count -eq 1) {
        Copy-Item -LiteralPath "$PSScriptRoot/sentinel.txt" -Destination $first[0].FullName -Force
        $sentinelDefaultHash = (Get-FileHash -LiteralPath $first[0].FullName).Hash
        & $script
        $secondCode = $LASTEXITCODE
        $defaultPreserved = $sentinelDefaultHash -eq (Get-FileHash -LiteralPath $first[0].FullName).Hash
        $second = @(Get-ChildItem -LiteralPath $fixture -Filter 'smart_hub_ui*.zip')
        Write-Output "DEFAULT second: exit=$secondCode original_preserved=$defaultPreserved names=$($second.Name -join ',')"
    }
    Write-Output "Retained disposable evidence: $fixture"
} finally {
    $env:TEMP = $priorTemp
    $env:TS = $priorTs
}
