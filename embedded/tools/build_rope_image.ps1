param(
    [ValidateSet("Comanche055", "Luminary099")]
    [string]$Program = "Comanche055",

    [string]$SourceDir = "",

    [string]$YaYulPath = "",

    [string]$OutputHeader = "",

    [string]$BuildDir = "",

    [string]$Python = "py",

    [switch]$ValidateOnly
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$EmbeddedDir = Split-Path -Parent $ScriptDir
$RepoRoot = Split-Path -Parent $EmbeddedDir

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return (Resolve-Path -LiteralPath $Path).ProviderPath
    }

    return (Resolve-Path -LiteralPath (Join-Path $RepoRoot $Path)).ProviderPath
}

function Get-DisplayPath {
    param([string]$Path)

    try {
        $relative = Resolve-Path -LiteralPath $Path -Relative -ErrorAction Stop
        return $relative
    } catch {
        return $Path
    }
}

function Find-YaYul {
    param([string]$ExplicitPath)

    $checked = New-Object System.Collections.Generic.List[string]

    if ($ExplicitPath.Trim().Length -gt 0) {
        if ([System.IO.Path]::IsPathRooted($ExplicitPath)) {
            $candidate = $ExplicitPath
        } else {
            $candidate = Join-Path $RepoRoot $ExplicitPath
        }
        $checked.Add($candidate)
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return @{ Path = (Resolve-Path -LiteralPath $candidate).ProviderPath; Checked = $checked }
        }
    }

    foreach ($commandName in @("yaYUL.exe", "yaYUL")) {
        $checked.Add("PATH:$commandName")
        $command = Get-Command $commandName -ErrorAction SilentlyContinue
        if ($null -ne $command -and $command.CommandType -eq "Application") {
            return @{ Path = $command.Source; Checked = $checked }
        }
    }

    $localCandidates = @(
        (Join-Path $RepoRoot "yaYUL.exe"),
        (Join-Path $RepoRoot "yaYUL\yaYUL.exe"),
        (Join-Path $RepoRoot "VirtualAGC\yaYUL\yaYUL.exe"),
        (Join-Path $RepoRoot "VirtualAGC\bin\yaYUL.exe"),
        (Join-Path $RepoRoot "virtualagc\yaYUL\yaYUL.exe"),
        (Join-Path $RepoRoot "embedded\tools\yaYUL.exe")
    )

    foreach ($candidate in $localCandidates) {
        $checked.Add($candidate)
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return @{ Path = (Resolve-Path -LiteralPath $candidate).ProviderPath; Checked = $checked }
        }
    }

    return @{ Path = $null; Checked = $checked }
}

function Write-YaYulNextCommand {
    param(
        [string]$ProgramName,
        [System.Collections.Generic.List[string]]$Checked
    )

    Write-Warning "No yaYUL executable was found, so assembly was not run."
    Write-Host ""
    Write-Host "Searched:"
    foreach ($item in $Checked) {
        Write-Host "  $item"
    }
    Write-Host ""
    Write-Host "Next command after placing yaYUL.exe at the repository root:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File embedded\tools\build_rope_image.ps1 -Program $ProgramName -YaYulPath .\yaYUL.exe"
    Write-Host ""
    Write-Host "If yaYUL.exe is already on PATH, use:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File embedded\tools\build_rope_image.ps1 -Program $ProgramName"
}

if ($SourceDir.Trim().Length -gt 0) {
    $SourceRoot = Resolve-RepoPath $SourceDir
} else {
    $SourceRoot = Resolve-RepoPath $Program
}

if ($OutputHeader.Trim().Length -eq 0) {
    $OutputHeader = Join-Path $EmbeddedDir "esp32_agc_core\rope_image.h"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputHeader)) {
    $OutputHeader = Join-Path $RepoRoot $OutputHeader
}

$MainSource = Join-Path $SourceRoot "MAIN.agc"
if (-not (Test-Path -LiteralPath $MainSource -PathType Leaf)) {
    throw "Expected MAIN.agc in $SourceRoot"
}

$SourceFiles = @(Get-ChildItem -LiteralPath $SourceRoot -Filter "*.agc" -File)
if ($SourceFiles.Count -eq 0) {
    throw "No .agc source files found in $SourceRoot"
}

