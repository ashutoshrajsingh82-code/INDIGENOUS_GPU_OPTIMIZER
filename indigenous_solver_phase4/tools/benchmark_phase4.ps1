param(
  [string]$ModelsPath = "benchmarks\netlib",
  [string]$Phase2Exe = "indigenous_solver_phase2\build\Release\solver_phase2_cli.exe",
  [string]$Phase3Exe = "indigenous_solver_phase3\build\Release\solver_phase3_cli.exe",
  [string]$Phase4Exe = "",
  [string]$OutputCsv = "benchmarks\netlib\phase4_comparison.csv",
  [string]$OutputSummaryCsv = "benchmarks\netlib\phase4_summary.csv",
  [string]$OutputReport = "benchmarks\netlib\phase4_benchmark_report.md"
)

$models = @("afiro","adlittle","blend","bore3d","brandy","grow15","kb2","lotfi","sc50b","share1b")

function Invoke-Solver($exe, $model) {
  if (!(Test-Path $exe)) { throw "Executable not found: $exe" }
  $modelPath = Join-Path $ModelsPath ($model + ".mps")
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $text = ((& $exe solve $modelPath 2>&1) -join [Environment]::NewLine)
  $sw.Stop()
  [pscustomobject]@{ Text=$text; WallMs=$sw.Elapsed.TotalMilliseconds }
}
function Get-Metric([string]$text,[string]$pattern,[scriptblock]$convert) {
  $m=[regex]::Match($text,$pattern); if(!$m.Success){throw "Metric not found: $pattern"}; return & $convert $m.Groups[1].Value.Trim()
}
function Get-Optional([string]$text,[string]$pattern,[double]$default=0.0) {
  $m=[regex]::Match($text,$pattern); if(!$m.Success){return $default}; return [double]$m.Groups[1].Value.Trim()
}
function Get-Backend([string]$text) {
  $m=[regex]::Match($text,"(?m)^Pricing backend:\s*(.+)$")
  if($m.Success){return $m.Groups[1].Value.Trim()}
  $m=[regex]::Match($text,"(?m)^Backend:\s*(.+)$")
  if($m.Success){return $m.Groups[1].Value.Trim()}
  return "UNKNOWN"
}

$hasPhase4 = [bool]($Phase4Exe -and (Test-Path $Phase4Exe))
$rows=@()
foreach($model in $models){
  Write-Host "===== $model ====="
  $p2=Invoke-Solver $Phase2Exe $model
  $p3=Invoke-Solver $Phase3Exe $model
  $p2Obj=Get-Metric $p2.Text "(?m)^Objective:\s*(.+)$" {[double]$args[0]}
  $p3Obj=Get-Metric $p3.Text "(?m)^Objective:\s*(.+)$" {[double]$args[0]}
  $p2Time=Get-Metric $p2.Text "(?m)^Timing total_ms:\s*(.+)$" {[double]$args[0]}
  $p3Time=Get-Metric $p3.Text "(?m)^Timing total_ms:\s*(.+)$" {[double]$args[0]}
  $p2Iter=Get-Metric $p2.Text "(?m)^Iterations:\s*(.+)$" {[int]$args[0]}
  $p3Iter=Get-Metric $p3.Text "(?m)^Iterations:\s*(.+)$" {[int]$args[0]}
  $p2Cert=Get-Metric $p2.Text "(?m)^Certificate:\s*(.+)$" {$args[0]}
  $p3Cert=Get-Metric $p3.Text "(?m)^Certificate:\s*(.+)$" {$args[0]}
  $p3Backend=Get-Backend $p3.Text
  $p3Pricing=Get-Optional $p3.Text "(?m)^Timing pricing_ms:\s*(.+)$"
  $p3Basis=Get-Optional $p3.Text "(?m)^Timing basis_ms:\s*(.+)$"
  $objDiff=[math]::Abs($p2Obj-$p3Obj)
  $rows += [pscustomobject]@{
    model=$model; phase2_ms=$p2Time; phase3_ms=$p3Time
    phase3_speedup=($(if($p3Time -gt 0){$p2Time/$p3Time}else{0}))
    phase2_wall_ms=$p2.WallMs; phase3_wall_ms=$p3.WallMs
    phase2_iterations=$p2Iter; phase3_iterations=$p3Iter
    phase3_pricing_ms=$p3Pricing; phase3_basis_ms=$p3Basis
    phase3_backend=$p3Backend; objective_difference=$objDiff
    phase2_certificate=$p2Cert; phase3_certificate=$p3Cert
  }
  Write-Host ("P2 {0:N4} ms | P3 {1:N4} ms | Speedup {2:N2}x | Backend {3} | Cert {4}" -f $p2Time,$p3Time,$rows[-1].phase3_speedup,$p3Backend,$p3Cert)
}

