import { useCallback, useEffect, useMemo, useState, type CSSProperties } from "react";
import { getRuntimeSnapshot } from "../services/api";
import type { RuntimeSnapshot } from "../types/runtime";

type HistoryPoint = {
  time: string;
  ftran: number;
  btran: number;
  pricing: number;
  updates: number;
  allocations: number;
  reuses: number;
  checks: number;
  failures: number;
  fallbacks: number;
};

const unavailable: RuntimeSnapshot = {
  status: "unavailable",
  executionBackend: "UNKNOWN",
  cudaCompiled: false,
  cudaDeviceReady: false,
  gpuRuntimeActive: false,
  basisGpuActive: false,
  pricingGpuActive: false,
  ftranCalls: 0,
  btranCalls: 0,
  pricingCalls: 0,
  updateCalls: 0,
  workspaceAllocations: 0,
  workspaceReuses: 0,
  workspacePersistent: false,
  asyncBackend: "UNKNOWN",
  batchBackend: "UNKNOWN",
  adaptiveGpuEligible: false,
  numericalStable: false,
  numericalChecks: 0,
  numericalFailures: 0,
  fallbackCount: 0,
  bottleneck: "UNKNOWN",
  bottleneckShare: 0,
  message: "Solver API is not available. No GPU state has been inferred locally.",
};

