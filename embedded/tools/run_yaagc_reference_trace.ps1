param(
  [string]$VirtualAgcRoot = "",
  [string]$YaAgcDir = "",
  [string]$Bash = "",
  [string]$Output = "embedded\tests\agc_trace_reference_yaagc.csv",
  [string]$BuildDir = "",
  [int]$Steps = 18,
  [string]$RomImage = "",
  [switch]$CpuOnly,
  [switch]$NoRun
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$EmbeddedDir = Split-Path -Parent $ScriptDir
$RepoRoot = Split-Path -Parent $EmbeddedDir

function Resolve-LocalPath {
  param([string]$Path)

  if ([System.IO.Path]::IsPathRooted($Path)) {
    return (Resolve-Path -LiteralPath $Path -ErrorAction Stop).ProviderPath
  }

  return (Resolve-Path -LiteralPath (Join-Path $RepoRoot $Path) -ErrorAction Stop).ProviderPath
}

function ConvertTo-MsysPath {
  param([string]$Path)

  $full = [System.IO.Path]::GetFullPath($Path)
  $drive = $full.Substring(0, 1).ToLowerInvariant()
  $rest = $full.Substring(2).Replace("\", "/")
  return "/$drive$rest"
}

function Quote-Bash {
  param([string]$Value)

  return "'" + ($Value -replace "'", "'""'""'") + "'"
}

function Find-Bash {
  param([string]$Requested)

  if ($Requested.Trim().Length -gt 0) {
    return Resolve-LocalPath $Requested
  }

  $default = "C:\msys64\usr\bin\bash.exe"
  if (Test-Path -LiteralPath $default -PathType Leaf) {
    return $default
  }

  $command = Get-Command bash.exe -ErrorAction SilentlyContinue
  if ($null -ne $command) {
    return $command.Source
  }

  throw "MSYS2 bash was not found. Install MSYS2, then rerun this script."
}

function Find-YaAgcDirectory {
  if ($YaAgcDir.Trim().Length -gt 0) {
    return Resolve-LocalPath $YaAgcDir
  }

  $candidates = New-Object System.Collections.Generic.List[string]

  if ($VirtualAgcRoot.Trim().Length -gt 0) {
    $root = Resolve-LocalPath $VirtualAgcRoot
    $candidates.Add((Join-Path $root "yaAGC"))
  }

  $candidates.Add((Join-Path (Split-Path -Parent $RepoRoot) "VirtualAGC\yaAGC"))
  $candidates.Add((Join-Path $RepoRoot "VirtualAGC\yaAGC"))

  foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath (Join-Path $candidate "agc_engine.h") -PathType Leaf) {
      return (Resolve-Path -LiteralPath $candidate).ProviderPath
    }
  }

  throw "Could not find VirtualAGC\yaAGC. Pass -VirtualAgcRoot or -YaAgcDir."
}

$bashPath = Find-Bash $Bash
$yaAgcPath = Find-YaAgcDirectory
$driverPath = Resolve-LocalPath "embedded\tests\yaagc_reference_trace_driver.c"

if ($BuildDir.Trim().Length -eq 0) {
  $BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "apollo11-yaagc-trace"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
  $BuildDir = Join-Path $RepoRoot $BuildDir
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

if (-not [System.IO.Path]::IsPathRooted($Output)) {
  $Output = Join-Path $RepoRoot $Output
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Output) | Out-Null

$romImagePath = ""
if ($RomImage.Trim().Length -gt 0) {
  $romImagePath = Resolve-LocalPath $RomImage
}

$libPath = Join-Path $yaAgcPath "libyaAGC.a"
if (-not (Test-Path -LiteralPath $libPath -PathType Leaf)) {
  Write-Host "libyaAGC.a was not found; building yaAGC first."
  $makeCommand = "cd $(Quote-Bash (ConvertTo-MsysPath $yaAgcPath)) && make cc=gcc"
  & $bashPath -lc $makeCommand
  if ($LASTEXITCODE -ne 0) {
    throw "yaAGC build failed with exit code $LASTEXITCODE."
  }
}

$exePath = Join-Path $BuildDir "yaagc_reference_trace_driver.exe"
$compileCommand = @(
  "gcc",
  "-std=c99",
  "-Wall",
  "-Wextra",
  "-I", (Quote-Bash (ConvertTo-MsysPath $yaAgcPath)),
  (Quote-Bash (ConvertTo-MsysPath $driverPath)),
  (Quote-Bash (ConvertTo-MsysPath $libPath)),
  "-lm",
  "-pthread",
  "-o", (Quote-Bash (ConvertTo-MsysPath $exePath))
) -join " "

Write-Host "Compiling yaAGC reference trace driver:"
Write-Host "  yaAGC: $yaAgcPath"
Write-Host "  output: $exePath"
& $bashPath -lc $compileCommand
if ($LASTEXITCODE -ne 0) {
  throw "Reference trace driver build failed with exit code $LASTEXITCODE."
}

if ($NoRun) {
  exit 0
}

$runParts = @(
  (Quote-Bash (ConvertTo-MsysPath $exePath)),
  "--steps",
  "$Steps"
)
if ($romImagePath.Trim().Length -gt 0) {
  $runParts += @("--rom", (Quote-Bash (ConvertTo-MsysPath $romImagePath)))
}
if ($CpuOnly) {
  $runParts += "--cpu-only"
}

$runCommand = ($runParts -join " ") + " > " + (Quote-Bash (ConvertTo-MsysPath $Output))
& $bashPath -lc $runCommand
if ($LASTEXITCODE -ne 0) {
  throw "Reference trace driver failed with exit code $LASTEXITCODE."
}

Write-Host "Wrote yaAGC reference trace: $Output"
