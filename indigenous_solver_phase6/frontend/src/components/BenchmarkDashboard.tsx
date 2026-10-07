import { useCallback, useEffect, useMemo, useState } from "react";
import { getBenchmarkSnapshot } from "../services/api";
import type { BenchmarkSnapshot } from "../types/benchmark";

const unavailable: BenchmarkSnapshot = {
  summary: { status: "unavailable", modelCount: 0, phase2TotalMs: null, phase3TotalMs: null, phase4TotalMs: null, aggregateSpeedup: null, phase3FasterCount: 0, phase4FasterCount: 0, certificatePassCount: 0, gpuRuns: 0, cpuRuns: 0, message: "Benchmark API is not available. No performance values have been inferred locally." },
  cases: [],
};
function fmt(v: number | null) { return v == null ? "—" : v.toFixed(3) + " ms"; }
function speed(v: number | null) { return v == null ? "—" : v.toFixed(2) + "×"; }

export function BenchmarkDashboard() {
  const [data, setData] = useState(unavailable);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const refresh = useCallback(async () => {
    setLoading(true);
    try { setData(await getBenchmarkSnapshot()); setError(null); }
    catch (e) { setData(unavailable); setError(e instanceof Error ? e.message : "Benchmark endpoint unavailable."); }
    finally { setLoading(false); }
  }, []);
  useEffect(() => { void refresh(); }, [refresh]);
  const rows = useMemo(() => data.cases, [data.cases]);
  const s = data.summary;
  return <section className="dashboard">
    <div className="hero"><div><span className="eyebrow">PHASE 6.9 · PERFORMANCE ANALYTICS</span><h2>Benchmark dashboard</h2><p>Compare solver phases, backend execution, speedup, iterations, objectives, and numerical certificates from authoritative benchmark results.</p></div>
      <div className={s.status === "ok" ? "hero-badge" : "hero-badge muted-badge"}><span className={s.status === "ok" ? "status-dot" : "status-dot muted"} />{s.status === "ok" ? "Benchmark data live" : "Data unavailable"}</div></div>
    {error && <article className="panel monitor-notice"><strong>Benchmark data unavailable</strong><span>{s.message ?? error}</span><button className="secondary-button" onClick={() => void refresh()} disabled={loading}>{loading ? "Refreshing…" : "Retry"}</button></article>}
    <div className="metric-grid">
      <Metric label="Models" value={String(s.modelCount)} detail="Benchmark cases" /><Metric label="P2 total" value={fmt(s.phase2TotalMs)} detail="Phase 2 baseline" />
      <Metric label="P3 total" value={fmt(s.phase3TotalMs)} detail="GPU pricing pipeline" /><Metric label="Aggregate speedup" value={speed(s.aggregateSpeedup)} detail="P2 / P3" />
    </div>
    <div className="content-grid">
      <article className="panel"><Heading eyebrow="COMPARISON" title="Phase performance" /><Status label="Phase 3 faster" value={String(s.phase3FasterCount)} /><Status label="Phase 4 faster" value={String(s.phase4FasterCount)} /><Status label="Phase 2 total" value={fmt(s.phase2TotalMs)} /><Status label="Phase 3 total" value={fmt(s.phase3TotalMs)} /><Status label="Phase 4 total" value={fmt(s.phase4TotalMs)} /></article>
      <article className="panel"><Heading eyebrow="VERIFICATION" title="Benchmark integrity" /><Status label="Certificates passed" value={s.modelCount ? s.certificatePassCount + " / " + s.modelCount : "—"} ok={s.modelCount > 0 && s.certificatePassCount === s.modelCount} /><Status label="CPU runs" value={String(s.cpuRuns)} /><Status label="GPU runs" value={String(s.gpuRuns)} /><Status label="Benchmark status" value={s.status.toUpperCase()} ok={s.status === "ok"} /><Status label="Timestamp" value={s.timestamp ?? "UNKNOWN"} /></article>
    </div>
    <article className="panel table-panel activity"><div className="panel-heading"><div><span className="eyebrow">MODEL-BY-MODEL</span><h3>Benchmark results</h3></div><button className="secondary-button" onClick={() => void refresh()} disabled={loading}>{loading ? "Refreshing…" : "Refresh"}</button></div>
      {rows.length === 0 ? <p className="empty-state">{loading ? "Loading benchmark results…" : "No benchmark records are available from the solver API."}</p> :
      <div className="table-wrap"><table><thead><tr><th>Model</th><th>Phase 2</th><th>Phase 3</th><th>Phase 4</th><th>Speedup</th><th>Backend</th><th>Status</th><th>Certificate</th><th>Iterations</th><th>Objective</th></tr></thead>
      <tbody>{rows.map(row => <tr key={row.model}><td>{row.model}</td><td>{fmt(row.phase2Ms)}</td><td>{fmt(row.phase3Ms)}</td><td>{fmt(row.phase4Ms)}</td><td>{speed(row.speedup)}</td><td>{row.backend}</td><td>{row.status.toUpperCase()}</td><td>{row.certificatePassed ? "PASS" : "FAIL"}</td><td>{row.iterations ?? "—"}</td><td>{row.objective == null ? "—" : row.objective.toPrecision(8)}</td></tr>)}</tbody></table></div>}
    </article>
    <article className="panel activity"><Heading eyebrow="TRUTHFUL PERFORMANCE" title="Benchmark interpretation" /><p className="model-message">{s.message ?? "Values shown here originate from the C++ benchmark service. The browser does not estimate timings or GPU speedups."}</p></article>
  </section>;
}
function Metric({ label, value, detail }: { label: string; value: string; detail: string }) { return <article className="metric-card"><span>{label}</span><strong>{value}</strong><small>{detail}</small></article>; }
function Heading({ eyebrow, title }: { eyebrow: string; title: string }) { return <div className="panel-heading"><div><span className="eyebrow">{eyebrow}</span><h3>{title}</h3></div></div>; }
function Status({ label, value, ok = false }: { label: string; value: string; ok?: boolean }) { return <div className="status-row"><span>{label}</span><strong><i className={ok ? "status-dot" : "status-dot muted"} />{value}</strong></div>; }