export function GpuMonitor() {
  const [snapshot, setSnapshot] = useState<RuntimeSnapshot | null>(null);
  const [history, setHistory] = useState<HistoryPoint[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [refreshing, setRefreshing] = useState(false);

  const refresh = useCallback(async () => {
    setRefreshing(true);
    try {
      const next = await getRuntimeSnapshot();
      setSnapshot(next);
      setError(null);
      setHistory((current) => [...current, toHistoryPoint(next)].slice(-24));
    } catch (err) {
      setSnapshot(unavailable);
      setError(err instanceof Error ? err.message : "Runtime endpoint unavailable.");
    } finally {
      setRefreshing(false);
    }
  }, []);

  useEffect(() => {
    void refresh();
    const timer = window.setInterval(() => void refresh(), 10000);
    return () => window.clearInterval(timer);
  }, [refresh]);

  const data = snapshot ?? unavailable;
  const gpuReady = data.cudaCompiled && data.cudaDeviceReady;
  const active = data.gpuRuntimeActive;
  const hardwareAvailable = data.cudaCompiled || data.cudaDeviceReady || active;
  const totalCalls = data.ftranCalls + data.btranCalls + data.pricingCalls;

  const headline = active ? "NVIDIA GPU" : data.executionBackend === "CPU" ? "CPU fallback" : "GPU telemetry";
  const statusText = active ? "CUDA RUNTIME ACTIVE" : gpuReady ? "CUDA READY · IDLE" : data.executionBackend === "CPU" ? "CPU EXECUTION" : "TELEMETRY UNAVAILABLE";

  const gauges = [
    { label: "CUDA Runtime", value: gpuReady ? 100 : 0, display: gpuReady ? "READY" : "N/A", tone: gpuReady ? "good" as const : "neutral" as const },
    { label: "GPU Runtime", value: active ? 100 : 0, display: active ? "ACTIVE" : "N/A", tone: active ? "good" as const : "neutral" as const },
    { label: "Basis GPU", value: data.basisGpuActive ? 100 : 0, display: data.basisGpuActive ? "ACTIVE" : "N/A", tone: data.basisGpuActive ? "good" as const : "neutral" as const },
    { label: "Pricing GPU", value: data.pricingGpuActive ? 100 : 0, display: data.pricingGpuActive ? "ACTIVE" : "N/A", tone: data.pricingGpuActive ? "good" as const : "neutral" as const },
  ];

  const chartData = useMemo(() => ({
    calls: history.map((p) => p.ftran + p.btran + p.pricing),
    workspace: history.map((p) => p.allocations + p.reuses),
    numerical: history.map((p) => p.checks),
    fallback: history.map((p) => p.fallbacks),
  }), [history]);

  return (
    <section className="gpu-monitor">
      <div className="gpu-toolbar">
        <div className="gpu-breadcrumb"><span className="gpu-menu">▦</span><strong>Operations</strong><b>/</b><span>GPU Monitoring</span><i>☆</i></div>
        <div className="gpu-toolbar-actions">
          <span className="gpu-live"><i className="status-dot" /> {statusText}</span>
          <span>Refresh 10s</span>
          <button onClick={() => void refresh()} disabled={refreshing} title="Refresh runtime">↻</button>
        </div>
      </div>

      <div className="gpu-filterbar">
        <div className="gpu-filter"><small>DEVICE</small><strong>{headline}</strong><span>⌄</span></div>
        <div className="gpu-filter"><small>BACKEND</small><strong>{data.executionBackend}</strong><span>⌄</span></div>
        <div className="gpu-filter"><small>WINDOW</small><strong>LIVE SESSION</strong><span>⌄</span></div>
        <div className="gpu-filter gpu-filter-note"><small>AUTHORITATIVE SOURCE</small><strong>C++ /runtime</strong></div>
      </div>

      {error && <div className="gpu-notice"><strong>Runtime telemetry unavailable</strong><span>{data.message ?? error}</span><button onClick={() => void refresh()} disabled={refreshing}>{refreshing ? "Refreshing…" : "Retry"}</button></div>}

      <div className="gpu-overview-grid">
        <article className="gpu-panel gpu-identity">
          <div className="gpu-panel-title"><span>DEVICE</span><b>{hardwareAvailable ? "DETECTED" : "NO CUDA DEVICE"}</b></div>
          <div className="gpu-device-name">{hardwareAvailable ? "CUDA DEVICE" : "Intel / CPU"}</div>
          <div className="gpu-device-sub">{data.executionBackend === "CPU" ? "Intel Iris Xe · CPU fallback path" : data.message ?? "Device telemetry supplied by solver runtime"}</div>
          <div className="gpu-mini-grid">
            <div><small>CUDA compiled</small><strong>{data.cudaCompiled ? "YES" : "NO"}</strong></div>
            <div><small>Device ready</small><strong>{data.cudaDeviceReady ? "YES" : "NO"}</strong></div>
            <div><small>GPU active</small><strong>{data.gpuRuntimeActive ? "YES" : "NO"}</strong></div>
            <div><small>CPU fallback</small><strong>{data.executionBackend === "CPU" ? "ENABLED" : "—"}</strong></div>
          </div>
        </article>

        {gauges.map((gauge) => <Gauge key={gauge.label} {...gauge} />)}
      </div>

      <div className="gpu-hardware-grid">
        <InfoTile label="P-State" value="N/A" detail="Hardware telemetry not exposed" />
        <InfoTile label="GPU Utilization" value="N/A" detail="No CUDA device" />
        <InfoTile label="Power Draw" value="N/A" detail="No CUDA device" />
        <InfoTile label="Temperature" value="N/A" detail="No CUDA device" />
        <InfoTile label="Memory Utilization" value="N/A" detail="No CUDA device" />
        <InfoTile label="GPU Clock" value="N/A" detail="No CUDA device" />
        <InfoTile label="Memory Clock" value="N/A" detail="No CUDA device" />
        <InfoTile label="Fan Speed" value="N/A" detail="No CUDA device" />
      </div>

      <div className="gpu-two-column">
        <article className="gpu-panel gpu-status-panel">
          <PanelTitle eyebrow="SOLVER TELEMETRY" title="Runtime counters" />
          <TelemetryRow label="FTRAN calls" value={data.ftranCalls} />
          <TelemetryRow label="BTRAN calls" value={data.btranCalls} />
          <TelemetryRow label="Pricing calls" value={data.pricingCalls} />
          <TelemetryRow label="Basis updates" value={data.updateCalls} />
          <TelemetryRow label="Workspace allocations" value={data.workspaceAllocations} />
          <TelemetryRow label="Workspace reuses" value={data.workspaceReuses} />
          <TelemetryRow label="Numerical checks" value={data.numericalChecks} />
          <TelemetryRow label="Fallback count" value={data.fallbackCount} />
          <TelemetryRow label="Total solver calls" value={totalCalls} />
        </article>
        <article className="gpu-panel gpu-status-panel">
          <PanelTitle eyebrow="BACKEND STATE" title="Execution status" />
          <StateRow label="Execution backend" value={data.executionBackend} good={data.executionBackend === "CUDA"} />
          <StateRow label="Workspace" value={data.workspacePersistent ? "PERSISTENT" : "CPU / NON-PERSISTENT"} good={data.workspacePersistent} />
          <StateRow label="Async backend" value={data.asyncBackend} />
          <StateRow label="Batch backend" value={data.batchBackend} />
          <StateRow label="Adaptive GPU" value={data.adaptiveGpuEligible ? "ELIGIBLE" : "CPU MODE"} good={data.adaptiveGpuEligible} />
          <StateRow label="Numerical stability" value={data.numericalStable ? "STABLE" : "NOT VERIFIED"} good={data.numericalStable} />
          <StateRow label="Profile bottleneck" value={`${data.bottleneck} · ${data.bottleneckShare.toFixed(1)}%`} />
        </article>
      </div>

      <div className="gpu-chart-grid">
        <TelemetryChart title="Solver Operations" subtitle="FTRAN + BTRAN + Pricing calls" values={chartData.calls} history={history} />
        <TelemetryChart title="Workspace Activity" subtitle="Allocations + persistent reuses" values={chartData.workspace} history={history} />
        <TelemetryChart title="Numerical Validation" subtitle="Checks performed during this session" values={chartData.numerical} history={history} />
        <TelemetryChart title="CPU Fallbacks" subtitle="Authoritative fallback counter" values={chartData.fallback} history={history} />
      </div>

      <div className="gpu-footer-state">
        <div><span className="eyebrow">TRUTHFUL GPU OBSERVABILITY</span><strong>{statusText}</strong><p>{data.message ?? (active ? "CUDA execution is active according to the native runtime." : "GPU hardware telemetry is unavailable on this machine. The page does not synthesize utilization, temperature, power, clocks, or memory values.")}</p></div>
        <div className="gpu-footer-actions"><span className="gpu-status-badge">{data.status.toUpperCase()}</span><span className="gpu-status-badge">{data.version ?? "Runtime version unknown"}</span><button onClick={() => void refresh()} disabled={refreshing}>{refreshing ? "Refreshing…" : "Refresh now"}</button></div>
      </div>
    </section>
  );
}

function toHistoryPoint(data: RuntimeSnapshot): HistoryPoint {
  return {
    time: new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" }),
    ftran: data.ftranCalls,
    btran: data.btranCalls,
    pricing: data.pricingCalls,
    updates: data.updateCalls,
    allocations: data.workspaceAllocations,
    reuses: data.workspaceReuses,
    checks: data.numericalChecks,
    failures: data.numericalFailures,
    fallbacks: data.fallbackCount,
  };
}

function Gauge({ label, value, display, tone }: { label: string; value: number; display: string; tone: "good" | "neutral" }) {
  return <article className={`gpu-panel gpu-gauge ${tone}`}>
    <div className="gpu-panel-title"><span>{label}</span><b>{value}%</b></div>
    <div className="gpu-gauge-body" style={{ "--gauge-value": `${value * 2.7}deg` } as CSSProperties}>
      <div className="gpu-gauge-arc"><div><strong>{display}</strong><small>{value ? "ACTIVE" : "NOT AVAILABLE"}</small></div></div>
    </div>
  </article>;
}

function InfoTile({ label, value, detail }: { label: string; value: string; detail: string }) {
  return <article className="gpu-info-tile"><div className="gpu-info-title"><span>{label}</span><i>i</i></div><strong>{value}</strong><small>{detail}</small></article>;
}

function PanelTitle({ eyebrow, title }: { eyebrow: string; title: string }) {
  return <div className="gpu-panel-title gpu-section-title"><div><span>{eyebrow}</span><strong>{title}</strong></div></div>;
}

function TelemetryRow({ label, value }: { label: string; value: number }) {
  return <div className="gpu-telemetry-row"><span>{label}</span><strong>{value.toLocaleString()}</strong></div>;
}

function StateRow({ label, value, good = false }: { label: string; value: string; good?: boolean }) {
  return <div className="gpu-telemetry-row"><span>{label}</span><strong className={good ? "good" : "muted"}><i className={good ? "status-dot" : "status-dot muted"} />{value}</strong></div>;
}

function TelemetryChart({ title, subtitle, values, history }: { title: string; subtitle: string; values: number[]; history: HistoryPoint[] }) {
  const width = 320;
  const height = 100;
  const max = Math.max(1, ...values);
  const points = values.length === 1
    ? `0,${height - 12} ${width},${height - Math.max(12, (values[0] / max) * 76)}`
    : values.map((value, index) => {
      const x = (index / Math.max(1, values.length - 1)) * width;
      const y = height - 12 - (value / max) * 76;
      return `${x},${y}`;
    }).join(" ");

  const lastValue = values.length ? values[values.length - 1] : 0;

  return <article className="gpu-panel gpu-chart-panel">
    <div className="gpu-chart-heading"><div><span>{title}</span><small>{subtitle}</small></div><strong>{lastValue.toLocaleString()}</strong></div>
    {values.length ? <div className="gpu-chart"><div className="gpu-chart-gridlines"><i /><i /><i /></div><svg viewBox={`0 0 ${width} ${height}`} preserveAspectRatio="none"><polyline points={points} fill="none" stroke="currentColor" strokeWidth="2" vectorEffect="non-scaling-stroke" /></svg></div> : <div className="gpu-chart-empty">Collecting runtime samples…</div>}
    <div className="gpu-chart-meta"><span>{history[0]?.time ?? "—"}</span><span>{history.length ? `${history.length} samples · 10s interval` : "No samples"}</span><span>{history.length ? history[history.length - 1].time : "—"}</span></div>
  </article>;
}
