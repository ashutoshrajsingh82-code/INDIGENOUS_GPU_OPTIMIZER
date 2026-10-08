import { useEffect, useMemo, useState } from "react";
import { getSolveResult } from "../services/api";
import type { SolveResult } from "../types/solve";

interface Props { jobId: string | null; }

export function ResultsExplorer({ jobId }: Props) {
  const [result, setResult] = useState<SolveResult | null>(null);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);
  const [tab, setTab] = useState<"variables" | "constraints">("variables");

  useEffect(() => {
    setResult(null);
    setError("");
    if (!jobId) return;
    setLoading(true);
    getSolveResult(jobId)
      .then(setResult)
      .catch((err) => setError(err instanceof Error ? err.message : "Unable to load solve results."))
      .finally(() => setLoading(false));
  }, [jobId]);

  if (!jobId) return <ResultsState title="No completed solve selected" message="Run a model from Live Solver first. Final variables, constraints, objective, and certificate data will appear here." />;
  if (loading) return <ResultsState title="Loading solution…" message="Requesting the authoritative result from the C++ solver API." />;
  if (error) return <ResultsState title="Results unavailable" message={error} danger />;
  if (!result) return null;

  return <section className="results-grafana">
    <header className="results-toolbar">
      <div className="results-breadcrumb"><span className="results-menu">☰</span><span>Home</span><b>›</b><span>Dashboards</span><b>›</b><span>Solver</span><b>›</b><strong>Optimization Results</strong></div>
      <div className="results-toolbar-actions"><span className="results-refresh">● Live result</span><span>↻</span><span>⋮</span></div>
    </header>

    <div className="results-filterbar">
      <Filter label="Job" value={shortId(result.jobId)} />
      <Filter label="Model" value={shortId(result.modelId)} />
      <Filter label="Backend" value={result.backend} />
      <Filter label="Status" value={result.status.toUpperCase()} good={result.status === "optimal"} />
      <Filter label="GPU" value={result.gpuActive ? "ACTIVE" : "NO DATA"} good={result.gpuActive} />
    </div>

    <div className="results-grid results-top-grid">
      <Panel className="result-identity"><PanelTitle title="Solver Result" /><div className="identity-value">{result.status === "optimal" ? "OPTIMAL" : result.status.toUpperCase()}</div><div className="identity-meta">{result.message || "Authoritative result returned by the native C++ solver."}</div><div className="identity-subgrid"><MiniStat label="Objective" value={formatNumber(result.objective)} /><MiniStat label="Iterations" value={String(result.iterations)} /></div></Panel>
      <Gauge title="Solve completion" value={result.status === "optimal" ? 100 : 0} display={result.status === "optimal" ? "100%" : "0%"} tone={result.status === "optimal" ? "good" : "warn"} />
      <Gauge title="Numerical certificate" value={result.certificate.passed ? 100 : 0} display={result.certificate.passed ? "PASS" : "FAIL"} tone={result.certificate.passed ? "good" : "bad"} />
      <Gauge title="GPU runtime" value={result.gpuActive ? 100 : 0} display={result.gpuActive ? "ACTIVE" : "NO DATA"} tone={result.gpuActive ? "good" : "neutral"} />
    </div>

    <div className="results-grid results-mid-grid">
      <Panel><PanelTitle title="Execution summary" /><div className="summary-table">
        <SummaryRow label="Objective value" value={formatNumber(result.objective)} />
        <SummaryRow label="Solver iterations" value={String(result.iterations)} />
        <SummaryRow label="Elapsed time" value={result.elapsedMs.toFixed(3) + " ms"} />
        <SummaryRow label="Backend" value={result.backend} />
        <SummaryRow label="GPU execution" value={result.gpuActive ? "ACTIVE" : "NOT ACTIVE"} />
        <SummaryRow label="Certificate" value={result.certificate.passed ? "PASS" : "FAIL"} good={result.certificate.passed} />
      </div></Panel>
      <Panel><PanelTitle title="Numerical verification" badge={result.certificate.passed ? "PASS" : "FAIL"} /><div className="verification-bars">
        <ResidualBar label="Primal residual" value={result.certificate.primalResidual} />
        <ResidualBar label="Dual residual" value={result.certificate.dualResidual} />
        <ResidualBar label="Complementarity" value={result.certificate.complementarityResidual} />
      </div><p className="results-muted">{result.certificate.message}</p></Panel>
      <Panel><PanelTitle title="Model dimensions" /><div className="dimension-grid"><Dimension value={result.variables.length} label="Variables" /><Dimension value={result.constraints.length} label="Constraints" /><Dimension value={result.iterations} label="Iterations" /><Dimension value={result.elapsedMs.toFixed(3) + " ms"} label="Runtime" /></div></Panel>
    </div>

    <div className="results-grid results-charts-grid">
      <Panel className="chart-panel"><PanelTitle title="Solution variable profile" /><BarProfile values={result.variables.slice(0, 12).map((v) => ({ label: v.name, value: v.value }))} empty="No variable values returned." /></Panel>
      <Panel className="chart-panel"><PanelTitle title="Solution magnitude mix" /><PieChart values={result.variables.filter((v) => Number.isFinite(v.value) && Math.abs(v.value) > 0).map((v) => ({ label: v.name, value: Math.abs(v.value) }))} empty="No non-zero variable magnitudes returned." /></Panel>
      <Panel className="chart-panel"><PanelTitle title="Constraint residual profile" /><BarProfile values={result.constraints.slice(0, 12).map((c) => ({ label: c.name, value: c.residual ?? Math.abs(c.activity - c.rhs) }))} empty="No constraint residuals returned." /></Panel>
      <Panel className="chart-panel"><PanelTitle title="Residual magnitude mix" /><PieChart values={result.constraints.filter((c) => Number.isFinite(c.residual ?? (c.activity - c.rhs)) && Math.abs(c.residual ?? (c.activity - c.rhs)) > 0).map((c) => ({ label: c.name, value: Math.abs(c.residual ?? (c.activity - c.rhs)) }))} empty="No non-zero residual magnitudes returned." /></Panel>
      <Panel className="chart-panel chart-note"><PanelTitle title="Runtime telemetry" /><div className="telemetry-empty"><span>⌁</span><strong>Telemetry not captured</strong><p>The result API exposes final solver measurements, not a historical time series. No synthetic GPU/CPU graph is generated.</p></div></Panel>
    </div>

    <Panel className="solution-panel"><div className="solution-heading"><PanelTitle title="Solution explorer" /><div className="results-tabs"><button className={tab === "variables" ? "active" : ""} onClick={() => setTab("variables")}>Variables <b>{result.variables.length}</b></button><button className={tab === "constraints" ? "active" : ""} onClick={() => setTab("constraints")}>Constraints <b>{result.constraints.length}</b></button></div></div>
      {tab === "variables" ? <div className="result-table-wrap"><table><thead><tr><th>Variable</th><th>Value</th><th>Reduced cost</th><th>Lower</th><th>Upper</th></tr></thead><tbody>{result.variables.map((v) => <tr key={v.name}><td>{v.name}</td><td>{formatNumber(v.value)}</td><td>{formatNumber(v.reducedCost)}</td><td>{formatNumber(v.lowerBound)}</td><td>{formatNumber(v.upperBound)}</td></tr>)}</tbody></table></div> : <div className="result-table-wrap"><table><thead><tr><th>Constraint</th><th>Activity</th><th>RHS</th><th>Dual</th><th>Residual</th></tr></thead><tbody>{result.constraints.map((c) => <tr key={c.name}><td>{c.name}</td><td>{formatNumber(c.activity)}</td><td>{formatNumber(c.rhs)}</td><td>{formatNumber(c.dualValue)}</td><td>{formatNumber(c.residual)}</td></tr>)}</tbody></table></div>}
    </Panel>
  </section>;
}

