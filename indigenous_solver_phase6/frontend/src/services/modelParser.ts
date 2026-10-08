import type { ModelFormat, ModelSummary } from "../types/model";
const MAX_MODEL_SIZE = 50 * 1024 * 1024;

export interface CheckResult { name: string; status: "passed" | "failed" | "pending"; where: "browser" | "solver api"; reason?: string; }
export interface ModelFileAnalysis {
  file: File; format: ModelFormat; formatLabel: "LP" | "MPS fixed" | "MPS free"; sha256: string; addedAt: string;
  lineCount: number; previewLines: string[]; checks: CheckResult[]; canSend: boolean;
  remoteStats: { rows: number | null; columns: number | null; nonzeros: number | null; density: string | null; source: string; };
}
export function detectModelFormat(fileName: string): ModelFormat | null {
  const extension = fileName.toLowerCase().split(".").pop();
  return extension === "lp" ? "LP" : extension === "mps" ? "MPS" : null;
}
function clean(line: string) { const t = line.trim(); return t === "" || t.startsWith("*") || t.startsWith("$") ? "" : line; }
function firstToken(line: string) { return clean(line).trim().split(/\s+/)[0]?.toUpperCase() ?? ""; }
function mpsSections(lines: string[]) {
  const sections = new Set<string>();
  for (const line of lines) {
    const token = firstToken(line);
    if (["NAME","ROWS","COLUMNS","RHS","BOUNDS","RANGES","ENDATA"].includes(token)) sections.add(token);
  }
  return sections;
}
function mpsFormat(lines: string[]): "MPS fixed" | "MPS free" {
  let current = "";
  for (const line of lines) {
    const cleanLine = clean(line);
    const token = firstToken(line);
    if (["ROWS","COLUMNS","RHS","BOUNDS","RANGES"].includes(token)) { current = token; continue; }
    if (current === "COLUMNS" && cleanLine.trim()) return cleanLine[0] === " " || cleanLine[0] === "\t" ? "MPS fixed" : "MPS free";
  }
  return "MPS fixed";
}
function hasLpSections(lines: string[]) {
  const text = lines.join("\n");
  return {
    objective: /(^|\n)\s*(minimize|minimization|maximize|maximization)\b/i.test(text),
    constraints: /(^|\n)\s*(subject\s+to|such\s+that|st|s\.t\.)\b/i.test(text),
    bounds: /(^|\n)\s*bounds\b/i.test(text),
    end: /(^|\n)\s*end\s*$/i.test(text),
  };
}
function check(name: string, ok: boolean, reason: string): CheckResult {
  return { name, status: ok ? "passed" : "failed", where: "browser", ...(ok ? {} : { reason }) };
}
async function sha256(file: File) {
  const digest = await crypto.subtle.digest("SHA-256", await file.arrayBuffer());
  return Array.from(new Uint8Array(digest)).map((v) => v.toString(16).padStart(2, "0")).join("");
}
export async function analyzeModelFile(file: File): Promise<ModelFileAnalysis> {
  const format = detectModelFormat(file.name);
  const extensionOk = format !== null;
  const sizeOk = file.size <= MAX_MODEL_SIZE;
  let text = ""; let utf8Ok = true;
  if (file.size > 0) try { text = new TextDecoder("utf-8", { fatal: true }).decode(await file.arrayBuffer()); } catch { utf8Ok = false; }
  const lines = text.split(/\r?\n/);
  const nonEmpty = file.size > 0 && text.trim().length > 0;
  const formatLabel = format === "MPS" ? mpsFormat(lines) : "LP";
  const checks: CheckResult[] = [
    check("extension", extensionOk, "unsupported extension; expected .lp or .mps"),
    check("size limit", sizeOk, "file exceeds the 50 MB limit"),
    check("non-empty and valid UTF-8", nonEmpty && utf8Ok, !nonEmpty ? "file is empty" : "file is not valid UTF-8"),
  ];
  if (format === "MPS") {
    const sections = mpsSections(lines);
    const required = ["NAME","ROWS","COLUMNS","RHS","BOUNDS","ENDATA"];
    const missing = required.find((section) => !sections.has(section));
    const ok = !missing;
    checks.push(check("expected sections found", ok, missing ? missing + " section missing" : ""));
  } else if (format === "LP") {
    const lp = hasLpSections(lines);
    const missing = !lp.objective ? "objective" : !lp.constraints ? "constraints" : !lp.bounds ? "bounds" : !lp.end ? "end" : "";
    checks.push(check("expected sections found", !missing, missing ? missing + " section missing" : ""));
  } else checks.push({ name: "expected sections found", status: "pending", where: "browser" });
  const sha = await sha256(file);
  const allPassed = checks.every((item) => item.status === "passed");
  return {
    file, format: format ?? "LP", formatLabel, sha256: sha, addedAt: new Date().toLocaleString(),
    lineCount: text === "" ? 0 : lines.length, previewLines: lines.slice(0, 10), checks, canSend: allPassed,
    remoteStats: { rows: null, columns: null, nonzeros: null, density: null, source: "awaiting solver parse" },
  };
}
export function inspectModel(file: File): ModelSummary {
  const format = detectModelFormat(file.name);
  return { name: file.name, format: format ?? "LP", sizeBytes: file.size, rows: 0, columns: 0, nonzeros: 0, status: "Rejected", message: format ? "Use analyzeModelFile for browser validation." : "Unsupported format. Select an .lp or .mps file." };
}
