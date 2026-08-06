[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Elf,
    [string]$SizeTool = 'arm-none-eabi-size',
    [Int64]$RamBytes = 128KB,
    [Int64]$FlashBytes = 1MB
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$elfPath = (Resolve-Path -LiteralPath $Elf -ErrorAction Stop).Path
$sizeCommand = (Get-Command $SizeTool -ErrorAction Stop).Source
$output = @(& $sizeCommand $elfPath)
if ($LASTEXITCODE -ne 0) { throw "$SizeTool failed ($LASTEXITCODE)" }

$values = $null
foreach ($line in $output) {
    if ($line -match '^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([0-9a-fA-Fx]+)\s+') {
        $values = @([Int64]$Matches[1], [Int64]$Matches[2], [Int64]$Matches[3])
        break
    }
}
if ($null -eq $values) {
    $output | ForEach-Object { Write-Host $_ }
    throw 'Could not parse Berkeley size output (text data bss dec hex filename).'
}

$text = $values[0]
$data = $values[1]
$bss = $values[2]
$flashUsed = $text + $data
$ramUsed = $data + $bss

$report = [pscustomobject]@{
    ELF             = $elfPath
    TextBytes       = $text
    DataBytes       = $data
    BssBytes        = $bss
    FlashUsedBytes  = $flashUsed
    FlashTotalBytes = $FlashBytes
    FlashPercent    = [Math]::Round(100.0 * $flashUsed / $FlashBytes, 2)
    RamUsedBytes    = $ramUsed
    RamTotalBytes   = $RamBytes
    RamPercent      = [Math]::Round(100.0 * $ramUsed / $RamBytes, 2)
    RamHeadroom     = $RamBytes - $ramUsed
}
$report | Format-List

Write-Warning 'Static size does not include worst-case stack usage. Measure stack high-water on the MCU.'
if ($flashUsed -gt $FlashBytes -or $ramUsed -gt $RamBytes) { exit 2 }
