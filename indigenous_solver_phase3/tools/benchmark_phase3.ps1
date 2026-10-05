param(
  [string]$ModelsPath = "benchmarks\netlib",
  [string]$Phase2Exe = "indigenous_solver_phase2\build\Release\solver_phase2_cli.exe",
  [string]$Phase3Exe = "indigenous_solver_phase3\build\Release\solver_phase3_cli.exe",
  [string]$OutputCsv = "benchmarks\netlib\phase3_comparison.csv"
)

$models = @("afiro","adlittle","blend","bore3d","brandy","grow15","kb2","lotfi","sc50b","share1b")

function Invoke-Solver($exe, $model) {
  if (!(Test-Path $exe)) { throw "Executable not found: $exe" }
  $modelPath = Join-Path $ModelsPath ($model + ".mps")
  return ((& $exe solve $modelPath 2>&1) -join [Environment]::NewLine)
}

$rows = @()
foreach ($model in $models) {
  Write-Host "===== $model ====="
  $p2 = Invoke-Solver $Phase2Exe $model
  $p3 = Invoke-Solver $Phase3Exe $model
  $p2Obj = [double]([regex]::Match($p2,"(?m)^Objective:\s*(.+)$").Groups[1].Value)
  $p3Obj = [double]([regex]::Match($p3,"(?m)^Objective:\s*(.+)$").Groups[1].Value)
  $p2Time = [double]([regex]::Match($p2,"(?m)^Timing total_ms:\s*(.+)$").Groups[1].Value)
  $p3Time = [double]([regex]::Match($p3,"(?m)^Timing total_ms:\s*(.+)$").Groups[1].Value)
  $p2Pricing = [double]([regex]::Match($p2,"(?m)^Timing pricing_ms:\s*(.+)$").Groups[1].Value)
  $p3Pricing = [double]([regex]::Match($p3,"(?m)^Timing pricing_ms:\s*(.+)$").Groups[1].Value)
  $p2Iter = [int]([regex]::Match($p2,"(?m)^Iterations:\s*(.+)$").Groups[1].Value)
  $p3Iter = [int]([regex]::Match($p3,"(?m)^Iterations:\s*(.+)$").Groups[1].Value)
  $p2Cert = [regex]::Match($p2,"(?m)^Certificate:\s*(.+)$").Groups[1].Value
  $p3Cert = [regex]::Match($p3,"(?m)^Certificate:\s*(.+)$").Groups[1].Value
  $speedup = if ($p3Time -gt 0) { $p2Time / $p3Time } else { 0 }
  $objDiff = [math]::Abs($p2Obj - $p3Obj)
  $rows += [pscustomobject]@{ model=$model; phase2_ms=$p2Time; phase3_ms=$p3Time; phase3_speedup=[math]::Round($speedup,4); phase2_pricing_ms=$p2Pricing; phase3_pricing_ms=$p3Pricing; phase2_iterations=$p2Iter; phase3_iterations=$p3Iter; objective_difference=$objDiff; phase2_certificate=$p2Cert; phase3_certificate=$p3Cert }
  Write-Host ("Phase2: {0:N4} ms | Phase3: {1:N4} ms | Speedup: {2:N2}x | Cert: {3}" -f $p2Time,$p3Time,$speedup,$p3Cert)
}

$parent = Split-Path $OutputCsv -Parent
if ($parent -and !(Test-Path $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
$rows | Export-Csv -Path $OutputCsv -NoTypeInformation
Write-Host ""
Write-Host "CSV: $OutputCsv"
