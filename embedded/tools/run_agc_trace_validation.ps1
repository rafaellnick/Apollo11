param(
  [string]$Compiler = "",
  [string]$Output = "embedded\tests\agc_trace_runner.exe",
  [string]$CandidateTrace = "embedded\tests\agc_trace_candidate.csv",
  [string]$ReferenceTrace = "",
  [ValidateSet("Synthetic", "Rope")]
  [string]$Mode = "Synthetic",
  [int]$Steps = 0,
  [switch]$HardwareTiming,
  [switch]$AllowRunnerFailure,
  [switch]$NoRun
)

$ErrorActionPreference = "Stop"

function Find-Compiler {
  param([string]$RequestedCompiler)

  if ($RequestedCompiler -ne "") {
    $command = Get-Command $RequestedCompiler -ErrorAction SilentlyContinue
    if ($null -eq $command) {
      Write-Error "Compiler '$RequestedCompiler' was not found on PATH."
    }
    return $command.Source
  }

  foreach ($candidate in @("g++", "clang++", "cl")) {
    $command = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($null -ne $command) {
      return $command.Source
    }
  }

  if ($env:LOCALAPPDATA) {
    $wingetPackages = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    if (Test-Path -LiteralPath $wingetPackages -PathType Container) {
      $winlibsCompilers = Get-ChildItem -LiteralPath $wingetPackages -Directory -Filter "BrechtSanders.WinLibs*" -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName "mingw64\bin\g++.exe" } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }

      foreach ($compiler in $winlibsCompilers) {
        return (Resolve-Path -LiteralPath $compiler).ProviderPath
      }
    }
  }

  Write-Error "No C++ compiler found. Install g++, clang++, or Visual Studio Build Tools, then run this script again."
}

function Invoke-TraceBuild {
  param(
    [string]$CompilerPath,
    [string]$OutputPath
  )

  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null

  if ((Split-Path -Leaf $CompilerPath) -ieq "cl.exe") {
    & $CompilerPath /nologo /std:c++17 /EHsc /I embedded\shared `
      /I embedded\esp32_agc_core `
      /Fe:$OutputPath embedded\tests\agc_trace_runner.cpp
  } else {
    & $CompilerPath -std=c++17 -Wall -Wextra -pedantic `
      -I embedded\shared -I embedded\esp32_agc_core `
      embedded\tests\agc_trace_runner.cpp -o $OutputPath
  }

  if ($LASTEXITCODE -ne 0) {
    Write-Error "Trace runner build failed with exit code $LASTEXITCODE."
  }
}

$compilerPath = Find-Compiler $Compiler
$compilerDir = Split-Path -Parent $compilerPath
if ($compilerDir -ne "" -and
    (($env:PATH -split [System.IO.Path]::PathSeparator) -notcontains $compilerDir)) {
  $env:PATH = "$compilerDir$([System.IO.Path]::PathSeparator)$env:PATH"
}
Write-Host "Using compiler: $compilerPath"
Invoke-TraceBuild -CompilerPath $compilerPath -OutputPath $Output
Write-Host "Built trace runner: $Output"

if ($NoRun) {
  exit 0
}

$traceArgs = @("--mode", $Mode.ToLowerInvariant())
if ($Steps -gt 0) {
  $traceArgs += @("--steps", "$Steps")
}
if ($HardwareTiming) {
  $traceArgs += "--hardware-timing"
}

& $Output @traceArgs > $CandidateTrace
$runnerExit = $LASTEXITCODE
if ($runnerExit -ne 0 -and -not $AllowRunnerFailure) {
  Write-Error "Trace runner failed with exit code $LASTEXITCODE."
} elseif ($runnerExit -ne 0) {
  Write-Warning "Trace runner exited with code $runnerExit after writing the partial trace."
}

Write-Host "Wrote candidate trace: $CandidateTrace"

if ($ReferenceTrace -ne "") {
  powershell -ExecutionPolicy Bypass -File embedded\tools\compare_agc_trace.ps1 `
    -CandidateTrace $CandidateTrace -ReferenceTrace $ReferenceTrace
}
