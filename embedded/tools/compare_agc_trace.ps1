param(
  [Parameter(Mandatory = $true)]
  [string]$CandidateTrace,

  [Parameter(Mandatory = $true)]
  [string]$ReferenceTrace,

  [string[]]$Columns = @(
    "step",
    "pc",
    "instr",
    "extended",
    "a",
    "l",
    "q",
    "z",
    "eb",
    "fb",
    "bb",
    "cyc",
    "mct",
    "irq",
    "ch010",
    "ch015"
  ),

  [int]$MaxMismatches = 20,

  [switch]$AllowMismatch
)

$ErrorActionPreference = "Stop"

function Resolve-TracePath {
  param([string]$Path)
  $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
  return $resolved.Path
}

$candidatePath = Resolve-TracePath $CandidateTrace
$referencePath = Resolve-TracePath $ReferenceTrace

$candidateRows = Import-Csv -LiteralPath $candidatePath
$referenceRows = Import-Csv -LiteralPath $referencePath

$rowCountMismatch = $false
if ($candidateRows.Count -ne $referenceRows.Count) {
  $rowCountMismatch = $true
  $message = "Trace row count mismatch: candidate=$($candidateRows.Count), reference=$($referenceRows.Count)"
  if ($AllowMismatch) {
    Write-Warning $message
  } else {
    Write-Error $message
  }
}

if ($candidateRows.Count -eq 0 -or $referenceRows.Count -eq 0) {
  Write-Error "Trace files are empty."
}

$availableColumns = $candidateRows[0].PSObject.Properties.Name
foreach ($column in $Columns) {
  if ($availableColumns -notcontains $column) {
    Write-Error "Candidate trace is missing column '$column'."
  }
  if ($referenceRows[0].PSObject.Properties.Name -notcontains $column) {
    Write-Error "Reference trace is missing column '$column'."
  }
}

$mismatches = 0
$rowLimit = [math]::Min($candidateRows.Count, $referenceRows.Count)
$stoppedAfterMax = $false
for ($rowIndex = 0; $rowIndex -lt $rowLimit; $rowIndex++) {
  foreach ($column in $Columns) {
    $candidateValue = $candidateRows[$rowIndex].$column
    $referenceValue = $referenceRows[$rowIndex].$column
    if ($candidateValue -eq $referenceValue) {
      continue
    }

    $mismatches++
    Write-Host ("Mismatch row {0}, column {1}: candidate={2}, reference={3}" -f `
      ($rowIndex + 1), $column, $candidateValue, $referenceValue)

    if ($mismatches -ge $MaxMismatches) {
      $stoppedAfterMax = $true
      break
    }
  }

  if ($stoppedAfterMax) {
    break
  }
}

if ($mismatches -gt 0) {
  $message = "Trace comparison failed with $mismatches mismatches."
  if ($stoppedAfterMax) {
    $message = "Stopping after $mismatches mismatches."
  }

  if ($AllowMismatch) {
    Write-Warning $message
    exit 0
  }

  Write-Error $message
}

if ($rowCountMismatch -and $AllowMismatch) {
  Write-Warning "Trace comparison finished with a row count mismatch."
  exit 0
}

Write-Host "Trace comparison passed: $($candidateRows.Count) rows, $($Columns.Count) columns."
