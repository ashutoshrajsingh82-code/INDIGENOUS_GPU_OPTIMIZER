import { useCallback, useEffect, useMemo, useState } from "react";
import { getReport } from "../services/api";
import type { SolverReport } from "../types/report";

function download(name: string, content: string, mime: string) {
  const blob = new Blob([content], { type: mime });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = name;
  anchor.click();
  URL.revokeObjectURL(url);
}
function csvEscape(value: unknown): string {
  const text = value == null ? "" : String(value);
  return /[",\n]/.test(text) ? '"' + text.replaceAll('"', '""') + '"' : text;
}
function reportCsv(report: SolverReport): string {
  const rows: Array<[string, unknown]> = [
    ["reportId", report.reportId], ["generatedAt", report.generatedAt], ["jobId", report.jobId],
    ["modelId", report.model.modelId], ["modelName", report.model.name], ["modelFormat", report.model.format],
    ["rows", report.model.rows], ["columns", report.model.columns], ["nonzeros", report.model.nonzeros],
    ["status", report.result?.status], ["objective", report.result?.objective], ["iterations", report.result?.iterations],
    ["elapsedMs", report.result?.elapsedMs], ["backend", report.result?.backend], ["gpuActive", report.result?.gpuActive],
    ["certificatePassed", report.result?.certificate.passed],
    ["primalResidual", report.result?.certificate.primalResidual], ["dualResidual", report.result?.certificate.dualResidual],
    ["complementarityResidual", report.result?.certificate.complementarityResidual],
    ["runtimeBackend", report.runtime?.executionBackend], ["cudaCompiled", report.runtime?.cudaCompiled],
    ["cudaDeviceReady", report.runtime?.cudaDeviceReady], ["gpuRuntimeActive", report.runtime?.gpuRuntimeActive],
    ["workspacePersistent", report.runtime?.workspacePersistent], ["fallbackCount", report.runtime?.fallbackCount],
    ["bottleneck", report.runtime?.bottleneck], ["bottleneckShare", report.runtime?.bottleneckShare],
    ["verificationStatus", report.verification?.summary.status], ["verificationPassed", report.verification?.summary.passed],
    ["verificationTotal", report.verification?.summary.total], ["verificationFailed", report.verification?.summary.failed],
    ["benchmarkModels", report.benchmark?.summary.modelCount], ["benchmarkSpeedup", report.benchmark?.summary.aggregateSpeedup],
    ["benchmarkCertificatePassCount", report.benchmark?.summary.certificatePassCount],
  ];
  return "field,value\n" + rows.map(([key, value]) => csvEscape(key) + "," + csvEscape(value)).join("\n") + "\n";
}

export function ReportsDashboard({ jobId }: { jobId: string | null }) {
  const [report, setReport] = useState<SolverReport | null>(null);
  const [status, setStatus] = useState<"loading" | "ready" | "unavailable">("loading");
  const [message, setMessage] = useState("");

  const load = useCallback(async () => {
    setStatus("loading");
    try {
      const response = await getReport(jobId);
      if (response.status !== "ok" || !response.report) {
        setReport(null); setStatus("unavailable"); setMessage(response.message ?? "Report data is unavailable."); return;
      }
      setReport(response.report); setStatus("ready"); setMessage("");
    } catch (error) {
      setReport(null); setStatus("unavailable");
      setMessage(error instanceof Error ? error.message : "Report API is unavailable.");
    }
  }, [jobId]);
  useEffect(() => { void load(); }, [load]);
  const json = useMemo(() => report ? JSON.stringify(report, null, 2) : "", [report]);

  if (status !== "ready" || !report) return <section className="dashboard"><article className="panel unavailable-panel">
    <span className="eyebrow">PHASE 6.12 · EXPORT & REPORTING</span><h2>Report unavailable</h2>
    <p>{status === "loading" ? "Loading the authoritative report from the solver API…" : message || "No report has been generated yet."}</p>
    <button className="secondary-button" onClick={() => void load()} disabled={status === "loading"}>Refresh</button>
  </article></section>;

  return <section className="dashboard">
    <div className="hero"><div><span className="eyebrow">PHASE 6.12 · EXPORT & REPORTING</span><h2>Solver report center</h2>
      <p>Export the authoritative solver, runtime, benchmark, verification, and architecture snapshot returned by the API.</p></div>
      <div className="hero-badge"><span className="status-dot" /> Report ready</div></div>
    <div className="metric-grid">
      <article className="metric-card"><span>Status</span><strong>{report.result?.status?.toUpperCase() ?? "N/A"}</strong><small>Solver result</small></article>
      <article className="metric-card"><span>Objective</span><strong>{formatNumber(report.result?.objective)}</strong><small>Final objective value</small></article>
      <article className="metric-card"><span>Iterations</span><strong>{report.result?.iterations ?? "N/A"}</strong><small>Simplex iterations</small></article>
      <article className="metric-card"><span>Backend</span><strong>{report.result?.backend ?? report.runtime?.executionBackend ?? "N/A"}</strong><small>{report.result?.gpuActive ? "GPU active" : "CPU execution"}</small></article>
    </div>
    <div className="content-grid">
      <article className="panel"><div className="panel-heading"><div><span className="eyebrow">IDENTITY</span><h3>Report metadata</h3></div></div>
        <StatusRow label="Report ID" value={report.reportId}/><StatusRow label="Generated" value={new Date(report.generatedAt).toLocaleString()}/>
        <StatusRow label="Job ID" value={report.jobId ?? "N/A"}/><StatusRow label="Model ID" value={report.model.modelId ?? "N/A"}/>
        <StatusRow label="Model" value={report.model.name ?? "N/A"}/><StatusRow label="Format" value={report.model.format ?? "N/A"}/>
      </article>
      <article className="panel"><div className="panel-heading"><div><span className="eyebrow">CERTIFICATE</span><h3>Numerical result</h3></div><span className="badge">{report.result?.certificate.passed ? "PASS" : "NOT PASSED"}</span></div>
        <StatusRow label="Elapsed" value={report.result ? report.result.elapsedMs + " ms" : "N/A"}/>
        <StatusRow label="Primal residual" value={formatNumber(report.result?.certificate.primalResidual)}/>
        <StatusRow label="Dual residual" value={formatNumber(report.result?.certificate.dualResidual)}/>
        <StatusRow label="Complementarity" value={formatNumber(report.result?.certificate.complementarityResidual)}/>
        <p className="model-message">{report.result?.certificate.message ?? report.message ?? "No certificate message."}</p>
      </article>
    </div>
    <div className="content-grid">
      <article className="panel"><div className="panel-heading"><div><span className="eyebrow">RUNTIME</span><h3>Execution summary</h3></div></div>
        <StatusRow label="CUDA compiled" value={bool(report.runtime?.cudaCompiled)}/><StatusRow label="CUDA device" value={bool(report.runtime?.cudaDeviceReady)}/>
        <StatusRow label="GPU runtime" value={bool(report.runtime?.gpuRuntimeActive)}/><StatusRow label="Persistent workspace" value={bool(report.runtime?.workspacePersistent)}/>
        <StatusRow label="Fallbacks" value={String(report.runtime?.fallbackCount ?? "N/A")}/><StatusRow label="Bottleneck" value={report.runtime?.bottleneck ?? "N/A"}/>
      </article>
      <article className="panel"><div className="panel-heading"><div><span className="eyebrow">VERIFICATION</span><h3>Validation summary</h3></div></div>
        <StatusRow label="Status" value={report.verification?.summary.status?.toUpperCase() ?? "N/A"}/>
        <StatusRow label="Passed / total" value={report.verification ? report.verification.summary.passed + " / " + report.verification.summary.total : "N/A"}/>
        <StatusRow label="Failed" value={String(report.verification?.summary.failed ?? "N/A")}/>
        <StatusRow label="Numerically stable" value={report.verification ? bool(report.verification.summary.numericalStable) : "N/A"}/>
      </article>
    </div>
    <article className="panel activity"><div className="panel-heading"><div><span className="eyebrow">EXPORT</span><h3>Download report</h3></div></div>
      <div className="config-actions"><button className="primary-button" onClick={() => download("solver-report.json", json, "application/json")}>Export JSON</button>
      <button className="secondary-button" onClick={() => download("solver-report.csv", reportCsv(report), "text/csv;charset=utf-8")}>Export CSV</button>
      <button className="secondary-button" onClick={() => void load()}>Refresh report</button></div>
      <p className="model-message">JSON preserves the complete API snapshot. CSV provides a compact audit summary. No browser-generated solver values are added.</p>
    </article>
  </section>;
}
function formatNumber(value: number | null | undefined) { return value == null ? "N/A" : Number.isFinite(value) ? value.toPrecision(8) : String(value); }
function bool(value: boolean | null | undefined) { return value == null ? "N/A" : value ? "YES" : "NO"; }
function StatusRow({ label, value }: { label: string; value: string }) { return <div className="status-row"><span>{label}</span><strong>{value}</strong></div>; }
