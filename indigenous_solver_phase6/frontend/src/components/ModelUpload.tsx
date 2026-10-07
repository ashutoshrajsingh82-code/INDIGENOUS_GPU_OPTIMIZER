import { useRef, useState } from "react";
import type { UploadedModel } from "../types/model";
import { inspectModel } from "../services/modelParser";

interface Props { onModelReady: (model: UploadedModel | null) => void; }

export function ModelUpload({ onModelReady }: Props) {
  const inputRef = useRef<HTMLInputElement>(null);
  const [dragging, setDragging] = useState(false);
  const [error, setError] = useState("");

  const acceptFiles = (files: FileList | File[]) => {
    const file = files[0];
    if (!file) return;
    const summary = inspectModel(file);
    setError(summary.status === "Rejected" ? summary.message : "");
    onModelReady(summary.status === "Ready" ? { file, summary } : null);
  };

  return <div className={dragging ? "upload-zone dragging" : "upload-zone"}
    onDragOver={(event) => { event.preventDefault(); setDragging(true); }}
    onDragLeave={() => setDragging(false)}
    onDrop={(event) => { event.preventDefault(); setDragging(false); acceptFiles(event.dataTransfer.files); }}>
    <div className="upload-icon">↑</div>
    <h3>Upload an LP or MPS model</h3>
    <p>Drag and drop your optimization model here, or choose a file from your computer.</p>
    <button className="primary-button" onClick={() => inputRef.current?.click()}>Choose model</button>
    <input ref={inputRef} hidden type="file" accept=".lp,.mps" onChange={(event) => event.target.files && acceptFiles(event.target.files)} />
    <small>Supported: .LP and .MPS · Maximum size: 50 MB</small>
    {error && <div className="upload-error">{error}</div>}
  </div>;
}
