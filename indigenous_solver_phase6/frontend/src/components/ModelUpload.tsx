import { useRef, useState } from "react";
import type { UploadedModel } from "../types/model";
import { analyzeModelFile } from "../services/modelParser";
import { inspectModel as inspectApiModel } from "../services/api";

interface Props { onModelReady: (model: UploadedModel | null) => void; }

export function ModelUpload({ onModelReady }: Props) {
  const inputRef = useRef<HTMLInputElement>(null);
  const [dragging, setDragging] = useState(false);
  const [error, setError] = useState("");
  const [inspecting, setInspecting] = useState(false);
  const [apiMessage, setApiMessage] = useState("");

  const acceptFiles = async (files: FileList | File[]) => {
    const file = files[0];
    if (!file) return;
    setError("");
    setApiMessage("");

    const analysis = await analyzeModelFile(file);
    const failedChecks = analysis.checks.filter((item) => item.status === "failed");
    if (!analysis.canSend || failedChecks.length > 0) {
      const reasons = failedChecks.map((item) => item.name + ": " + (item.reason ?? "validation failed"));
      setError(reasons.join(" · ") || "Browser validation failed.");
      onModelReady(null);
      return;
    }

    setInspecting(true);
    try {
      const remote = await inspectApiModel(file);
      onModelReady({
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
      });
      setApiMessage("Authoritative model dimensions received from the solver API.");
    } catch {
      onModelReady({
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
      });
      setApiMessage("Browser validation passed. Solver API unavailable; exact dimensions will be available when the API is connected.");
    } finally {
      setInspecting(false);
    }
  };

  return <div className={dragging ? "upload-zone dragging" : "upload-zone"}
    onDragOver={(event) => { event.preventDefault(); setDragging(true); }}
    onDragLeave={() => setDragging(false)}
    onDrop={(event) => { event.preventDefault(); setDragging(false); void acceptFiles(event.dataTransfer.files); }}>
    <div className="upload-icon">↑</div>
    <h3>Upload an LP or MPS model</h3>
    <p>Drag and drop your optimization model here, or choose a file from your computer.</p>
    <button className="primary-button" onClick={() => inputRef.current?.click()} disabled={inspecting}>{inspecting ? "Inspecting…" : "Choose model"}</button>
    <input ref={inputRef} hidden type="file" accept=".lp,.mps" onChange={(event) => event.target.files && void acceptFiles(event.target.files)} />
    <small>Supported: .LP and .MPS · Maximum size: 50 MB</small>
    {error && <div className="upload-error">{error}</div>}
    {apiMessage && <div className="model-message">{apiMessage}</div>}
  </div>;
}
