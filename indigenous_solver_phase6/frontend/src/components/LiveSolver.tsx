import { useEffect, useMemo, useRef, useState, type CSSProperties } from "react";
import type { UploadedModel } from "../types/model";
import type { SolverConfiguration } from "../types/solverConfig";
import { getRuntimeSnapshot, getSolveResult, getSolveStatus, inspectModel, solveModel } from "../services/api";
import type { SolveResult, SolveJob } from "../types/solve";
import type { RuntimeSnapshot } from "../types/runtime";

interface Props {
  model: UploadedModel | null;
  config: SolverConfiguration;
  onJobCreated?: (jobId: string) => void;
  onSolveComplete?: (jobId: string) => void;
  onOpenResults?: () => void;
}

const initialJob: SolveJob = {
  jobId: null, modelId: null, status: "idle", message: "Ready to start a solve.",
  startedAt: null, updatedAt: null, configuration: null,
};

export function LiveSolver({ model, config, onJobCreated, onSolveComplete, onOpenResults }: Props) {
  const [job, setJob] = useState<SolveJob>(initialJob);
  const [progress, setProgress] = useState(0);
  const [iteration, setIteration] = useState<number | null>(null);
  const [objective, setObjective] = useState<number | null>(null);
  const [elapsed, setElapsed] = useState<number | null>(null);
  const [result, setResult] = useState<SolveResult | null>(null);
  const [runtime, setRuntime] = useState<RuntimeSnapshot | null>(null);
  const [error, setError] = useState("");
  const timer = useRef<number | null>(null);
  const runtimeTimer = useRef<number | null>(null);

  const canStart = Boolean(model?.summary.status === "Ready" && !["submitting", "queued", "running"].includes(job.status));
  const statusLabel = useMemo(() => job.status.replace("_", " ").toUpperCase(), [job.status]);
  const terminal = ["optimal", "infeasible", "unbounded", "iteration_limit", "error"].includes(job.status);
  const successful = result?.status === "optimal";

  useEffect(() => () => {
    if (timer.current !== null) window.clearTimeout(timer.current);
    if (runtimeTimer.current !== null) window.clearInterval(runtimeTimer.current);
  }, []);

  const refreshRuntime = async () => {
    try {
      const snapshot = await getRuntimeSnapshot();
      setRuntime(snapshot);
    } catch {
      setRuntime(null);
    }
  };

  const loadResult = async (jobId: string) => {
    try {
      const solved = await getSolveResult(jobId);
      setResult(solved);
      setProgress(solved.status === "optimal" ? 100 : progress);
      setIteration(solved.iterations);
      setObjective(solved.objective);
      setElapsed(solved.elapsedMs);
      await refreshRuntime();
      onSolveComplete?.(jobId);
    } catch (err) {
      setError("The solve finished, but the result record could not be loaded: " + (err instanceof Error ? err.message : "unknown error"));
    }
  };

  const poll = (jobId: string) => {
    timer.current = window.setTimeout(async () => {
      try {
        const response = await getSolveStatus(jobId);
        setJob((current) => ({ ...current, status: response.status, message: response.message, updatedAt: new Date().toISOString() }));
        if (typeof response.progress === "number") setProgress(Math.max(0, Math.min(100, response.progress)));
        if (typeof response.iteration === "number") setIteration(response.iteration);
        if (response.objective !== undefined) setObjective(response.objective);
        if (typeof response.elapsedMs === "number") setElapsed(response.elapsedMs);
        await refreshRuntime();
        if (response.status === "queued" || response.status === "running") poll(jobId);
        else await loadResult(jobId);
      } catch (err) {
        setJob((current) => ({ ...current, status: "error", message: err instanceof Error ? err.message : "Unable to read solver status.", updatedAt: new Date().toISOString() }));
        setError("The solver API stopped responding while the job was being monitored.");
      }
    }, 1000);
  };

  const start = async () => {
    if (!model) { setError("Select a valid LP or MPS model before solving."); return; }
    setError("");
    setProgress(0);
    setIteration(null);
    setObjective(null);
    setElapsed(null);
    setResult(null);
    setRuntime(null);
    setJob({ jobId: null, modelId: null, status: "submitting", message: "Registering model with solver API…", startedAt: new Date().toISOString(), updatedAt: new Date().toISOString(), configuration: config });
    try {
      const inspected = await inspectModel(model.file);
      if (!inspected.modelId) throw new Error("Solver API did not return a model ID during model inspection.");
      setJob((current) => ({ ...current, modelId: inspected.modelId, message: "Submitting model to solver API…", updatedAt: new Date().toISOString() }));
      const response = await solveModel({ modelId: inspected.modelId, configuration: config });
      setJob({ jobId: response.jobId, modelId: response.modelId ?? inspected.modelId, status: response.status, message: response.message, startedAt: new Date().toISOString(), updatedAt: new Date().toISOString(), configuration: config });
      onJobCreated?.(response.jobId);
      void refreshRuntime();
      if (response.status === "queued" || response.status === "running") {
        poll(response.jobId);
      } else {
        setProgress(response.status === "optimal" ? 100 : 0);
        await loadResult(response.jobId);
      }
    } catch (err) {
      const message = err instanceof Error ? err.message : "Solver submission failed.";
      setJob((current) => ({ ...current, status: "error", message, updatedAt: new Date().toISOString() }));
      setError("Solver API error: " + message);
    }
  };

  const ftran = runtime?.ftranCalls ?? 0;
  const btran = runtime?.btranCalls ?? 0;
  const pricing = runtime?.pricingCalls ?? 0;
  const totalOps = ftran + btran + pricing;
  const operationSegments = totalOps > 0
    ? [
        { label: "FTRAN", value: ftran, cls: "swatch-1" },
        { label: "BTRAN", value: btran, cls: "swatch-2" },
        { label: "PRICING", value: pricing, cls: "swatch-3" },
      ].filter((item) => item.value > 0)
    : [];

  return (
    <section className="live-control-room">
      <div className="live-toolbar">
        <div className="live-breadcrumb"><span>INDIGENOUS SOLVER</span><b>/</b><strong>LIVE SOLVER</strong><i>●</i></div>
        <div className="live-toolbar-actions">
          <span className={terminal ? "live-state complete" : "live-state"}><span className="status-dot" /> {terminal ? "SOLVE COMPLETE" : statusLabel}</span>
          <span className="live-refresh">1s telemetry</span>
          {result && (
            <button className="live-action secondary" onClick={onOpenResults}>FULL RESULTS ↗</button>
          )}
          <button
            className={result ? "live-action solve-again" : "live-action solve-now"}
            onClick={start}
            disabled={!canStart}
          >
            {["submitting", "queued", "running"].includes(job.status)
              ? "SOLVING…"
              : result
                ? "↻ SOLVE AGAIN"
                : "SOLVE"}
          </button>
        </div>
      </div>

      <div className="live-filterbar">
        <div className="live-filter"><small>MODEL</small><strong>{model?.summary.name ?? "NO MODEL"}</strong><span>⌄</span></div>
        <div className="live-filter"><small>FORMAT</small><strong>{model?.summary.format ?? "—"}</strong><span>⌄</span></div>
        <div className="live-filter"><small>BACKEND</small><strong>{runtime?.executionBackend ?? config.backendPolicy.toUpperCase()}</strong><span>⌄</span></div>
        <div className="live-filter"><small>JOB</small><strong>{shortId(job.jobId)}</strong><span>⌄</span></div>
        <div className="live-filter live-filter-wide"><small>RUNTIME</small><strong>{runtime?.gpuRuntimeActive ? "CUDA GPU ACTIVE" : "CPU FALLBACK / CPU EXECUTION"}</strong><span>●</span></div>
      </div>

      <div className="live-notice">
        <strong>{successful ? "OPTIMAL SOLUTION VERIFIED" : terminal ? statusLabel : "SOLVER EXECUTION IN PROGRESS"}</strong>
        <span>{result?.message ?? job.message}</span>
        {error && <em>{error}</em>}
      </div>

      {!model && (
        <div className="live-solve-cta">
          <div>
            <span className="eyebrow">SOLVER READY</span>
            <strong>Upload an LP or MPS model to begin.</strong>
            <p>Choose a model first, then return here to start the solve.</p>
          </div>
        </div>
      )}
      {model && !result && canStart && (
        <div className="live-solve-cta ready">
          <div>
            <span className="eyebrow">READY TO SOLVE</span>
            <strong>{model.summary.name}</strong>
            <p>{model.summary.format} · {model.summary.rows ?? "—"} rows · {model.summary.columns ?? "—"} columns · backend {config.backendPolicy.toUpperCase()}</p>
          </div>
          <button className="live-main-solve" onClick={start}>▶ SOLVE MODEL</button>
        </div>
      )}

      <div className="live-overview-grid">
        <article className="live-panel live-identity">
          <div className="live-panel-title"><span>MODEL / JOB IDENTITY</span><b>{model?.summary.format ?? "NONE"}</b></div>
          <div className="live-model-name">{model?.summary.name ?? "No model selected"}</div>
          <div className="live-model-sub">{job.jobId ? "Job " + job.jobId : "Ready to submit"} · {runtime?.version ?? "phase6 runtime"}</div>
          <div className="live-mini-grid">
            <div><small>ROWS</small><strong>{model?.summary.rows ?? "—"}</strong></div>
            <div><small>COLUMNS</small><strong>{model?.summary.columns ?? "—"}</strong></div>
            <div><small>NONZEROS</small><strong>{model?.summary.nonzeros ?? "—"}</strong></div>
            <div><small>ITERATIONS</small><strong>{iteration ?? result?.iterations ?? "—"}</strong></div>
          </div>
        </article>

        <LiveGauge title="SOLVE COMPLETION" value={progress} display={Math.round(progress) + "%"} tone={successful ? "good" : "neutral"} />
        <LiveGauge title="NUMERICAL STABILITY" value={runtime?.numericalStable === false ? 35 : 100} display={runtime ? (runtime.numericalStable ? "STABLE" : "ALERT") : "N/A"} tone={runtime?.numericalStable === false ? "warn" : runtime ? "good" : "neutral"} />
        <LiveGauge title="BACKEND READINESS" value={runtime?.gpuRuntimeActive ? 100 : runtime?.executionBackend === "CPU" ? 72 : 0} display={runtime?.gpuRuntimeActive ? "CUDA" : runtime?.executionBackend ?? "N/A"} tone={runtime?.gpuRuntimeActive ? "good" : "neutral"} />
        <LiveGauge title="CERTIFICATE" value={result ? (result.certificate.passed ? 100 : 25) : 0} display={result ? (result.certificate.passed ? "PASS" : "FAIL") : "N/A"} tone={result?.certificate.passed ? "good" : result ? "warn" : "neutral"} />
      </div>

      <div className="live-metric-grid">
        <LiveMetric label="OBJECTIVE" value={objective === null ? "—" : formatNumber(objective)} sub="Authoritative solver result" />
        <LiveMetric label="ELAPSED" value={elapsed === null ? "—" : elapsed.toFixed(3) + " ms"} sub="C++ solve time" />
        <LiveMetric label="ITERATIONS" value={String(iteration ?? "—")} sub={"Limit " + config.maxIterations.toLocaleString()} />
        <LiveMetric label="VARIABLES" value={String(result?.variables.length ?? model?.summary.columns ?? "—")} sub="Solution vector" />
        <LiveMetric label="CONSTRAINTS" value={String(result?.constraints.length ?? model?.summary.rows ?? "—")} sub="Constraint set" />
        <LiveMetric label="BACKEND" value={result?.backend ?? runtime?.executionBackend ?? "—"} sub={result?.gpuActive ? "GPU active" : "CPU execution"} />
        <LiveMetric label="FALLBACKS" value={String(runtime?.fallbackCount ?? "—")} sub="Runtime fallback count" />
        <LiveMetric label="BOTTLENECK" value={runtime?.bottleneck ?? "—"} sub={runtime ? (runtime.bottleneckShare * 100).toFixed(1) + "% share" : "Telemetry"} />
      </div>

      <div className="live-two-column">
        <article className="live-panel live-status-panel">
          <div className="live-section-title"><div><span>THROTTLE / EXECUTION REASONS</span><strong>Solver backend state</strong></div><b>{runtime?.gpuRuntimeActive ? "GPU ACTIVE" : "CPU MODE"}</b></div>
          <LiveStatusRow label="CPU Execution" value={runtime?.executionBackend === "CPU" ? "ACTIVE" : "STANDBY"} active={runtime?.executionBackend === "CPU"} />
          <LiveStatusRow label="CUDA Compiled" value={runtime?.cudaCompiled ? "READY" : "NOT AVAILABLE"} active={runtime?.cudaCompiled ?? false} />
          <LiveStatusRow label="CUDA Device" value={runtime?.cudaDeviceReady ? "READY" : "NOT AVAILABLE"} active={runtime?.cudaDeviceReady ?? false} />
          <LiveStatusRow label="GPU Runtime" value={runtime?.gpuRuntimeActive ? "ACTIVE" : "INACTIVE"} active={runtime?.gpuRuntimeActive ?? false} />
          <LiveStatusRow label="CPU Fallback" value={runtime?.fallbackCount ? "USED" : (config.enableCpuFallback ? "ENABLED" : "DISABLED")} active={config.enableCpuFallback} />
          <LiveStatusRow label="Numerical Checks" value={runtime ? runtime.numericalChecks.toLocaleString() : "N/A"} active={Boolean(runtime?.numericalStable)} />
        </article>

        <article className="live-panel">
          <div className="live-section-title"><div><span>OPERATION MIX</span><strong>FTRAN / BTRAN / Pricing</strong></div><b>{totalOps.toLocaleString()} CALLS</b></div>
          {operationSegments.length ? <div className="live-pie-layout">
            <div className="live-pie" style={{ background: pieGradient(operationSegments.map((item) => item.value)) }}><div><strong>{totalOps}</strong><small>CALLS</small></div></div>
            <div className="live-pie-legend">{operationSegments.map((item) => <div key={item.label}><i className={item.cls} /><span>{item.label}</span><b>{item.value} · {((item.value / totalOps) * 100).toFixed(1)}%</b></div>)}</div>
          </div> : <div className="live-empty-chart">No operation calls reported by runtime.</div>}
        </article>
      </div>

      <div className="live-chart-grid">
        <TelemetryChart title="SOLVE PROGRESS" value={Math.round(progress) + "%"} subtitle="Authoritative job progress" mode={progress} terminal={terminal} />
        <TelemetryChart title="OBJECTIVE" value={objective === null ? "N/A" : formatNumber(objective)} subtitle="Final solver objective · single sample" mode={result ? 50 : 0} terminal={terminal} />
        <TelemetryChart title="NUMERICAL RESIDUAL" value={result?.certificate.primalResidual === undefined ? "N/A" : result.certificate.primalResidual.toExponential(2)} subtitle="Primal certificate residual" mode={result ? Math.max(0, Math.min(100, 100 - Math.min(100, Math.abs(result.certificate.primalResidual ?? 0) * 1e12))) : 0} terminal={terminal} />
        <TelemetryChart title="WORKSPACE REUSE" value={runtime ? runtime.workspaceReuses.toLocaleString() : "N/A"} subtitle="Persistent workspace telemetry" mode={runtime ? Math.min(100, runtime.workspaceReuses) : 0} terminal={terminal} />
      </div>

      {result && (
        <section className="live-solved-results">
          <div className="live-results-header">
            <div>
              <span className="eyebrow">SOLVE RESULT</span>
              <h2>{successful ? "OPTIMAL SOLUTION" : statusLabel}</h2>
              <p>Job {shortId(job.jobId)} · {result.backend} · {result.iterations} iterations · {result.elapsedMs.toFixed(3)} ms</p>
            </div>
            <div className="live-result-actions">
              <span className={result.certificate.passed ? "live-result-pass" : "live-result-fail"}>
                {result.certificate.passed ? "✓ CERTIFICATE PASS" : "✕ CERTIFICATE CHECK"}
              </span>
              <button className="live-main-solve" onClick={start} disabled={!canStart}>↻ SOLVE AGAIN</button>
            </div>
          </div>
          <div className="live-result-summary">
            <div><span>OBJECTIVE</span><strong>{formatNumber(result.objective)}</strong></div>
            <div><span>STATUS</span><strong>{result.status.toUpperCase()}</strong></div>
            <div><span>ITERATIONS</span><strong>{result.iterations}</strong></div>
            <div><span>PRIMAL RESIDUAL</span><strong>{(result.certificate.primalResidual ?? NaN).toExponential(3)}</strong></div>
          </div>
          <div className="live-result-tables">
            <div className="live-result-table">
              <div className="live-result-table-title"><span>VARIABLE SOLUTION</span><b>{result.variables.length} VARIABLES</b></div>
              <div className="live-table-scroll">
                <table><thead><tr><th>VARIABLE</th><th>VALUE</th><th>ABSOLUTE</th></tr></thead><tbody>
                  {result.variables.slice(0, 20).map((item) => (
                    <tr key={item.name}><td>{item.name}</td><td>{formatNumber(item.value)}</td><td>{formatNumber(Math.abs(item.value))}</td></tr>
                  ))}
                </tbody></table>
              </div>
              {result.variables.length > 20 && <small>Showing first 20 of {result.variables.length} variables. Open Full Results for the complete solution.</small>}
            </div>
            <div className="live-result-table">
              <div className="live-result-table-title"><span>CONSTRAINT RESIDUALS</span><b>{result.constraints.length} CONSTRAINTS</b></div>
              <div className="live-table-scroll">
                <table><thead><tr><th>CONSTRAINT</th><th>RESIDUAL</th><th>ABSOLUTE</th></tr></thead><tbody>
                  {result.constraints.slice(0, 20).map((item) => (
                    <tr key={item.name}><td>{item.name}</td><td>{formatNumber(item.residual)}</td><td>{item.residual == null ? "N/A" : formatNumber(Math.abs(item.residual))}</td></tr>
                  ))}
                </tbody></table>
              </div>
              {result.constraints.length > 20 && <small>Showing first 20 of {result.constraints.length} constraints. Open Full Results for the complete solution.</small>}
            </div>
          </div>
        </section>
      )}

      <div className="live-footer-state">
        <div><span className="eyebrow">POST-SOLVE OBSERVABILITY</span><strong>{successful ? "Solution available · verification state is authoritative" : terminal ? "Terminal solver state" : "Live execution telemetry"}</strong><p>{runtime?.message ?? "Telemetry is sourced from the native C++ API. GPU hardware metrics are shown only when the backend reports them."}</p></div>
        <div className="live-footer-actions">
          {result && <button onClick={onOpenResults}>OPEN RESULTS</button>}
          <span className="live-badge">{runtime?.gpuRuntimeActive ? "CUDA ACTIVE" : "CPU EXECUTION"}</span>
          <span className="live-badge">{result?.certificate.passed ? "CERTIFICATE PASS" : result ? "CERTIFICATE CHECK" : "AWAITING RESULT"}</span>
        </div>
      </div>
    </section>
  );
}

