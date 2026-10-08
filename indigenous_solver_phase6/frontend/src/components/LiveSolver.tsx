import { useEffect, useMemo, useRef, useState } from "react";
import type { UploadedModel } from "../types/model";
import type { SolverConfiguration } from "../types/solverConfig";
import { getSolveStatus, inspectModel, solveModel } from "../services/api";
import type { SolveJob, SolveJobStatus } from "../types/solve";

interface Props { model: UploadedModel | null; config: SolverConfiguration; onJobCreated?: (jobId: string) => void; onSolveComplete?: (jobId: string) => void; }

const initialJob: SolveJob = {
  jobId: null, modelId: null, status: "idle", message: "Ready to start a solve.",
  startedAt: null, updatedAt: null, configuration: null,
};

export function LiveSolver({ model, config, onJobCreated, onSolveComplete }: Props) {
  const [job, setJob] = useState<SolveJob>(initialJob);
  const [progress, setProgress] = useState(0);
  const [iteration, setIteration] = useState<number | null>(null);
  const [objective, setObjective] = useState<number | null>(null);
  const [elapsed, setElapsed] = useState<number | null>(null);
  const [error, setError] = useState("");
  const timer = useRef<number | null>(null);

  const canStart = Boolean(model?.summary.status === "Ready" && job.status !== "submitting" && job.status !== "queued" && job.status !== "running");
  const statusLabel = useMemo(() => job.status.replace("_", " ").toUpperCase(), [job.status]);

  useEffect(() => () => { if (timer.current !== null) window.clearTimeout(timer.current); }, []);

  const poll = (jobId: string) => {
    timer.current = window.setTimeout(async () => {
      try {
        const response = await getSolveStatus(jobId);
        setJob((current) => ({ ...current, status: response.status, message: response.message, updatedAt: new Date().toISOString() }));
        if (typeof response.progress === "number") setProgress(Math.max(0, Math.min(100, response.progress)));
        if (typeof response.iteration === "number") setIteration(response.iteration);
        if (response.objective !== undefined) setObjective(response.objective);
        if (typeof response.elapsedMs === "number") setElapsed(response.elapsedMs);
        if (response.status === "queued" || response.status === "running") poll(jobId);
      } catch (err) {
        setJob((current) => ({ ...current, status: "error", message: err instanceof Error ? err.message : "Unable to read solver status.", updatedAt: new Date().toISOString() }));
        setError("The solver API stopped responding while the job was being monitored.");
      }
    }, 1000);
  };

  const start = async () => {
    if (!model) { setError("Select a valid LP or MPS model before solving."); return; }
    setError(""); setProgress(0); setIteration(null); setObjective(null); setElapsed(null);
    setJob({ jobId: null, modelId: null, status: "submitting", message: "Registering model with solver API…", startedAt: new Date().toISOString(), updatedAt: new Date().toISOString(), configuration: config });
    try {
      // The native API keeps inspected models in process memory. Re-inspect immediately
      // before solving so a model selected before an API restart never has a stale/missing ID.
      const inspected = await inspectModel(model.file);
      if (!inspected.modelId) throw new Error("Solver API did not return a model ID during model inspection.");
      setJob((current) => ({ ...current, modelId: inspected.modelId, message: "Submitting model to solver API…", updatedAt: new Date().toISOString() }));
      const response = await solveModel({ modelId: inspected.modelId, configuration: config });
      setJob({ jobId: response.jobId, modelId: response.modelId ?? null, status: response.status, message: response.message, startedAt: new Date().toISOString(), updatedAt: new Date().toISOString(), configuration: config });
      onJobCreated?.(response.jobId);
      if (response.status === "queued" || response.status === "running") { poll(response.jobId); } else { onSolveComplete?.(response.jobId); }
    } catch (err) {
      setJob((current) => ({ ...current, status: "error", message: err instanceof Error ? err.message : "Solver submission failed.", updatedAt: new Date().toISOString() }));
      setError("Could not connect to the solver API. No solve was executed by the browser.");
    }
  };

  return <section className="dashboard">
    <div className="hero"><div><span className="eyebrow">PHASE 6.6 · LIVE SOLVER</span><h2>Run optimization</h2><p>Submit the selected model and monitor the authoritative C++ solver job without executing solver logic in the browser.</p></div><div className="hero-badge"><span className={job.status === "error" ? "status-dot danger" : "status-dot"} /> {statusLabel}</div></div>
    <div className="content-grid">
      <article className="panel">
        <div className="panel-heading"><div><span className="eyebrow">MODEL</span><h3>{model?.summary.name ?? "No model selected"}</h3></div><span className="badge">{model?.summary.format ?? "NONE"}</span></div>
        {model ? <><StatusLine label="Rows" value={String(model.summary.rows)} /><StatusLine label="Columns" value={String(model.summary.columns)} /><StatusLine label="Nonzeros" value={String(model.summary.nonzeros)} /></> : <p className="empty-state">Upload a model before starting a solve.</p>}
        <button className="primary-button solve-button" disabled={!canStart || !model} onClick={() => void start()}>{job.status === "submitting" ? "Submitting…" : "Start solve"}</button>
      </article>
      <article className="panel">
        <div className="panel-heading"><div><span className="eyebrow">JOB STATUS</span><h3>{statusLabel}</h3></div>{job.jobId && <span className="badge">{job.jobId}</span>}</div>
        <div className="progress-track"><div className="progress-fill" style={{ width: progress + "%" }} /></div><div className="progress-meta"><span>{Math.round(progress)}%</span><span>{job.message}</span></div>
        <StatusLine label="Iteration" value={iteration === null ? "—" : String(iteration)} /><StatusLine label="Objective" value={objective === null ? "—" : String(objective)} /><StatusLine label="Elapsed" value={elapsed === null ? "—" : elapsed.toFixed(2) + " ms"} />
        {error && <div className="upload-error">{error}</div>}
      </article>
    </div>
    <article className="panel activity"><div className="panel-heading"><div><span className="eyebrow">EXECUTION POLICY</span><h3>Configuration snapshot</h3></div></div><div className="action-grid"><StatusLine label="Backend" value={config.backendPolicy.toUpperCase()} /><StatusLine label="Max iterations" value={String(config.maxIterations)} /><StatusLine label="CPU fallback" value={config.enableCpuFallback ? "ENABLED" : "DISABLED"} /><StatusLine label="Validation" value={config.enableNumericalValidation ? "ENABLED" : "DISABLED"} /></div></article>
  </section>;
}

function StatusLine({ label, value }: { label: string; value: string }) {
  return <div className="status-row"><span>{label}</span><strong>{value}</strong></div>;
}
