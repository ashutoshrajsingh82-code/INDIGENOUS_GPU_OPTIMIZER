export type ModelFormat = "LP" | "MPS";
export type ModelStatus = "Ready" | "Rejected";

export interface ModelSummary {
  name: string;
  format: ModelFormat;
  sizeBytes: number;
  rows: number;
  columns: number;
  nonzeros: number;
  status: ModelStatus;
  message: string;
}

export interface UploadedModel {
  file: File;
  summary: ModelSummary;
  modelId?: string;
}
