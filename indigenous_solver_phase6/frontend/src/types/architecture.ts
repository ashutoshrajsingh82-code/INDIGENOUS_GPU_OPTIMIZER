export type ArchitectureNodeKind = "application" | "solver" | "backend" | "workspace" | "validation";
export type ArchitectureStatus = "implemented" | "runtime" | "planned";

export interface ArchitectureNode {
  id: string;
  title: string;
  phase: string;
  kind: ArchitectureNodeKind;
  status: ArchitectureStatus;
  description: string;
  technologies: string[];
  children?: string[];
}

export interface ArchitectureEdge {
  from: string;
  to: string;
  label: string;
}

export interface ArchitectureSnapshot {
  version: string;
  nodes: ArchitectureNode[];
  edges: ArchitectureEdge[];
}
