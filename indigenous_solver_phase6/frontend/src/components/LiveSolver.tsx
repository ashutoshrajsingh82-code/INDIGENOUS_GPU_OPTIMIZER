import { useEffect, useMemo, useRef, useState, type CSSProperties } from "react";
import type { UploadedModel } from "../types/model";
import type { SolverConfiguration } from "../types/solverConfig";
import { getRuntimeSnapshot, getSolveStatus, solveModel } from "../services/api";
import type { RuntimeSnapshot } from "../types/runtime";
import type { SolveJob } from "../types/solve";

interface Props {
  model: UploadedModel | null;
  config: SolverConfiguration;
  onJobCreated?: (jobId: string) => void;
}

interface HistoryPoint {
  progress: number;
  iteration: number;
  objective: number;
  elapsed: number;
  pricing: number;
  ftran: number;
  btran: number;
}

const initialJob: SolveJob = {
  jobId: null,
  modelId: null,
  status: "idle",
  message: "Ready to start a solve.",
  startedAt: null,
  updatedAt: null,
  configuration: null,
};

const emptyRuntime: RuntimeSnapshot = {
  status: "unavailable",
  executionBackend: "UNKNOWN",
  cudaCompiled: false,
  cudaDeviceReady: false,
  gpuRuntimeActive: false,
  basisGpuActive: false,
  pricingGpuActive: false,
  ftranCalls: 0,
  btranCalls: 0,
  pricingCalls: 0,
  updateCalls: 0,
  workspaceAllocations: 0,
  workspaceReuses: 0,
  workspacePersistent: false,
  asyncBackend: "UNAVAILABLE",
  batchBackend: "UNAVAILABLE",
  adaptiveGpuEligible: false,
  numericalStable: true,
  numericalChecks: 0,
  numericalFailures: 0,
  fallbackCount: 0,
  bottleneck: "UNKNOWN",
  bottleneckShare: 0,
};

const MAX_HISTORY = 48;

