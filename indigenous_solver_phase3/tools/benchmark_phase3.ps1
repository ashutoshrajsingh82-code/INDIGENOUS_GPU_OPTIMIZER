param(
  [string]$ModelsPath = "benchmarks\netlib",
  [string]$Phase2Exe = "indigenous_solver_phase2\build\Release\solver_phase2_cli.exe",
  [string]$Phase3Exe = "indigenous_solver_phase3\build\Release\solver_phase3_cli.exe",
  [string]$OutputCsv = "benchmarks\netlib\phase3_comparison.csv",
  [string]$OutputSummaryCsv = "benchmarks\netlib\phase3_summary.csv",
  [string]$OutputReport = "benchmarks\netlib\phase3_benchmark_report.md",
  [switch]$RequireCuda
)

$models = @("afiro","adlittle","blend","bore3d","brandy","grow15","kb2","lotfi","sc50b","share1b")

function Invoke-Solver($exe, $model) {
  if (!(Test-Path $exe)) { throw "Executable not found: $exe" }
  $modelPath = Join-Path $ModelsPath ($model + ".mps")
  return ((& $exe solve $modelPath 2>&1) -join [Environment]::NewLine)
}

function Get-Metric([string]$text, [string]$pattern, [scriptblock]$convert) {
  $match = [regex]::Match($text, $pattern)
  if (!$match.Success) { throw "Metric not found: $pattern" }
  return & $convert $match.Groups[1].Value.Trim()
}

function Get-OptionalMetric([string]$text, [string]$pattern) {
  $match = [regex]::Match($text, $pattern)
  if (!$match.Success) { return $null }
  return [double]$match.Groups[1].Value.Trim()
}

function Get-OptionalText([string]$text, [string]$pattern) {
  $match = [regex]::Match($text, $pattern)
  if (!$match.Success) { return "N/A" }
  return $match.Groups[1].Value.Trim()
}

function Format-Metric($value) {
  if ($null -eq $value) { return "N/A" }
  return ("{0:N4}" -f [double]$value)
}

if ($RequireCuda) {
  if (!(Test-Path $Phase3Exe)) { throw "CUDA was required, but Phase 3 executable was not found: $Phase3Exe" }
  Write-Host "CUDA required: YES"
  Write-Host "Performing CUDA backend preflight using afiro..."
  $preflight = Invoke-Solver $Phase3Exe "afiro"
  $preflightBackend = Get-OptionalText $preflight "(?m)^Pricing backend:\s*(.+)$"
  $preflightCudaCalls = Get-OptionalMetric $preflight "(?m)^Pricing CUDA calls:\s*(.+)$"
  $preflightCpuCalls = Get-OptionalMetric $preflight "(?m)^Pricing CPU calls:\s*(.+)$"
  if ($preflightBackend -ne "CUDA" -or ($null -ne $preflightCudaCalls -and $preflightCudaCalls -le 0)) {
    throw "CUDA was required but is not active. Phase 3 reported Pricing backend: $preflightBackend (CUDA calls: $preflightCudaCalls, CPU calls: $preflightCpuCalls). Rebuild Phase 3 with CUDA on an NVIDIA/CUDA machine before using -RequireCuda."
  }
  Write-Host "CUDA preflight: PASS (Pricing backend: $preflightBackend)"
} else {
  Write-Host "CUDA required: NO (CPU fallback is allowed)"
}

