# Phase 6.12 — Export & Reporting Contract

The Reports module is a presentation/export layer. It never runs the solver and never invents missing metrics.

## Endpoint

GET /reports/{jobId} returns the report for a completed solve.

If no job ID is supplied, the UI requests GET /reports/latest for the latest completed report.

## Response

{
  "status": "ok",
  "report": {
    "reportId": "report-...",
    "generatedAt": "2026-10-08T00:00:00Z",
    "jobId": "job-...",
    "model": {
      "name": "model.mps",
      "modelId": "model-...",
      "format": "MPS",
      "rows": 10,
      "columns": 20,
      "nonzeros": 100
    },
    "result": {},
    "runtime": {},
    "benchmark": {},
    "verification": {},
    "architecture": {}
  }
}

Existing Phase 6 contracts define the nested result/runtime/benchmark/verification/architecture fields. Missing sections are null and displayed as unavailable.

## Export policy

- JSON: complete report snapshot received from the API.
- CSV: compact audit summary derived only from fields present in the report.
- No hardcoded benchmark totals, verification counts, GPU activity, or solver results are permitted.
- PDF generation remains a server-side/reporting concern and is not simulated in the browser.

## Availability

When the API is unavailable, the UI shows an explicit unavailable state rather than presenting placeholder solver data.
