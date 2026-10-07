import { useRef, useState } from "react";
import type { UploadedModel } from "../types/model";
import { inspectModel as inspectLocalModel } from "../services/modelParser";
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
    const localSummary = inspectLocalModel(file);
    setError(localSummary.status === "Rejected" ? localSummary.message : "");
    setApiMessage("");
    if (localSummary.status === "Rejected") {
      onModelReady(null);
      return;
    }

    setInspecting(true);
    try {
      const remote = await inspectApiModel(file);
      onModelReady({
        file,
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
      onModelReady({ file, summary: localSummary });
      setApiMessage("Solver API unavailable. Local file validation passed; exact dimensions will be available when the API is connected.");
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