export function LiveSolver({ model, config, onJobCreated }: Props) {
  const [job, setJob] = useState<SolveJob>(initialJob);
  const [progress, setProgress] = useState(0);
  const [iteration, setIteration] = useState<number | null>(null);
  const [objective, setObjective] = useState<number | null>(null);
  const [elapsed, setElapsed] = useState<number | null>(null);
  const [runtime, setRuntime] = useState<RuntimeSnapshot>(emptyRuntime);
  const [history, setHistory] = useState<HistoryPoint[]>([]);
  const [error, setError] = useState("");
  const timer = useRef<number | null>(null);

  const canStart = Boolean(
    model?.summary.status === "Ready" &&
      job.status !== "submitting" &&
      job.status !== "queued" &&
      job.status !== "running",
  );
  const statusLabel = useMemo(() => job.status.replaceAll("_", " ").toUpperCase(), [job.status]);
  const iterationLoad = config.maxIterations > 0 && iteration !== null
    ? Math.min(100, (iteration / config.maxIterations) * 100)
    : 0;
  const stability = runtime.numericalStable ? 100 : 0;

  const refreshRuntime = async () => {
    try {
      const snapshot = await getRuntimeSnapshot();
      setRuntime(snapshot);
      setHistory((current) => {
        const point: HistoryPoint = {
          progress,
          iteration: iteration ?? 0,
          objective: objective ?? 0,
          elapsed: elapsed ?? 0,
          pricing: snapshot.pricingCalls,
          ftran: snapshot.ftranCalls,
          btran: snapshot.btranCalls,
        };
        return [...current, point].slice(-MAX_HISTORY);
      });
    } catch {
      setRuntime((current) => ({ ...current, status: "unavailable", message: "Runtime endpoint unavailable." }));
    }
  };

  useEffect(() => {
    void refreshRuntime();
    const interval = window.setInterval(() => void refreshRuntime(), 1000);
    return () => window.clearInterval(interval);
  }, [progress, iteration, objective, elapsed]);

  useEffect(() => () => {
    if (timer.current !== null) window.clearTimeout(timer.current);
  }, []);

  const poll = (jobId: string) => {
    timer.current = window.setTimeout(async () => {
      try {
        const response = await getSolveStatus(jobId);
        setJob((current) => ({
          ...current,
          status: response.status,
          message: response.message,
          updatedAt: new Date().toISOString(),
        }));
        if (typeof response.progress === "number") setProgress(Math.max(0, Math.min(100, response.progress)));
        if (typeof response.iteration === "number") setIteration(response.iteration);
        if (response.objective !== undefined) setObjective(response.objective);
        if (typeof response.elapsedMs === "number") setElapsed(response.elapsedMs);
        if (response.status === "queued" || response.status === "running") poll(jobId);
      } catch (err) {
        setJob((current) => ({
          ...current,
          status: "error",
          message: err instanceof Error ? err.message : "Unable to read solver status.",
          updatedAt: new Date().toISOString(),
        }));
        setError("The solver API stopped responding while the job was being monitored.");
      }
    }, 1000);
  };

  const start = async () => {
    if (!model?.modelId) {
      setError("The solver API did not return a model ID. Re-inspect the model before solving.");
      return;
    }
    setError("");
    setProgress(0);
    setIteration(null);
    setObjective(null);
    setElapsed(null);
    setHistory([]);
    setJob({
      jobId: null,
      modelId: null,
      status: "submitting",
      message: "Submitting model to solver API…",
      startedAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
      configuration: config,
    });

    try {
      const response = await solveModel({ modelId: model.modelId, configuration: config });
      setJob({
        jobId: response.jobId,
        modelId: response.modelId ?? null,
        status: response.status,
        message: response.message,
        startedAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        configuration: config,
      });
      onJobCreated?.(response.jobId);
      if (response.status === "queued" || response.status === "running") poll(response.jobId);
    } catch (err) {
      setJob((current) => ({
        ...current,
        status: "error",
        message: err instanceof Error ? err.message : "Solver submission failed.",
        updatedAt: new Date().toISOString(),
      }));
      setError("Could not connect to the solver API. No solve was executed by the browser.");
    }
  };

  return (
    <section className="dashboard live-solver-dashboard">
      <div className="live-toolbar">
        <div>
          <span className="eyebrow">PHASE 6.6 · LIVE SOLVER</span>
          <h2>Live Solver Metrics</h2>
        </div>
        <div className="live-toolbar-actions">
          <span className="live-selector">{model?.summary.name ?? "No model selected"} ▾</span>
          <span className="live-refresh"><span className="status-dot" /> 1s refresh</span>
          <button className="secondary-button" onClick={() => void refreshRuntime()}>↻</button>
        </div>
      </div>

      <div className="live-kicker">
        <span>REVISED SIMPLEX</span>
        <span>•</span>
        <span>{config.backendPolicy.toUpperCase()} BACKEND POLICY</span>
        <span>•</span>
        <span className={job.status === "error" ? "live-state danger" : "live-state"}>{statusLabel}</span>
      </div>

      <div className="live-gauge-grid">
        <GaugeCard label="Solve Progress %" value={progress} center={progress.toFixed(0) + "%"} detail={job.message} />
        <GaugeCard label="Iteration Load %" value={iterationLoad} center={iteration === null ? "—" : String(iteration)} detail={"Limit " + config.maxIterations.toLocaleString()} />
        <GaugeCard label="Numerical Stability" value={stability} center={runtime.numericalStable ? "STABLE" : "FAIL"} detail={runtime.numericalChecks + " checks · " + runtime.numericalFailures + " failures"} />
        <GaugeCard label="CPU Fallback" value={config.enableCpuFallback ? 100 : 0} center={config.enableCpuFallback ? "ON" : "OFF"} detail="Truthful fallback policy" />
      </div>

      <div className="live-status-grid">
        <LiveStat label="Execution Backend" value={runtime.executionBackend} />
        <LiveStat label="CUDA Device" value={runtime.cudaDeviceReady ? "READY" : "NOT AVAILABLE"} muted={!runtime.cudaDeviceReady} />
        <LiveStat label="GPU Runtime" value={runtime.gpuRuntimeActive ? "ACTIVE" : "NOT ACTIVE"} muted={!runtime.gpuRuntimeActive} />
        <LiveStat label="Job ID" value={job.jobId ?? "—"} />
      </div>

      <div className="live-panel-grid">
        <article className="panel live-panel">
          <PanelTitle eyebrow="SOLVER ACTIVITY" title="Execution status" />
          <StatusLine label="Model" value={model?.summary.name ?? "No model selected"} />
          <StatusLine label="Rows / columns" value={model ? model.summary.rows + " / " + model.summary.columns : "—"} />
          <StatusLine label="Nonzeros" value={model ? String(model.summary.nonzeros) : "—"} />
          <StatusLine label="Iteration" value={iteration === null ? "—" : String(iteration)} />
          <StatusLine label="Objective" value={objective === null ? "—" : formatNumber(objective)} />
          <StatusLine label="Elapsed" value={elapsed === null ? "—" : elapsed.toFixed(2) + " ms"} />
          <button className="primary-button solve-button" disabled={!canStart || !model} onClick={() => void start()}>
            {job.status === "submitting" ? "Submitting…" : job.status === "running" ? "Solver running…" : "Start solve"}
          </button>
          {error && <div className="upload-error">{error}</div>}
        </article>

        <article className="panel live-panel">
          <PanelTitle eyebrow="THROTTLE / HEALTH" title="Solver conditions" />
          <HealthBar label="CUDA compiled" active={runtime.cudaCompiled} value={runtime.cudaCompiled ? "READY" : "NOT ACTIVE"} />
          <HealthBar label="CUDA device" active={runtime.cudaDeviceReady} value={runtime.cudaDeviceReady ? "READY" : "NOT ACTIVE"} />
          <HealthBar label="GPU pricing" active={runtime.pricingGpuActive} value={runtime.pricingGpuActive ? "ACTIVE" : "CPU FALLBACK"} />
          <HealthBar label="Basis GPU" active={runtime.basisGpuActive} value={runtime.basisGpuActive ? "ACTIVE" : "CPU FALLBACK"} />
          <HealthBar label="Persistent workspace" active={runtime.workspacePersistent} value={runtime.workspacePersistent ? "ACTIVE" : "NOT ACTIVE"} />
          <HealthBar label="Adaptive GPU eligible" active={runtime.adaptiveGpuEligible} value={runtime.adaptiveGpuEligible ? "YES" : "NO"} />
        </article>
      </div>

      <div className="live-chart-grid">
        <LiveChart title="Solve Progress" values={history.map((p) => p.progress)} suffix="%" />
        <LiveChart title="Objective" values={history.map((p) => p.objective)} formatter={formatNumber} />
        <LiveChart title="Elapsed Time" values={history.map((p) => p.elapsed)} suffix=" ms" />
        <LiveChart title="Iteration" values={history.map((p) => p.iteration)} />
      </div>

      <div className="live-chart-grid secondary">
        <LiveChart title="Pricing Calls" values={history.map((p) => p.pricing)} />
        <LiveChart title="FTRAN Calls" values={history.map((p) => p.ftran)} />
        <LiveChart title="BTRAN Calls" values={history.map((p) => p.btran)} />
        <LiveChart title="Workspace Reuses" values={history.map(() => runtime.workspaceReuses)} />
      </div>

      <article className="panel live-config-strip">
        <div><span className="eyebrow">EXECUTION POLICY</span><strong>{config.backendPolicy.toUpperCase()}</strong></div>
        <div><span className="eyebrow">CPU FALLBACK</span><strong>{config.enableCpuFallback ? "ENABLED" : "DISABLED"}</strong></div>
        <div><span className="eyebrow">ASYNC</span><strong>{runtime.asyncBackend}</strong></div>
        <div><span className="eyebrow">BATCH</span><strong>{runtime.batchBackend}</strong></div>
        <div><span className="eyebrow">BOTTLENECK</span><strong>{runtime.bottleneck} · {runtime.bottleneckShare.toFixed(1)}%</strong></div>
      </article>
    </section>
  );
}