$SourceByName = @{}
foreach ($file in $SourceFiles) {
    $SourceByName[$file.Name.ToLowerInvariant()] = $file

    $header = (Get-Content -LiteralPath $file.FullName -TotalCount 12) -join "`n"
    $declaredName = [regex]::Match($header, '(?im)^\s*#\s*Filename:\s*([^\s]+\.agc)\s*$')
    if ($declaredName.Success) {
        $SourceByName[$declaredName.Groups[1].Value.ToLowerInvariant()] = $file
    }
}

$MainText = Get-Content -LiteralPath $MainSource -Raw
$IncludeMatches = [regex]::Matches($MainText, '(?m)^\s*\$([^\s#]+\.agc)')
$MissingIncludes = New-Object System.Collections.Generic.List[string]
$AliasCopies = @{}
foreach ($match in $IncludeMatches) {
    $includeName = $match.Groups[1].Value
    $includePath = Join-Path $SourceRoot $includeName
    if (-not (Test-Path -LiteralPath $includePath -PathType Leaf)) {
        $includeKey = $includeName.ToLowerInvariant()
        if ($SourceByName.ContainsKey($includeKey)) {
            $AliasCopies[$includeName] = $SourceByName[$includeKey].FullName
        } else {
            $MissingIncludes.Add($includeName)
        }
    }
}

if ($MissingIncludes.Count -gt 0) {
    throw "MAIN.agc references missing include files: $($MissingIncludes -join ', ')"
}

Write-Host "Validated $Program source tree:"
Write-Host "  Source: $(Get-DisplayPath $SourceRoot)"
Write-Host "  MAIN.agc includes: $($IncludeMatches.Count)"
Write-Host "  .agc files present: $($SourceFiles.Count)"
if ($AliasCopies.Count -gt 0) {
    Write-Host "  include filename aliases: $($AliasCopies.Count)"
    foreach ($aliasName in ($AliasCopies.Keys | Sort-Object)) {
        Write-Host "    $aliasName <= $(Split-Path -Leaf $AliasCopies[$aliasName])"
    }
}

$YaYul = Find-YaYul $YaYulPath
if ($null -eq $YaYul.Path) {
    Write-YaYulNextCommand -ProgramName $Program -Checked $YaYul.Checked
    exit 2
}

Write-Host "yaYUL: $($YaYul.Path)"

if ($ValidateOnly) {
    Write-Host "Validation only; assembly was not run."
    Write-Host "Build command:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File embedded\tools\build_rope_image.ps1 -Program $Program -YaYulPath `"$($YaYul.Path)`""
    exit 0
}

if ($BuildDir.Trim().Length -eq 0) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "apollo11-rope-$Program-$stamp"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

foreach ($file in $SourceFiles) {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $BuildDir $file.Name) -Force
}
foreach ($aliasName in $AliasCopies.Keys) {
    Copy-Item -LiteralPath $AliasCopies[$aliasName] -Destination (Join-Path $BuildDir $aliasName) -Force
}

$ListingPath = Join-Path $BuildDir "$Program.lst"
$BinPath = Join-Path $BuildDir "MAIN.agc.bin"
$Converter = Join-Path $ScriptDir "rope_to_header.py"

Write-Host "Assembling temp copy:"
Write-Host "  Build dir: $BuildDir"
Write-Host "  Listing: $ListingPath"

Push-Location $BuildDir
try {
    & $YaYul.Path "MAIN.agc" > $ListingPath
    $YaYulExit = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($YaYulExit -ne 0) {
    throw "yaYUL failed with exit code $YaYulExit. See $ListingPath"
}

if (-not (Test-Path -LiteralPath $BinPath -PathType Leaf)) {
    throw "yaYUL completed but did not produce $BinPath"
}

$PythonCommand = Get-Command $Python -ErrorAction SilentlyContinue
if ($null -eq $PythonCommand) {
    Write-Host "yaYUL produced $BinPath, but Python command '$Python' was not found."
    Write-Host "Next conversion command after installing Python or choosing -Python:"
    Write-Host "  $Python embedded\tools\rope_to_header.py `"$BinPath`" `"$OutputHeader`" --format yayul --name `"$Program`""
    exit 3
}

Write-Host "Converting yaYUL binary to ESP32 header:"
Write-Host "  Output: $OutputHeader"
& $Python $Converter $BinPath $OutputHeader --format yayul --name $Program
$PythonExit = $LASTEXITCODE
if ($PythonExit -ne 0) {
    throw "rope_to_header.py failed with exit code $PythonExit"
}

Write-Host "Rope header generated."