function Panel({ children, className = "" }: { children: React.ReactNode; className?: string }) { return <article className={"results-panel " + className}>{children}</article>; }
function PanelTitle({ title, badge }: { title: string; badge?: string }) { return <div className="results-panel-title"><span>{title}</span>{badge && <b className={badge === "PASS" ? "pass" : "fail"}>{badge}</b>}</div>; }
function Filter({ label, value, good = false }: { label: string; value: string; good?: boolean }) { return <div className="results-filter"><small>{label}</small><strong>{good && <i>●</i>}{value}</strong><span>⌄</span></div>; }
function MiniStat({ label, value }: { label: string; value: string }) { return <div><small>{label}</small><strong>{value}</strong></div>; }
function SummaryRow({ label, value, good }: { label: string; value: string; good?: boolean }) { return <div className="summary-row"><span>{label}</span><strong className={good ? "good-text" : ""}>{value}</strong></div>; }
function Dimension({ value, label }: { value: number | string; label: string }) { return <div className="dimension"><strong>{value}</strong><span>{label}</span></div>; }

function Gauge({ title, value, display, tone }: { title: string; value: number; display: string; tone: "good" | "warn" | "bad" | "neutral" }) {
  const angle = -135 + (Math.max(0, Math.min(100, value)) * 2.7);
  return <Panel className={"result-gauge " + tone}><PanelTitle title={title} /><div className="gauge-wrap"><div className="gauge-arc" style={{ "--gauge-angle": angle + "deg" } as React.CSSProperties}><div className="gauge-core"><strong>{display}</strong></div></div></div></Panel>;
}

