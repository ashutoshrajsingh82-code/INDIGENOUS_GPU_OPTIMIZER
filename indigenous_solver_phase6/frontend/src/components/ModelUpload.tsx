import { useRef, useState } from "react";
import type { UploadedModel } from "../types/model";
import { inspectModel as inspectApiModel } from "../services/api";
import { analyzeModelFile, type ModelFileAnalysis, type CheckResult } from "../services/modelParser";

interface Props { onModelReady: (model: UploadedModel | null) => void; }
const statusIcon = (status: CheckResult["status"]) => status === "passed" ? "✓" : status === "failed" ? "×" : "◷";
const formatBytes = (bytes: number | null) => bytes === null ? "-" : bytes < 1024 ? bytes + " B" : bytes < 1048576 ? (bytes / 1024).toFixed(1) + " KB" : (bytes / 1048576).toFixed(1) + " MB";
const shortHash = (hash: string) => hash.length > 24 ? hash.slice(0, 12) + "…" + hash.slice(-8) : hash;

export function ModelUpload({ onModelReady }: Props) {
  const inputRef = useRef<HTMLInputElement>(null);
  const [dragging, setDragging] = useState(false);
  const [analysis, setAnalysis] = useState<ModelFileAnalysis | null>(null);
  const [sending, setSending] = useState(false);
  const [apiError, setApiError] = useState("");
  const [apiPassed, setApiPassed] = useState(false);

  const acceptFile = async (file?: File) => {
    if (!file) return;
    setSending(false); setApiError(""); setApiPassed(false); onModelReady(null);
    setAnalysis(await analyzeModelFile(file));
  };
  const sendToSolver = async () => {
    if (!analysis?.canSend) return;
    setSending(true); setApiError(""); setApiPassed(false);
    try {
      const remote = await inspectApiModel(analysis.file);
      setApiPassed(true);
      const density = remote.rows > 0 && remote.columns > 0
        ? ((remote.nonzeros / (remote.rows * remote.columns)) * 100).toFixed(6) + "%"
        : null;
      setAnalysis((current) => current ? {
        ...current,
        remoteStats: {
          rows: remote.rows,
          columns: remote.columns,
          nonzeros: remote.nonzeros,
          density,
          source: "solver api",
        },
      } : current);
      onModelReady({ file: analysis.file, modelId: remote.modelId, summary: remote });
    } catch (error) {
      setApiError(error instanceof Error ? error.message : "Solver API inspection failed.");
      onModelReady(null);
    } finally { setSending(false); }
  };
  const clear = () => {
    setAnalysis(null); setApiError(""); setApiPassed(false); onModelReady(null);
    if (inputRef.current) inputRef.current.value = "";
  };
  const openPicker = () => inputRef.current?.click();

  return <section className="model-workspace">
    <div className="model-breadcrumb"><span>solver / models / upload</span><span>limit 50 MB | .lp .mps</span></div>
    <div className="model-title-row"><div><h2>Model workspace</h2><p>Validate a model locally, inspect its structure, then send it to the solver API.</p></div></div>

    <div className={"model-drop-strip" + (dragging ? " dragging" : "")} tabIndex={0} role="button"
      aria-label="Choose a model file" onClick={openPicker}
      onKeyDown={(event) => { if (event.key === "Enter") { event.preventDefault(); openPicker(); } }}
      onDragOver={(event) => { event.preventDefault(); setDragging(true); }}
      onDragLeave={() => setDragging(false)}
      onDrop={(event) => { event.preventDefault(); setDragging(false); void acceptFile(event.dataTransfer.files[0]); }}>
      <span>Drop a file here to replace the current model</span>
      <button type="button" className="model-browse-button" onClick={(event) => { event.stopPropagation(); openPicker(); }}>Browse</button>
      <input ref={inputRef} hidden type="file" accept=".lp,.mps" onChange={(event) => void acceptFile(event.target.files?.[0])} />
    </div>

    {analysis ? <>
      <div className="model-info-grid">
        <section className="model-info-block">
          <div className="model-section-label">FILE</div>
          <div className="model-data-row"><span>Name</span><code>{analysis.file.name}</code></div>
          <div className="model-data-row"><span>Detected format</span><code>{analysis.formatLabel}</code></div>
          <div className="model-data-row"><span>Size</span><code>{formatBytes(analysis.file.size)}</code></div>
          <div className="model-data-row"><span>SHA-256</span><code title={analysis.sha256}>{shortHash(analysis.sha256)} <button type="button" className="hash-copy" onClick={() => void navigator.clipboard?.writeText(analysis.sha256)}>copy</button></code></div>
          <div className="model-data-row"><span>Added time</span><code>{analysis.addedAt}</code></div>
        </section>
        <section className="model-info-block">
          <div className="model-section-label">MODEL STATISTICS</div>
          <div className="model-data-row"><span>Rows</span><code>{analysis.remoteStats.rows ?? "-"}</code></div>
          <div className="model-data-row"><span>Columns</span><code>{analysis.remoteStats.columns ?? "-"}</code></div>
          <div className="model-data-row"><span>Nonzeros</span><code>{analysis.remoteStats.nonzeros ?? "-"}</code></div>
          <div className="model-data-row"><span>Density</span><code>{analysis.remoteStats.density ?? "-"}</code></div>
          <div className="model-source">{analysis.remoteStats.source}</div>
        </section>
      </div>

      <section className="model-section-block">
        <div className="model-section-label">CHECKS</div>
        <div className="model-check-table">
          {analysis.checks.map((check) => <div className="model-check-row" key={check.name}>
            <span className={"model-check-icon " + check.status}>{statusIcon(check.status)}</span>
            <span className="model-check-text"><strong>{check.name}</strong>{check.reason && <small>{check.reason}</small>}</span>
            <code>{check.where}</code>
          </div>)}
          <div className="model-check-row">
            <span className={"model-check-icon " + (apiPassed ? "passed" : apiError ? "failed" : "pending")}>{apiPassed ? "✓" : apiError ? "×" : "◷"}</span>
            <span className="model-check-text"><strong>solver parse</strong>{apiError && <small>{apiError}</small>}</span>
            <code>solver api</code>
          </div>
        </div>
      </section>

      <section className="model-section-block">
        <div className="model-section-heading"><div className="model-section-label">PREVIEW</div><code>first {analysis.previewLines.length} of {analysis.lineCount} lines</code></div>
        <pre className="model-preview">{analysis.previewLines.map((line, index) => <span key={index}><b>{String(index + 1).padStart(4, " ")}</b>{line}{"\n"}</span>)}</pre>
      </section>

      <div className="model-actions">
        <button type="button" className="model-action-primary" disabled={!analysis.canSend || sending} onClick={() => void sendToSolver()}>{sending ? "Sending…" : "Send to solver"}</button>
        <button type="button" className="model-action-secondary" onClick={clear}>Clear</button>
      </div>
    </> : <div className="model-empty-state">Select an LP or MPS file to calculate the local checks, hash, line count and preview.</div>}
  </section>;
}