$rows = @()
foreach ($model in $models) {
  Write-Host "========================================"
  Write-Host "===== $model ====="
  Write-Host "========================================"
  $p2 = Invoke-Solver $Phase2Exe $model
  $p3 = Invoke-Solver $Phase3Exe $model

  $p2Obj = Get-Metric $p2 "(?m)^Objective:\s*(.+)$" { param($v) [double]$v }
  $p3Obj = Get-Metric $p3 "(?m)^Objective:\s*(.+)$" { param($v) [double]$v }
  $p2Time = Get-Metric $p2 "(?m)^Timing total_ms:\s*(.+)$" { param($v) [double]$v }
  $p3Time = Get-Metric $p3 "(?m)^Timing total_ms:\s*(.+)$" { param($v) [double]$v }
  $p2Pricing = Get-Metric $p2 "(?m)^Timing pricing_ms:\s*(.+)$" { param($v) [double]$v }
  $p3Pricing = Get-Metric $p3 "(?m)^Timing pricing_ms:\s*(.+)$" { param($v) [double]$v }
  $p2Backend = Get-OptionalMetric $p2 "(?m)^Timing pricing_backend_ms:\s*(.+)$"
  $p3Backend = Get-OptionalMetric $p3 "(?m)^Timing pricing_backend_ms:\s*(.+)$"
  $p2Selection = Get-OptionalMetric $p2 "(?m)^Timing pricing_selection_ms:\s*(.+)$"
  $p3Selection = Get-OptionalMetric $p3 "(?m)^Timing pricing_selection_ms:\s*(.+)$"
  $p3WorkspaceInit = Get-OptionalMetric $p3 "(?m)^Timing pricing_workspace_init_ms:\s*(.+)$"
  $p3H2D = Get-OptionalMetric $p3 "(?m)^Timing pricing_host_to_device_ms:\s*(.+)$"
  $p3Kernel = Get-OptionalMetric $p3 "(?m)^Timing pricing_kernel_ms:\s*(.+)$"
  $p3D2H = Get-OptionalMetric $p3 "(?m)^Timing pricing_device_to_host_ms:\s*(.+)$"
  $p3BackendMode = Get-OptionalText $p3 "(?m)^Pricing backend:\s*(.+)$"
  $p3CudaCalls = Get-OptionalMetric $p3 "(?m)^Pricing CUDA calls:\s*(.+)$"
  $p3CpuCalls = Get-OptionalMetric $p3 "(?m)^Pricing CPU calls:\s*(.+)$"
  if ($RequireCuda -and ($p3BackendMode -ne "CUDA" -or ($null -ne $p3CudaCalls -and $p3CudaCalls -le 0))) {
    throw "CUDA was required but model $model reported Pricing backend: $p3BackendMode (CUDA calls: $p3CudaCalls, CPU calls: $p3CpuCalls)."
  }

  $p2Iter = Get-Metric $p2 "(?m)^Iterations:\s*(.+)$" { param($v) [int]$v }
  $p3Iter = Get-Metric $p3 "(?m)^Iterations:\s*(.+)$" { param($v) [int]$v }
  $p2Cert = Get-Metric $p2 "(?m)^Certificate:\s*(.+)$" { param($v) $v }
  $p3Cert = Get-Metric $p3 "(?m)^Certificate:\s*(.+)$" { param($v) $v }

  $speedup = if ($p3Time -gt 0) { $p2Time / $p3Time } else { 0 }
  $pricingSpeedup = if ($p3Pricing -gt 0) { $p2Pricing / $p3Pricing } else { 0 }
  $objDiff = [math]::Abs($p2Obj - $p3Obj)
  $faster = $p3Time -lt $p2Time
  $rows += [pscustomobject]@{
    model=$model
    phase2_ms=$p2Time
    phase3_ms=$p3Time
    phase3_speedup=[math]::Round($speedup,4)
    phase2_pricing_ms=$p2Pricing
    phase3_pricing_ms=$p3Pricing
    phase2_pricing_backend_ms=$p2Backend
    phase3_pricing_backend_ms=$p3Backend
    phase2_pricing_selection_ms=$p2Selection
    phase3_pricing_selection_ms=$p3Selection
    phase3_workspace_init_ms=$p3WorkspaceInit
    phase3_host_to_device_ms=$p3H2D
    phase3_kernel_ms=$p3Kernel
    phase3_device_to_host_ms=$p3D2H
    phase3_backend=$p3BackendMode
    phase3_cuda_calls=$p3CudaCalls
    phase3_cpu_calls=$p3CpuCalls
    pricing_speedup=[math]::Round($pricingSpeedup,4)
    phase2_iterations=$p2Iter
    phase3_iterations=$p3Iter
    objective_difference=$objDiff
    phase2_certificate=$p2Cert
    phase3_certificate=$p3Cert
    phase3_faster=$faster
  }

  Write-Host ("Phase2: {0:N4} ms | Phase3: {1:N4} ms | Speedup: {2:N2}x | Backend: {3} | Cert: {4}" -f $p2Time,$p3Time,$speedup,$p3BackendMode,$p3Cert)
}