function GaugeCard({ label, value, center, detail }: { label: string; value: number; center: string; detail: string }) {
  const safe = Math.max(0, Math.min(100, value));
  const style = { "--gauge-value": safe + "deg" } as CSSProperties;
  return (
    <article className="live-gauge-card">
      <div className="live-panel-label">{label}</div>
      <div className="gauge" style={style}>
        <div className="gauge-inner"><strong>{center}</strong></div>
      </div>
      <small>{detail}</small>
    </article>
  );
}

function LiveStat({ label, value, muted = false }: { label: string; value: string; muted?: boolean }) {
  return <article className="live-stat"><span>{label}</span><strong className={muted ? "muted-value" : ""}>{value}</strong></article>;
}

function HealthBar({ label, active, value }: { label: string; active: boolean; value: string }) {
  return (
    <div className="health-row">
      <span>{label}</span>
      <div className="health-track"><div className={active ? "health-fill active" : "health-fill"} style={{ width: active ? "100%" : "18%" }} /></div>
      <strong className={active ? "health-ok" : "health-muted"}>{value}</strong>
    </div>
  );
}

function PanelTitle({ eyebrow, title }: { eyebrow: string; title: string }) {
  return <div className="panel-heading"><div><span className="eyebrow">{eyebrow}</span><h3>{title}</h3></div></div>;
}

function StatusLine({ label, value }: { label: string; value: string }) {
  return <div className="status-row"><span>{label}</span><strong>{value}</strong></div>;
}

function LiveChart({ title, values, suffix = "", formatter = (value: number) => value.toFixed(0) }: { title: string; values: number[]; suffix?: string; formatter?: (value: number) => string }) {
  const current = values.length ? values[values.length - 1] : 0;
  const points = toPolyline(values, 320, 88);
  return (
    <article className="live-chart">
      <div className="live-chart-header"><span>{title}</span><strong>{values.length ? formatter(current) + suffix : "—"}</strong></div>
      <svg viewBox="0 0 320 88" preserveAspectRatio="none" role="img" aria-label={title + " history"}>
        <path className="chart-grid-line" d="M0 22H320 M0 44H320 M0 66H320" />
        {points && <polyline className="chart-line" points={points} />}
      </svg>
      <div className="chart-axis"><span>−48s</span><span>NOW</span></div>
    </article>
  );
}

function toPolyline(values: number[], width: number, height: number) {
  if (!values.length) return "";
  if (values.length === 1) return "0," + height / 2 + " " + width + "," + height / 2;
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;
  return values.map((value, index) => {
    const x = (index / (values.length - 1)) * width;
    const y = height - 8 - ((value - min) / range) * (height - 16);
    return x.toFixed(1) + "," + y.toFixed(1);
  }).join(" ");
}

function formatNumber(value: number) {
  if (!Number.isFinite(value)) return "—";
  return Math.abs(value) >= 1000 ? value.toLocaleString(undefined, { maximumFractionDigits: 2 }) : value.toFixed(4);
}
