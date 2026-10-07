import { useEffect, useState } from "react";
import { getSolveResult } from "../services/api";
import type { SolveResult } from "../types/solve";

interface Props { jobId: string | null; }

export function ResultsExplorer({ jobId }: Props) {
  const [result, setResult] = useState<SolveResult | null>(null);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);
  const [tab, setTab] = useState<"variables" | "constraints">("variables");

  useEffect(() => {
    setResult(null); setError("");
    if (!jobId) return;
    setLoading(true);
    getSolveResult(jobId).then((data) => setResult(data)).catch((err) => setError(err instanceof Error ? err.message : "Unable to load solve results.")).finally(() => setLoading(false));
  }, [jobId]);

  if (!jobId) return <section className="dashboard"><div className="placeholder panel"><span className="eyebrow">PHASE 6.7 · RESULTS</span><h2>No completed solve selected</h2><p>Run a model from Live Solver first. Final variables, constraints, objective, and certificate data will appear here.</p></div></section>;
  if (loading) return <section className="dashboard"><div className="placeholder panel"><span className="eyebrow">PHASE 6.7 · RESULTS</span><h2>Loading solution…</h2><p>Requesting the authoritative result from the C++ solver API.</p></div></section>;
  if (error) return <section className="dashboard"><div className="placeholder panel"><span className="eyebrow">PHASE 6.7 · RESULTS</span><h2>Results unavailable</h2><p>{error}</p><p className="model-message">No result was fabricated by the frontend.</p></div></section>;
  if (!result) return null;

  return <section className="dashboard">
    <div className="hero"><div><span className="eyebrow">PHASE 6.7 · SOLUTION EXPLORER</span><h2>Optimization results</h2><p>Inspect the authoritative solution returned by the C++ solver.</p></div><div className="hero-badge"><span className={result.status === "optimal" ? "status-dot" : "status-dot danger"} /> {result.status.toUpperCase()}</div></div>
    <div className="metric-grid">
      <Metric label="Objective" value={formatNumber(result.objective)} detail="Final objective value" />
      <Metric label="Iterations" value={String(result.iterations)} detail="Simplex iterations" />
      <Metric label="Elapsed" value={result.elapsedMs.toFixed(3) + " ms"} detail="Solver runtime" />
      <Metric label="Backend" value={result.backend} detail={result.gpuActive ? "GPU active" : "CPU execution"} />
    </div>
    <div className="content-grid">
      <article className="panel"><div className="panel-heading"><div><span className="eyebrow">CERTIFICATE</span><h3>Numerical verification</h3></div><span className="badge">{result.certificate.passed ? "PASS" : "FAIL"}</span></div>
        <StatusLine label="Primal residual" value={formatNumber(result.certificate.primalResidual)} /><StatusLine label="Dual residual" value={formatNumber(result.certificate.dualResidual)} /><StatusLine label="Complementarity" value={formatNumber(result.certificate.complementarityResidual)} /><p className="model-message">{result.certificate.message}</p>
      </article>
      <article className="panel"><div className="panel-heading"><div><span className="eyebrow">EXECUTION</span><h3>Solver context</h3></div></div><StatusLine label="Job ID" value={result.jobId} /><StatusLine label="Model ID" value={result.modelId} /><StatusLine label="Backend" value={result.backend} /><StatusLine label="GPU runtime" value={result.gpuActive ? "ACTIVE" : "NOT ACTIVE"} /></article>
    </div>
    <article className="panel table-panel"><div className="panel-heading"><div><span className="eyebrow">SOLUTION DATA</span><h3>Explore model solution</h3></div><div className="tabs"><button className={tab === "variables" ? "tab active" : "tab"} onClick={() => setTab("variables")}>Variables ({result.variables.length})</button><button className={tab === "constraints" ? "tab active" : "tab"} onClick={() => setTab("constraints")}>Constraints ({result.constraints.length})</button></div></div>
      {tab === "variables" ? <div className="table-wrap"><table><thead><tr><th>Variable</th><th>Value</th><th>Reduced cost</th><th>Lower</th><th>Upper</th></tr></thead><tbody>{result.variables.map((v) => <tr key={v.name}><td>{v.name}</td><td>{formatNumber(v.value)}</td><td>{formatNumber(v.reducedCost)}</td><td>{formatNumber(v.lowerBound)}</td><td>{formatNumber(v.upperBound)}</td></tr>)}</tbody></table></div> : <div className="table-wrap"><table><thead><tr><th>Constraint</th><th>Activity</th><th>RHS</th><th>Dual</th><th>Residual</th></tr></thead><tbody>{result.constraints.map((c) => <tr key={c.name}><td>{c.name}</td><td>{formatNumber(c.activity)}</td><td>{formatNumber(c.rhs)}</td><td>{formatNumber(c.dualValue)}</td><td>{formatNumber(c.residual)}</td></tr>)}</tbody></table></div>}
    </article>
  </section>;
}

function Metric({ label, value, detail }: { label:string; value:string; detail:string }) { return <article className="metric-card"><span>{label}</span><strong>{value}</strong><small>{detail}</small></article>; }
function StatusLine({ label, value }: { label:string; value:string }) { return <div className="status-row"><span>{label}</span><strong>{value}</strong></div>; }
function formatNumber(value: number | null | undefined) { return value === null || value === undefined || !Number.isFinite(value) ? "—" : Number(value).toPrecision(10).replace(/\.0+$/,""); }
