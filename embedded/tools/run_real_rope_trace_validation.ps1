param(
  [ValidateSet("Comanche055", "Luminary099")]
  [string]$Program = "Comanche055",

  [int]$Steps = 64,

  [string]$YaYulPath = "",
  [string]$Python = "py",
  [string]$VirtualAgcRoot = "",
  [string]$YaAgcDir = "",
  [string]$Bash = "",
  [string]$BuildDir = "",

  [string]$CandidateTrace = "embedded\tests\agc_trace_real_candidate.csv",
  [string]$ReferenceTrace = "embedded\tests\agc_trace_real_yaagc.csv",

  [switch]$CpuOnly
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$EmbeddedDir = Split-Path -Parent $ScriptDir
$RepoRoot = Split-Path -Parent $EmbeddedDir

function Resolve-OutputPath {
  param([string]$Path)

  if ([System.IO.Path]::IsPathRooted($Path)) {
    return $Path
  }

  return (Join-Path $RepoRoot $Path)
}

function Invoke-PowershellScript {
  param([string[]]$Arguments)

  & powershell @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed with exit code $LASTEXITCODE`: powershell $($Arguments -join ' ')"
  }
}

if ($Steps -le 0) {
  throw "-Steps must be greater than zero."
}

if ($Program -ne "Comanche055") {
  Write-Warning "The embedded candidate runner uses embedded\esp32_agc_core\rope_image.h. Rebuild that header for $Program before trusting the candidate trace."
}

if ($CpuOnly) {
  if ($CandidateTrace -eq "embedded\tests\agc_trace_real_candidate.csv") {
    $CandidateTrace = "embedded\tests\agc_trace_real_cpu_candidate.csv"
  }
  if ($ReferenceTrace -eq "embedded\tests\agc_trace_real_yaagc.csv") {
    $ReferenceTrace = "embedded\tests\agc_trace_real_cpu_yaagc.csv"
  }
}

if ($BuildDir.Trim().Length -eq 0) {
  $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
  $suffix = [guid]::NewGuid().ToString("N").Substring(0, 8)
  $BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "apollo11-real-rope-trace-$stamp-$suffix"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
  $BuildDir = Join-Path $RepoRoot $BuildDir
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$tempHeader = Join-Path $BuildDir "rope_image.h"
$tempManifest = Join-Path $BuildDir "rope_image.manifest.txt"
$ropeBuildDir = Join-Path $BuildDir "rope-build"

$buildArgs = @(
  "-ExecutionPolicy", "Bypass",
  "-File", (Join-Path $ScriptDir "build_rope_image.ps1"),
  "-Program", $Program,
  "-OutputHeader", $tempHeader,
  "-OutputManifest", $tempManifest,
  "-BuildDir", $ropeBuildDir,
  "-Python", $Python
)
if ($YaYulPath.Trim().Length -gt 0) {
  $buildArgs += @("-YaYulPath", $YaYulPath)
}

Write-Host "Building temporary yaYUL rope image for yaAGC reference..."
Invoke-PowershellScript -Arguments $buildArgs

$manifest = Get-Content -LiteralPath $tempManifest
$romImage = ($manifest | Where-Object { $_ -like "bin=*" } | Select-Object -First 1)
if ($null -eq $romImage) {
  throw "Could not find bin= entry in $tempManifest"
}
$romImage = $romImage.Substring(4)
if (-not (Test-Path -LiteralPath $romImage -PathType Leaf)) {
  throw "yaYUL binary was not found: $romImage"
}

$referenceOut = Resolve-OutputPath $ReferenceTrace
$candidateOut = Resolve-OutputPath $CandidateTrace

$yaagcArgs = @(
  "-ExecutionPolicy", "Bypass",
  "-File", (Join-Path $ScriptDir "run_yaagc_reference_trace.ps1"),
  "-Output", $referenceOut,
  "-Steps", "$Steps",
  "-RomImage", $romImage
)
if ($VirtualAgcRoot.Trim().Length -gt 0) {
  $yaagcArgs += @("-VirtualAgcRoot", $VirtualAgcRoot)
}
if ($YaAgcDir.Trim().Length -gt 0) {
  $yaagcArgs += @("-YaAgcDir", $YaAgcDir)
}
if ($Bash.Trim().Length -gt 0) {
  $yaagcArgs += @("-Bash", $Bash)
}
if ($CpuOnly) {
  $yaagcArgs += "-CpuOnly"
}

if ($CpuOnly) {
  Write-Host "Generating yaAGC real-rope CPU-only reference trace..."
} else {
  Write-Host "Generating yaAGC real-rope reference trace..."
}
Invoke-PowershellScript -Arguments $yaagcArgs

$candidateArgs = @(
  "-ExecutionPolicy", "Bypass",
  "-File", (Join-Path $ScriptDir "run_agc_trace_validation.ps1"),
  "-Mode", "Rope",
  "-Steps", "$Steps",
  "-CandidateTrace", $candidateOut,
  "-AllowRunnerFailure"
)
if (-not $CpuOnly) {
  $candidateArgs += "-HardwareTiming"
}

Write-Host "Generating embedded-core real-rope candidate trace..."
Invoke-PowershellScript -Arguments $candidateArgs

Write-Host "Comparing traces and reporting the first mismatches..."
Invoke-PowershellScript -Arguments @(
  "-ExecutionPolicy", "Bypass",
  "-File", (Join-Path $ScriptDir "compare_agc_trace.ps1"),
  "-CandidateTrace", $candidateOut,
  "-ReferenceTrace", $referenceOut,
  "-MaxMismatches", "20",
  "-AllowMismatch"
)

Write-Host ""
Write-Host "Real-rope trace run complete."
Write-Host "  Candidate: $candidateOut"
Write-Host "  Reference: $referenceOut"
Write-Host "  Build dir:  $BuildDir"
