import type { ModelFormat, ModelSummary } from "../types/model";

const MAX_MODEL_SIZE = 50 * 1024 * 1024;

export function detectModelFormat(fileName: string): ModelFormat | null {
  const extension = fileName.toLowerCase().split(".").pop();
  return extension === "lp" ? "LP" : extension === "mps" ? "MPS" : null;
}

export function inspectModel(file: File): ModelSummary {
  const format = detectModelFormat(file.name);
  if (!format) {
    return { name: file.name, format: "LP", sizeBytes: file.size, rows: 0, columns: 0, nonzeros: 0, status: "Rejected", message: "Unsupported format. Select an .lp or .mps file." };
  }
  if (file.size === 0) {
    return { name: file.name, format, sizeBytes: 0, rows: 0, columns: 0, nonzeros: 0, status: "Rejected", message: "The selected model is empty." };
  }
  if (file.size > MAX_MODEL_SIZE) {
    return { name: file.name, format, sizeBytes: file.size, rows: 0, columns: 0, nonzeros: 0, status: "Rejected", message: "Model exceeds the 50 MB web upload limit." };
  }
  return { name: file.name, format, sizeBytes: file.size, rows: 0, columns: 0, nonzeros: 0, status: "Ready", message: "File accepted. Solver-side parsing will provide exact model statistics." };
}
