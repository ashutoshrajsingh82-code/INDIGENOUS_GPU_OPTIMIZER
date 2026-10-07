import { useCallback, useEffect, useState } from "react";
import { getRuntimeSnapshot } from "../services/api";
import type { RuntimeSnapshot } from "../types/runtime";

const unavailable: RuntimeSnapshot = {
  status: "unavailable", executionBackend: "UNKNOWN", cudaCompiled: false,
  cudaDeviceReady: false, gpuRuntimeActive: false, basisGpuActive: false, pricingGpuActive: false,
  ftranCalls: 0, btranCalls: 0, pricingCalls: 0, updateCalls: 0,
  workspaceAllocations: 0, workspaceReuses: 0, workspacePersistent: false,
  asyncBackend: "UNKNOWN", batchBackend: "UNKNOWN", adaptiveGpuEligible: false,
  numericalStable: false, numericalChecks: 0, numericalFailures: 0, fallbackCount: 0,
  bottleneck: "UNKNOWN", bottleneckShare: 0,
  message: "Solver API is not available. No GPU state has been inferred locally.",
};

export function GpuMonitor() {
  const [snapshot, setSnapshot] = useState<RuntimeSnapshot | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  const refresh = useCallback(async () => {
    setRefreshing(true);
    try { setSnapshot(await getRuntimeSnapshot()); setError(null); }
    catch (err) { setSnapshot(unavailable); setError(err instanceof Error ? err.message : "Runtime endpoint unavailable."); }
    finally { setRefreshing(false); }
  }, []);

  useEffect(() => { void refresh(); }, [refresh]);
  const data = snapshot ?? unavailable;
  const gpuReady = data.cudaCompiled && data.cudaDeviceReady;
  const active = data.gpuRuntimeActive;

  return <section className="dashboard">
    <div className="hero">
      <div><span className="eyebrow">PHASE 6.8 · RUNTIME OBSERVABILITY</span><h2>GPU monitoring</h2><p>Observe CUDA readiness, solver backend activity, persistent workspace usage, and numerical fallback state from the authoritative C++ runtime.</p></div>
      <div className={active ? "hero-badge" : "hero-badge muted-badge"}><span className={active ? "status-dot" : "status-dot muted"} />{active ? "GPU active" : gpuReady ? "GPU ready" : "CPU execution"}</div>
    </div>
    {error && <article className="panel monitor-notice"><strong>Live runtime unavailable</strong><span>{data.message ?? error}</span><button className="secondary-button" onClick={() => void refresh()} disabled={refreshing}>{refreshing ? "Refreshing…" : "Retry"}</button></article>}
    <div className="metric-grid">
      <Metric label="Execution" value={data.executionBackend} detail={active ? "GPU runtime active" : "CPU authoritative"} />
      <Metric label="CUDA compiled" value={data.cudaCompiled ? "YES" : "NO"} detail={data.cudaDeviceReady ? "Device ready" : "Device not ready"} />
      <Metric label="GPU operations" value={String(data.ftranCalls + data.btranCalls + data.pricingCalls)} detail="FTRAN + BTRAN + pricing" />
      <Metric label="Fallbacks" value={String(data.fallbackCount)} detail={data.numericalFailures + " numerical failures"} />
    </div>
    <div className="content-grid">
      <article className="panel"><Heading eyebrow="HARDWARE" title="CUDA readiness" />
        <Status label="CUDA compiled" value={data.cudaCompiled ? "YES" : "NO"} ok={data.cudaCompiled} />
        <Status label="CUDA device ready" value={data.cudaDeviceReady ? "YES" : "NO"} ok={data.cudaDeviceReady} />
        <Status label="GPU runtime active" value={data.gpuRuntimeActive ? "YES" : "NO"} ok={data.gpuRuntimeActive} />
        <Status label="Basis GPU active" value={data.basisGpuActive ? "YES" : "NO"} ok={data.basisGpuActive} />
        <Status label="Pricing GPU active" value={data.pricingGpuActive ? "YES" : "NO"} ok={data.pricingGpuActive} />
        <Status label="Adaptive GPU eligible" value={data.adaptiveGpuEligible ? "YES" : "NO"} ok={data.adaptiveGpuEligible} />
      </article>
      <article className="panel"><Heading eyebrow="SOLVER ACTIVITY" title="Operation counters" />
        <Status label="FTRAN calls" value={String(data.ftranCalls)} /><Status label="BTRAN calls" value={String(data.btranCalls)} />
        <Status label="Pricing calls" value={String(data.pricingCalls)} /><Status label="Basis updates" value={String(data.updateCalls)} />
        <Status label="Workspace allocations" value={String(data.workspaceAllocations)} /><Status label="Workspace reuses" value={String(data.workspaceReuses)} />
      </article>
    </div>
    <div className="content-grid">
      <article className="panel"><Heading eyebrow="WORKSPACE" title="Execution infrastructure" />
        <Status label="Persistent workspace" value={data.workspacePersistent ? "YES" : "NO"} ok={data.workspacePersistent} />
        <Status label="Async backend" value={data.asyncBackend} /><Status label="Batch backend" value={data.batchBackend} />
      </article>
      <article className="panel"><Heading eyebrow="NUMERICAL SAFETY" title="Stability & fallback" />
        <Status label="Numerically stable" value={data.numericalStable ? "YES" : "NO"} ok={data.numericalStable} />
        <Status label="Validation checks" value={String(data.numericalChecks)} /><Status label="Numerical failures" value={String(data.numericalFailures)} />
        <Status label="Fallback count" value={String(data.fallbackCount)} /><Status label="Profile bottleneck" value={data.bottleneck + " (" + data.bottleneckShare.toFixed(1) + "%)"} />
      </article>
    </div>
    <article className="panel activity"><Heading eyebrow="TRUTHFUL RUNTIME STATE" title="Backend interpretation" />
      <p className="model-message">{data.message ?? (active ? "CUDA execution is active according to the C++ runtime." : "GPU execution is not active. The UI does not infer GPU activity from browser hardware or benchmark labels.")}</p>
      <div className="monitor-actions"><span className="badge">{data.status.toUpperCase()}</span><span className="badge">{data.version ? "Runtime " + data.version : "Runtime version unknown"}</span><button className="secondary-button" onClick={() => void refresh()} disabled={refreshing}>{refreshing ? "Refreshing…" : "Refresh runtime"}</button></div>
    </article>
  </section>;
}
function Metric({ label, value, detail }: { label: string; value: string; detail: string }) { return <article className="metric-card"><span>{label}</span><strong>{value}</strong><small>{detail}</small></article>; }
function Heading({ eyebrow, title }: { eyebrow: string; title: string }) { return <div className="panel-heading"><div><span className="eyebrow">{eyebrow}</span><h3>{title}</h3></div></div>; }
function Status({ label, value, ok = false }: { label: string; value: string; ok?: boolean }) { return <div className="status-row"><span>{label}</span><strong><i className={ok ? "status-dot" : "status-dot muted"} />{value}</strong></div>; }
