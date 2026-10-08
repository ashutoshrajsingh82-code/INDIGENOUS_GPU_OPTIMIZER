import { useEffect, useRef, useState } from "react";
import type { UploadedModel } from "../types/model";
import { analyzeModelFile } from "../services/modelParser";
import { apiHealth, getRuntimeSnapshot, inspectModel as inspectApiModel } from "../services/api";
import type { RuntimeSnapshot } from "../types/runtime";

interface Props {
  onModelReady: (model: UploadedModel | null) => void;
}

type PipelineState = "idle" | "validating" | "parsing" | "ready" | "error";

interface BackendState {
  api: "connected" | "degraded" | "offline";
  parser: "ready" | "checking" | "unknown";
  execution: "CPU" | "CUDA" | "UNKNOWN";
  cudaReady: boolean;
}

const initialBackend: BackendState = {
  api: "offline",
  parser: "unknown",
  execution: "UNKNOWN",
  cudaReady: false,
};

export function ModelUpload({ onModelReady }: Props) {
  const inputRef = useRef<HTMLInputElement>(null);
  const [dragging, setDragging] = useState(false);
  const [error, setError] = useState("");
  const [inspecting, setInspecting] = useState(false);
  const [apiMessage, setApiMessage] = useState("");
  const [pipeline, setPipeline] = useState<PipelineState>("idle");
  const [backend, setBackend] = useState<BackendState>(initialBackend);
  const [runtime, setRuntime] = useState<RuntimeSnapshot | null>(null);
  const [currentModel, setCurrentModel] = useState<UploadedModel | null>(null);

  useEffect(() => {
    let mounted = true;

    const refreshBackend = async () => {
      try {
        const health = await apiHealth();
        if (!mounted) return;

        setBackend((previous) => ({
          ...previous,
          api: health.status === "ok" ? "connected" : "degraded",
          execution: health.executionBackend,
          cudaReady: health.cudaCompiled && health.cudaDeviceReady,
          parser: "checking",
        }));

        try {
          const snapshot = await getRuntimeSnapshot();
          if (!mounted) return;
          setRuntime(snapshot);
          setBackend((previous) => ({
            ...previous,
            api: snapshot.status === "ok" ? "connected" : "degraded",
            execution: snapshot.executionBackend,
            cudaReady: snapshot.cudaDeviceReady,
            parser: "ready",
          }));
        } catch {
          if (!mounted) return;
          setBackend((previous) => ({ ...previous, parser: "unknown" }));
        }
      } catch {
        if (!mounted) return;
        setBackend(initialBackend);
        setRuntime(null);
      }
    };

    void refreshBackend();
    const timer = window.setInterval(() => void refreshBackend(), 5000);
    return () => {
      mounted = false;
      window.clearInterval(timer);
    };
  }, []);

  const acceptFiles = async (files: FileList | File[]) => {
    const file = files[0];
    if (!file) return;

    setError("");
    setApiMessage("");
    setCurrentModel(null);
    onModelReady(null);
    setPipeline("validating");

    const analysis = await analyzeModelFile(file);
    const failedChecks = analysis.checks.filter((item) => item.status === "failed");

    if (!analysis.canSend || failedChecks.length > 0) {
      const reasons = failedChecks.map(
        (item) => item.name + ": " + (item.reason ?? "validation failed"),
      );
      setError(reasons.join(" · ") || "Browser validation failed.");
      setPipeline("error");
      return;
    }

    setPipeline("parsing");
    setInspecting(true);

    try {
      const remote = await inspectApiModel(file);
      const uploaded: UploadedModel = {
        file,
        modelId: remote.modelId,
        summary: {
          name: remote.name,
          format: remote.format,
          sizeBytes: remote.sizeBytes,
          rows: remote.rows,
          columns: remote.columns,
          nonzeros: remote.nonzeros,
          status: remote.status,
          message: remote.message,
        },
      };

      setCurrentModel(uploaded);
      onModelReady(uploaded);
      setApiMessage("Authoritative model dimensions received from the solver API.");
      setPipeline("ready");
    } catch (caught) {
      const message = caught instanceof Error ? caught.message : "Unknown solver API error.";
      const browserValidated: UploadedModel = {
        file,
        summary: {
          name: file.name,
          format: analysis.format,
          sizeBytes: file.size,
          rows: 0,
          columns: 0,
          nonzeros: 0,
          status: "Ready",
          message: "Browser validation passed; exact dimensions require the solver API.",
        },
      };

      setCurrentModel(browserValidated);
      onModelReady(browserValidated);
      setApiMessage("Browser validation passed. Solver API request failed: " + message);
      setError(message);
      setPipeline("error");
    } finally {
      setInspecting(false);
    }
  };

  const executionLabel = backend.execution === "CUDA" && backend.cudaReady
    ? "CUDA"
    : backend.execution === "CPU"
      ? "CPU FALLBACK"
      : "API OFFLINE";

  const pipelineItems = [
    { key: "UPLOAD", label: "Upload", state: pipeline === "idle" ? "active" : "complete" },
    { key: "VALIDATE", label: "Validate", state: pipeline === "validating" ? "active" : pipeline === "error" && !currentModel ? "error" : pipeline !== "idle" ? "complete" : "pending" },
    { key: "PARSE", label: "Parse", state: pipeline === "parsing" ? "active" : pipeline === "ready" ? "complete" : pipeline === "error" && currentModel ? "error" : "pending" },
    { key: "SOLVE", label: "Solve", state: "pending" },
    { key: "VERIFY", label: "Verify", state: "pending" },
    { key: "RESULTS", label: "Results", state: "pending" },
  ];

  const backendRows = [
    {
      name: "Solver API",
      value: backend.api === "connected" ? "CONNECTED" : backend.api === "degraded" ? "DEGRADED" : "OFFLINE",
      state: backend.api === "connected" ? "ok" : backend.api === "degraded" ? "warn" : "muted",
    },
    {
      name: "Model parser",
      value: backend.parser === "ready" ? "READY" : backend.parser === "checking" ? "CHECKING" : "—",
      state: backend.parser === "ready" ? "ok" : "muted",
    },
    {
      name: "CPU backend",
      value: backend.execution === "CPU" ? "ACTIVE" : backend.execution === "CUDA" ? "AVAILABLE" : "—",
      state: backend.execution === "CPU" ? "ok" : "muted",
    },
    {
      name: "CUDA backend",
      value: backend.cudaReady ? "READY" : "NOT AVAILABLE",
      state: backend.cudaReady ? "ok" : "muted",
    },
  ];

  return (
    <section className="model-workspace">
      <header className="model-workspace-header">
        <div>
          <span className="model-eyebrow">PHASE 6 / WEB CONTROL CENTER</span>
          <h2>Model Workspace</h2>
          <p>Upload, validate and prepare an optimization model for solving.</p>
        </div>
        <div className="model-runtime-status" aria-live="polite">
          <span className={backend.api === "connected" ? "model-status-led on" : "model-status-led"} />
          <div>
            <small>EXECUTION BACKEND</small>
            <strong>{executionLabel}</strong>
          </div>
          <i>{backend.api === "connected" ? "API ONLINE" : "API OFFLINE"}</i>
        </div>
      </header>

      <div className="model-workspace-grid">
        <main className="model-intake">
          <div className="model-section-bar">
            <div>
              <span className="model-section-kicker">01 · MODEL INTAKE</span>
              <strong>Optimization model</strong>
            </div>
            <span className="model-format-badge">LP / MPS · ≤ 50 MB</span>
          </div>

          <div
            className={dragging ? "model-dropzone dragging" : "model-dropzone"}
            onDragOver={(event) => {
              event.preventDefault();
              setDragging(true);
            }}
            onDragLeave={() => setDragging(false)}
            onDrop={(event) => {
              event.preventDefault();
              setDragging(false);
              void acceptFiles(event.dataTransfer.files);
            }}
            role="region"
            aria-label="Optimization model upload area"
          >
            <div className="model-upload-glyph" aria-hidden="true">
              <span>↑</span>
            </div>
            <div className="model-drop-copy">
              <strong>{inspecting ? "Inspecting model with solver API" : dragging ? "Release to inspect model" : "Drop your optimization model here"}</strong>
              <span>{inspecting ? "Browser validation passed · parsing authoritative model structure…" : "or browse from your computer"}</span>
            </div>
            <button
              className="model-primary-button"
              type="button"
              onClick={() => inputRef.current?.click()}
              disabled={inspecting}
            >
              {inspecting ? "Inspecting…" : "Choose model"}
            </button>
            <input
              ref={inputRef}
              hidden
              type="file"
              accept=".lp,.mps"
              aria-label="Choose LP or MPS model"
              onChange={(event) => {
                if (event.target.files) void acceptFiles(event.target.files);
                event.target.value = "";
              }}
            />
            <div className="model-upload-meta">
              <span><b>LP</b> Linear Programming</span>
              <span><b>MPS</b> Mathematical Programming System</span>
              <span>Maximum size <b>50 MB</b></span>
            </div>
          </div>

          {(error || apiMessage) && (
            <div className={error ? "model-feedback error" : "model-feedback"} role={error ? "alert" : "status"}>
              <span>{error ? "!" : "✓"}</span>
              <div>
                <strong>{error ? "Model intake requires attention" : "Model accepted"}</strong>
                <p>{error || apiMessage}</p>
              </div>
            </div>
          )}

          {currentModel && (
            <div className="model-loaded">
              <div className="model-loaded-heading">
                <div>
                  <span className="model-section-kicker">CURRENT MODEL</span>
                  <strong>{currentModel.summary.name}</strong>
                </div>
                <span className="model-ready-badge">READY</span>
              </div>
              <div className="model-stat-grid">
                <div><small>FORMAT</small><strong>{currentModel.summary.format}</strong></div>
                <div><small>ROWS</small><strong>{currentModel.summary.rows.toLocaleString()}</strong></div>
                <div><small>COLUMNS</small><strong>{currentModel.summary.columns.toLocaleString()}</strong></div>
                <div><small>NONZEROS</small><strong>{currentModel.summary.nonzeros.toLocaleString()}</strong></div>
                <div><small>SIZE</small><strong>{formatBytes(currentModel.summary.sizeBytes)}</strong></div>
              </div>
            </div>
          )}
        </main>

        <aside className="model-side-stack">
          <section className="model-panel backend-panel">
            <div className="model-panel-heading">
              <div>
                <span className="model-section-kicker">02 · RUNTIME</span>
                <strong>Backend status</strong>
              </div>
              <span className="model-live-tag">LIVE</span>
            </div>
            <div className="model-backend-list">
              {backendRows.map((row) => (
                <div className="model-backend-row" key={row.name}>
                  <span className="model-backend-name"><i className={"model-status-led " + row.state} />{row.name}</span>
                  <strong>{row.value}</strong>
                </div>
              ))}
            </div>
            <div className="model-runtime-note">
              <span>Runtime</span>
              <strong>{runtime?.version ?? "—"}</strong>
            </div>
          </section>

          <section className="model-panel format-panel">
            <div className="model-panel-heading">
              <div>
                <span className="model-section-kicker">SUPPORTED FORMATS</span>
                <strong>Model interfaces</strong>
              </div>
            </div>
            <div className="model-format-grid">
              <article>
                <b>LP</b>
                <div>
                  <strong>LP format</strong>
                  <span>Human-readable linear programming model.</span>
                </div>
              </article>
              <article>
                <b>MPS</b>
                <div>
                  <strong>MPS format</strong>
                  <span>Standard mathematical programming interchange format.</span>
                </div>
              </article>
            </div>
          </section>
        </aside>
      </div>

      <section className="model-panel model-pipeline-panel">
        <div className="model-panel-heading">
          <div>
            <span className="model-section-kicker">03 · SOLVER PIPELINE</span>
            <strong>Model lifecycle</strong>
          </div>
          <span className="model-pipeline-caption">AUTHORITATIVE C++ SOLVER</span>
        </div>
        <div className="model-pipeline" aria-label="Model processing pipeline">
          {pipelineItems.map((item, index) => (
            <div className="model-pipeline-step-wrap" key={item.key}>
              <div className={"model-pipeline-step " + item.state}>
                <span>{String(index + 1).padStart(2, "0")}</span>
                <strong>{item.label}</strong>
              </div>
              {index < pipelineItems.length - 1 && <i className="model-pipeline-connector" />}
            </div>
          ))}
        </div>
      </section>

      <section className="model-panel model-history-panel">
        <div className="model-panel-heading">
          <div>
            <span className="model-section-kicker">04 · RECENT MODELS</span>
            <strong>Workspace history</strong>
          </div>
          <span className="model-history-caption">{currentModel ? "1 CURRENT SESSION MODEL" : "NO STORED HISTORY"}</span>
        </div>
        {currentModel ? (
          <div className="model-history-row">
            <div className="model-history-icon">{currentModel.summary.format}</div>
            <div className="model-history-main">
              <strong>{currentModel.summary.name}</strong>
              <span>Current session · {formatBytes(currentModel.summary.sizeBytes)} · {currentModel.summary.rows.toLocaleString()} rows · {currentModel.summary.columns.toLocaleString()} columns</span>
            </div>
            <span className="model-ready-badge">READY</span>
          </div>
        ) : (
          <div className="model-history-empty">
            <span>∅</span>
            <div>
              <strong>No model history is stored</strong>
              <p>Uploaded models remain in the current session only. No historical models are displayed until the application provides persistent model storage.</p>
            </div>
          </div>
        )}
      </section>
    </section>
  );
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) return bytes + " B";
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
  return (bytes / (1024 * 1024)).toFixed(1) + " MB";
}