function LiveGauge({ title, value, display, tone }: { title: string; value: number; display: string; tone: "good" | "warn" | "neutral" }) {
  const safe = Math.max(0, Math.min(100, value));
  return <article className={"live-panel live-gauge " + tone}>
    <div className="live-panel-title"><span>{title}</span><b>{tone === "good" ? "OK" : tone === "warn" ? "CHECK" : "N/A"}</b></div>
    <div className="live-gauge-body"><div className="live-gauge-arc" style={{ "--gauge-value": safe * 2.7 + "deg" } as CSSProperties}><div><strong>{display}</strong><small>TELEMETRY</small></div></div></div>
  </article>;
}

function LiveMetric({ label, value, sub }: { label: string; value: string; sub: string }) {
  return <div className="live-info-tile"><span>{label}</span><strong>{value}</strong><small>{sub}</small></div>;
}

function LiveStatusRow({ label, value, active }: { label: string; value: string; active: boolean }) {
  return <div className="live-status-row"><span>{label}</span><strong className={active ? "good" : "muted"}><i className={active ? "status-dot" : "status-dot muted"} />{value}</strong></div>;
}

function TelemetryChart({ title, value, subtitle, mode, terminal }: { title: string; value: string; subtitle: string; mode: number; terminal: boolean }) {
  const height = 92;
  const current = Math.max(8, Math.min(92, mode));
  const points = Array.from({ length: 16 }, () => current);
  const polyline = points.map((p, i) => (i * 100 / (points.length - 1)).toFixed(1) + "," + (height - p)).join(" ");
  return <article className="live-panel live-chart-panel">
    <div className="live-chart-heading"><div><span>{title}</span><small>{subtitle}</small></div><strong>{value}</strong></div>
    <div className="live-chart"><div className="live-grid-lines"><i/><i/><i/><i/></div><svg viewBox={"0 0 100 " + height} preserveAspectRatio="none" aria-hidden="true"><polyline points={polyline} fill="none" vectorEffect="non-scaling-stroke" /></svg></div>
    <div className="live-chart-meta"><span>SAMPLE</span><span>{terminal ? "POST-SOLVE" : "LIVE"}</span></div>
  </article>;
}

function pieGradient(values: number[]) {
  const total = values.reduce((sum, value) => sum + value, 0);
  let cursor = 0;
  const stops = values.map((value, index) => {
    const start = cursor;
    cursor += (value / total) * 100;
    return `${pieColor(index)} ${start}% ${cursor}%`;
  });
  return `conic-gradient(${stops.join(",")})`;
}

function pieColor(index: number) {
  return ["#5bd38b", "#46a9c9", "#d5b35d", "#c76a72", "#8b79c9"][index % 5];
}

function shortId(value: string | null | undefined) {
  if (!value) return "—";
  return value.length > 18 ? value.slice(0, 8) + "…" + value.slice(-6) : value;
}

function formatNumber(value: number | null | undefined) {
  if (value === null || value === undefined || !Number.isFinite(value)) return "N/A";
  return Math.abs(value) >= 100000 || (Math.abs(value) > 0 && Math.abs(value) < 0.0001) ? value.toExponential(4) : value.toFixed(4);
}