function ResidualBar({ label, value }: { label: string; value?: number }) {
  const valid = value !== undefined && Number.isFinite(value);
  const display = valid ? formatNumber(value) : "—";
  const ratio = valid ? Math.min(100, Math.max(0, -Math.log10(Math.max(Math.abs(value as number), 1e-16)) * 10)) : 0;
  return <div className="residual-item"><div><span>{label}</span><strong>{display}</strong></div><div className="residual-track"><i style={{ width: ratio + "%" }} /></div></div>;
}

function BarProfile({ values, empty }: { values: Array<{ label: string; value: number }>; empty: string }) {
  const max = Math.max(...values.map((v) => Math.abs(v.value)), 0);
  if (!values.length) return <div className="telemetry-empty"><strong>{empty}</strong></div>;
  return <div className="bar-profile">{values.map((v) => <div className="profile-row" key={v.label}><span title={v.label}>{v.label}</span><div><i style={{ width: (max ? Math.max(2, Math.abs(v.value) / max * 100) : 2) + "%" }} /></div><b>{formatNumber(v.value)}</b></div>)}</div>;
}

function PieChart({ values, empty }: { values: Array<{ label: string; value: number }>; empty: string }) {
  const total = values.reduce((sum, item) => sum + Math.abs(item.value), 0);
  if (!values.length || total <= 0) return <div className="pie-empty">{empty}</div>;

  const slices = values.slice().sort((a, b) => b.value - a.value).slice(0, 8);
  const visibleTotal = slices.reduce((sum, item) => sum + Math.abs(item.value), 0);
  let offset = 0;
  const gradient = slices.map((item, index) => {
    const start = offset;
    offset += Math.abs(item.value) / visibleTotal * 100;
    return `var(--pie-${(index % 8) + 1}) ${start}% ${offset}%`;
  }).join(", ");

  return <div className="pie-chart-wrap">
    <div className="pie-chart" style={{ background: `conic-gradient(${gradient})` }}>
      <div className="pie-hole"><strong>{values.length}</strong><span>items</span></div>
    </div>
    <div className="pie-legend">{slices.map((item, index) => {
      const share = Math.abs(item.value) / visibleTotal * 100;
      return <div className="pie-legend-row" key={item.label}>
        <i className={`pie-swatch swatch-${(index % 8) + 1}`} />
        <span title={item.label}>{item.label}</span>
        <b>{share.toFixed(1)}%</b>
      </div>;
    })}</div>
    {values.length > slices.length && <small className="pie-caption">Showing 8 largest contributors; remaining items omitted for readability.</small>}
  </div>;
}

function ResultsState({ title, message, danger = false }: { title: string; message: string; danger?: boolean }) {
  return <section className="results-grafana"><Panel className={"results-state " + (danger ? "danger-state" : "")}><span className="results-state-icon">{danger ? "!" : "◌"}</span><h2>{title}</h2><p>{message}</p></Panel></section>;
}

function formatNumber(value: number | null | undefined) {
  return value === null || value === undefined || !Number.isFinite(value) ? "—" : Number(value).toPrecision(10).replace(/\.0+$/, "");
}

function shortId(value: string | null | undefined) {
  if (!value) return "—";
  return value.length > 18 ? value.slice(0, 8) + "…" + value.slice(-6) : value;
}
