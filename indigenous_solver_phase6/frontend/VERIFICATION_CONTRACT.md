# Phase 6.10 Verification API Contract

## Endpoint
GET /verification

The C++ solver verification runner is authoritative. The browser does not execute tests, infer pass/fail state, or fabricate counts.

## Response shape
The endpoint returns a VerificationSnapshot containing a summary and individual checks. Subsystem counters are nullable when the backend did not report that category.

Status values:
- ok: required verification completed and passed.
- degraded: completed with warnings or skipped checks but no required failure.
- failed: one or more required checks failed.
- unavailable: no authoritative snapshot is available.

The illustrative example below is not live data:

{
  "summary": {
    "status": "ok",
    "passed": 22,
    "total": 22,
    "failed": 0,
    "skipped": 0,
    "numericalStable": true,
    "certificatePassCount": 10,
    "fallbackChecksPassed": 1,
    "regressionChecksPassed": 10,
    "ftranChecksPassed": 1,
    "btranChecksPassed": 1,
    "pricingChecksPassed": 1,
    "workspaceChecksPassed": 1,
    "gpuRuntimeReady": false,
    "lastVerifiedAt": "2026-10-07T00:00:00Z",
    "message": "Verification passed."
  },
  "checks": []
}

The UI displays NOT REPORTED/UNKNOWN for null values and shows an explicit unavailable state when the endpoint cannot provide data.