$parent = Split-Path $OutputCsv -Parent
if ($parent -and !(Test-Path $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
$rows | Export-Csv -Path $OutputCsv -NoTypeInformation

$totalP2 = ($rows | Measure-Object -Property phase2_ms -Sum).Sum
$totalP3 = ($rows | Measure-Object -Property phase3_ms -Sum).Sum
$totalP2Pricing = ($rows | Measure-Object -Property phase2_pricing_ms -Sum).Sum
$totalP3Pricing = ($rows | Measure-Object -Property phase3_pricing_ms -Sum).Sum
$totalP2Backend = ($rows | Measure-Object -Property phase2_pricing_backend_ms -Sum).Sum
$totalP3Backend = ($rows | Measure-Object -Property phase3_pricing_backend_ms -Sum).Sum
$totalP2Selection = ($rows | Measure-Object -Property phase2_pricing_selection_ms -Sum).Sum
$totalP3Selection = ($rows | Measure-Object -Property phase3_pricing_selection_ms -Sum).Sum
$totalP3WorkspaceInit = ($rows | Measure-Object -Property phase3_workspace_init_ms -Sum).Sum
$totalP3H2D = ($rows | Measure-Object -Property phase3_host_to_device_ms -Sum).Sum
$totalP3Kernel = ($rows | Measure-Object -Property phase3_kernel_ms -Sum).Sum
$totalP3D2H = ($rows | Measure-Object -Property phase3_device_to_host_ms -Sum).Sum
$totalP2Iterations = ($rows | Measure-Object -Property phase2_iterations -Sum).Sum
$totalP3Iterations = ($rows | Measure-Object -Property phase3_iterations -Sum).Sum
$aggregateSpeedup = if ($totalP3 -gt 0) { $totalP2 / $totalP3 } else { 0 }
$aggregatePricingSpeedup = if ($totalP3Pricing -gt 0) { $totalP2Pricing / $totalP3Pricing } else { 0 }
$wins = @($rows | Where-Object { $_.phase3_faster }).Count
$losses = @($rows | Where-Object { !$_.phase3_faster }).Count
$ties = @($rows | Where-Object { $_.phase2_ms -eq $_.phase3_ms }).Count
$certPass = @($rows | Where-Object { $_.phase3_certificate -eq "PASS" }).Count
$maxObjectiveDiff = ($rows | Measure-Object -Property objective_difference -Maximum).Maximum
$avgP2 = $totalP2 / $rows.Count
$avgP3 = $totalP3 / $rows.Count
$avgSpeedup = ($rows | Measure-Object -Property phase3_speedup -Average).Average

$sortedSpeedups = @($rows | ForEach-Object { [double]$_.phase3_speedup } | Sort-Object)
$mid = [math]::Floor($sortedSpeedups.Count / 2)
$medianSpeedup = if ($sortedSpeedups.Count % 2 -eq 0) {
  ($sortedSpeedups[$mid - 1] + $sortedSpeedups[$mid]) / 2
} else {
  $sortedSpeedups[$mid]
}

$summary = [pscustomobject]@{
  models=$rows.Count
  phase2_total_ms=[math]::Round($totalP2,4)
  phase3_total_ms=[math]::Round($totalP3,4)
  aggregate_speedup=[math]::Round($aggregateSpeedup,4)
  phase2_average_ms=[math]::Round($avgP2,4)
  phase3_average_ms=[math]::Round($avgP3,4)
  average_per_model_speedup=[math]::Round($avgSpeedup,4)
  median_per_model_speedup=[math]::Round($medianSpeedup,4)
  phase2_pricing_total_ms=[math]::Round($totalP2Pricing,4)
  phase3_pricing_total_ms=[math]::Round($totalP3Pricing,4)
  pricing_aggregate_speedup=[math]::Round($aggregatePricingSpeedup,4)
  phase2_backend_total_ms=[math]::Round($totalP2Backend,4)
  phase3_backend_total_ms=[math]::Round($totalP3Backend,4)
  phase2_selection_total_ms=[math]::Round($totalP2Selection,4)
  phase3_selection_total_ms=[math]::Round($totalP3Selection,4)
  phase3_workspace_init_total_ms=[math]::Round($totalP3WorkspaceInit,4)
  phase3_host_to_device_total_ms=[math]::Round($totalP3H2D,4)
  phase3_kernel_total_ms=[math]::Round($totalP3Kernel,4)
  phase3_device_to_host_total_ms=[math]::Round($totalP3D2H,4)
  phase2_iterations_total=$totalP2Iterations
  phase3_iterations_total=$totalP3Iterations
  phase3_wins=$wins
  phase3_losses=$losses
  phase3_ties=$ties
  phase3_certificates_pass=$certPass
  max_objective_difference=$maxObjectiveDiff
}
$summaryParent = Split-Path $OutputSummaryCsv -Parent
if ($summaryParent -and !(Test-Path $summaryParent)) { New-Item -ItemType Directory -Path $summaryParent -Force | Out-Null }
$summary | Export-Csv -Path $OutputSummaryCsv -NoTypeInformation

$reportParent = Split-Path $OutputReport -Parent
if ($reportParent -and !(Test-Path $reportParent)) { New-Item -ItemType Directory -Path $reportParent -Force | Out-Null }

$report = @()
$report += "# Phase 3 Benchmark Report"
$report += ""
$report += "Generated by `indigenous_solver_phase3/tools/benchmark_phase3.ps1`."
$report += ""
$report += "## Summary"
$report += ""
$report += "| Metric | Result |"
$report += "|---|---:|"
$report += "| Models | $($rows.Count) |"
$report += "| Phase 2 total time | $([math]::Round($totalP2,4)) ms |"
$report += "| Phase 3 total time | $([math]::Round($totalP3,4)) ms |"
$report += "| Aggregate Phase 3 speedup | $([math]::Round($aggregateSpeedup,4))x |"
$report += "| Average Phase 2 time/model | $([math]::Round($avgP2,4)) ms |"
$report += "| Average Phase 3 time/model | $([math]::Round($avgP3,4)) ms |"
$report += "| Average per-model speedup | $([math]::Round($avgSpeedup,4))x |"
$report += "| Median per-model speedup | $([math]::Round($medianSpeedup,4))x |"
$report += "| Phase 2 pricing total | $([math]::Round($totalP2Pricing,4)) ms |"
$report += "| Phase 3 pricing total | $([math]::Round($totalP3Pricing,4)) ms |"
$report += "| Phase 2 backend pricing total | $([math]::Round($totalP2Backend,4)) ms |"
$report += "| Phase 3 backend pricing total | $([math]::Round($totalP3Backend,4)) ms |"
$report += "| Phase 2 selection total | $([math]::Round($totalP2Selection,4)) ms |"
$report += "| Phase 3 selection total | $([math]::Round($totalP3Selection,4)) ms |"
$report += "| Phase 3 workspace initialization | $(Format-Metric $totalP3WorkspaceInit) ms |"
$report += "| Phase 3 host → device | $(Format-Metric $totalP3H2D) ms |"
$report += "| Phase 3 CUDA kernel | $(Format-Metric $totalP3Kernel) ms |"
$report += "| Phase 3 device → host | $(Format-Metric $totalP3D2H) ms |"
$report += "| Aggregate pricing speedup | $([math]::Round($aggregatePricingSpeedup,4))x |"
$report += "| Phase 2 iterations | $totalP2Iterations |"
$report += "| Phase 3 iterations | $totalP3Iterations |"
$report += "| Phase 3 faster | $wins / $($rows.Count) |"
$report += "| Phase 3 slower | $losses / $($rows.Count) |"
$report += "| Phase 3 certificates PASS | $certPass / $($rows.Count) |"
$report += "| CUDA required | $RequireCuda |"
$report += "| Phase 3 CUDA calls | $(($rows | Measure-Object -Property phase3_cuda_calls -Sum).Sum) |"
$report += "| Phase 3 CPU calls | $(($rows | Measure-Object -Property phase3_cpu_calls -Sum).Sum) |"
$report += "| Maximum objective difference | $maxObjectiveDiff |"
$report += ""
$report += "## Per-model Results"
$report += ""
$report += "| Model | Phase 2 ms | Phase 3 ms | Speedup | P2 Pricing ms | P3 Pricing ms | P2 Backend | P3 Backend | P3 Backend Mode | P3 CUDA Calls | P3 CPU Calls | P3 Selection | P3 Cert |"
$report += "|---|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|---|"
foreach ($row in $rows) {
  $report += "| $($row.model) | $([math]::Round($row.phase2_ms,4)) | $([math]::Round($row.phase3_ms,4)) | $([math]::Round($row.phase3_speedup,4))x | $([math]::Round($row.phase2_pricing_ms,4)) | $([math]::Round($row.phase3_pricing_ms,4)) | $([math]::Round($row.phase2_pricing_backend_ms,4)) | $([math]::Round($row.phase3_pricing_backend_ms,4)) | $($row.phase3_backend) | $($row.phase3_cuda_calls) | $($row.phase3_cpu_calls) | $([math]::Round($row.phase3_pricing_selection_ms,4)) | $($row.phase3_certificate) |"
}
$report += ""
$report += "## Interpretation"
$report += ""
if ($aggregateSpeedup -gt 1) {
  $report += "- Phase 3 is faster overall on this benchmark set, with an aggregate speedup of $([math]::Round($aggregateSpeedup,4))x."
} elseif ($aggregateSpeedup -lt 1) {
  $report += "- Phase 3 is slower overall on this benchmark set, with an aggregate speedup of $([math]::Round($aggregateSpeedup,4))x."
} else {
  $report += "- Phase 3 and Phase 2 have equal aggregate timing on this benchmark set."
}
$report += "- Phase 3 is faster on $wins of $($rows.Count) models."
$report += "- All Phase 3 certificates passed: $certPass/$($rows.Count)."
$report += "- Aggregate pricing speedup is $([math]::Round($aggregatePricingSpeedup,4))x."
$report += "- Objective differences are checked against the solver outputs; the maximum absolute difference between Phase 2 and Phase 3 is $maxObjectiveDiff."
$report += ""
$report += "> Note: This benchmark compares Phase 2 and Phase 3 on the current machine. If CUDA is unavailable, Phase 3 uses its CPU fallback; therefore these results are not a measurement of NVIDIA GPU acceleration."
$report | Set-Content -Path $OutputReport -Encoding UTF8

Write-Host ""
Write-Host "=== Phase 3 Benchmark Summary ==="
Write-Host ("Models: {0} | Faster: {1} | Slower: {2} | Certificates PASS: {3}/{0}" -f $rows.Count,$wins,$losses,$certPass)
Write-Host ("Total: Phase2 {0:N4} ms | Phase3 {1:N4} ms | Aggregate speedup: {2:N2}x" -f $totalP2,$totalP3,$aggregateSpeedup)
Write-Host ("Pricing: Phase2 {0:N4} ms | Phase3 {1:N4} ms | Aggregate speedup: {2:N2}x" -f $totalP2Pricing,$totalP3Pricing,$aggregatePricingSpeedup)
Write-Host ("Backend: Phase2 {0:N4} ms | Phase3 {1:N4} ms | Selection: Phase2 {2:N4} ms | Phase3 {3:N4} ms" -f $totalP2Backend,$totalP3Backend,$totalP2Selection,$totalP3Selection)
Write-Host ("Phase3 GPU stages: init {0} ms | H2D {1} ms | kernel {2} ms | D2H {3} ms" -f (Format-Metric $totalP3WorkspaceInit),(Format-Metric $totalP3H2D),(Format-Metric $totalP3Kernel),(Format-Metric $totalP3D2H))
Write-Host ("Average per-model speedup: {0:N2}x | Median: {1:N2}x | Max objective diff: {2:E6}" -f $avgSpeedup,$medianSpeedup,$maxObjectiveDiff)
Write-Host ""
Write-Host "CSV: $OutputCsv"
Write-Host "Summary CSV: $OutputSummaryCsv"
Write-Host ("CUDA requirement: {0}" -f $RequireCuda)
Write-Host "Report: $OutputReport"