$parent=Split-Path $OutputCsv -Parent
if($parent -and !(Test-Path $parent)){New-Item -ItemType Directory -Path $parent -Force|Out-Null}
$rows|Export-Csv $OutputCsv -NoTypeInformation
$total2=($rows|Measure-Object phase2_ms -Sum).Sum
$total3=($rows|Measure-Object phase3_ms -Sum).Sum
$sum=[pscustomobject]@{
  models=$rows.Count; phase2_total_ms=[math]::Round($total2,4); phase3_total_ms=[math]::Round($total3,4)
  aggregate_speedup=[math]::Round($(if($total3 -gt 0){$total2/$total3}else{0}),4)
  phase3_certificates_pass=@($rows|?{$_.phase3_certificate -eq "PASS"}).Count
  max_objective_difference=[math]::Round(($rows|Measure-Object objective_difference -Maximum).Maximum,12)
  gpu_backend_models=@($rows|?{$_.phase3_backend -match "CUDA|GPU"}).Count
  cpu_backend_models=@($rows|?{$_.phase3_backend -match "CPU"}).Count
}
$sum|Export-Csv $OutputSummaryCsv -NoTypeInformation

$report=@("# Phase 4.12 Backend and Benchmark Report","","This Phase 4.12 report combines the existing Phase 2/3 benchmark data with explicit backend capability reporting. The Phase 4 basis layer remains a validated backend facade and is not yet a full replacement for the Phase 2 revised simplex solver. This benchmark runs the existing Phase 2 and Phase 3 solver CLIs on the standard Netlib model set. Phase 4 is represented by its validated basis backend status; the Phase 4 basis library is not yet a full replacement for the Phase 2 revised simplex solver.","","## Summary","","| Metric | Result |","|---|---:|")
$report+="| Models | $($sum.models) |"
$report+="| Phase 2 total | $($sum.phase2_total_ms) ms |"
$report+="| Phase 3 total | $($sum.phase3_total_ms) ms |"
$report+="| Phase 3 aggregate speedup | $($sum.aggregate_speedup)x |"
$report+="| Phase 3 certificates PASS | $($sum.phase3_certificates_pass)/$($sum.models) |"
$report+="| CPU backend models | $($sum.cpu_backend_models) |"
$report+="| CUDA/GPU backend models | $($sum.gpu_backend_models) |"
$report+="| Maximum objective difference | $($sum.max_objective_difference) |"
$report+=""
$report+="## Per-model results",""
$report+="| Model | P2 ms | P3 ms | Speedup | P3 backend | P3 certificate | Objective diff |"
$report+="|---|---:|---:|---:|---|---|---:|"
foreach($r in $rows){$report+="| $($r.model) | $([math]::Round($r.phase2_ms,4)) | $([math]::Round($r.phase3_ms,4)) | $([math]::Round($r.phase3_speedup,4))x | $($r.phase3_backend) | $($r.phase3_certificate) | $([math]::Round($r.objective_difference,12)) |"}
$report+="","## Backend reporting",""
$report+="- Phase 4.12 backend reporting separates CUDA build capability from active execution.\n- Phase 4.10 numerical validation is a separate CUDA-capable-machine test and is not silently converted into a CPU result."
$report+="- On the current Intel-only machine, Phase 3 backend measurements are CPU fallback measurements, not GPU acceleration."
$report+="- Phase 4 basis functionality is validated by the Phase 4 CTest suite; full revised-simplex replacement remains a later integration task."
$report|Set-Content $OutputReport -Encoding UTF8

Write-Host ""
Write-Host "=== Phase 4.11 Summary ==="
Write-Host ("Models: {0} | Certificates PASS: {1}/{0} | CPU backend: {2} | GPU backend: {3}" -f $sum.models,$sum.phase3_certificates_pass,$sum.cpu_backend_models,$sum.gpu_backend_models)
Write-Host ("P2 total: {0:N4} ms | P3 total: {1:N4} ms | Speedup: {2:N2}x" -f $total2,$total3,$sum.aggregate_speedup)
Write-Host "CSV: $OutputCsv"
Write-Host "Summary: $OutputSummaryCsv"
Write-Host "Report: $OutputReport"
