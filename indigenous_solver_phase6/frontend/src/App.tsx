import { useEffect, useMemo, useState } from "react";
import { ModelUpload } from "./components/ModelUpload";
import type { UploadedModel } from "./types/model";
import type { SolverConfiguration } from "./types/solverConfig";
import { defaultSolverConfiguration } from "./types/solverConfig";
import { SolverConfigurationPanel } from "./components/SolverConfigurationPanel";
import { LiveSolver } from "./components/LiveSolver";
import { ResultsExplorer } from "./components/ResultsExplorer";
import { GpuMonitor } from "./components/GpuMonitor";
import { BenchmarkDashboard } from "./components/BenchmarkDashboard";
import { VerificationDashboard } from "./components/VerificationDashboard";
import { ArchitectureVisualization } from "./components/ArchitectureVisualization";
import { ReportsDashboard } from "./components/ReportsDashboard";
import { getRuntimeSnapshot } from "./services/api";
import type { RuntimeSnapshot } from "./types/runtime";
import { navigation, runtimeStatus, type SolverNavigationItem } from "./types/solver";
import "./styles.css";

function App() {
  const [active, setActive] = useState<SolverNavigationItem["id"]>("dashboard");
  const [uploadedModel, setUploadedModel] = useState<UploadedModel | null>(null);
  const [solverConfiguration, setSolverConfiguration] = useState<SolverConfiguration>(defaultSolverConfiguration);
  const [lastJobId, setLastJobId] = useState<string | null>(null);
  const activeItem = useMemo(
    () => navigation.find((item) => item.id === active) ?? navigation[0],
    [active],
  );

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand">
          <div className="brand-mark">IG</div>
          <div><strong>Indigenous Solver</strong><span>GPU Optimizer</span></div>
        </div>
        <nav>
          {navigation.map((item) => (
            <button key={item.id} className={active === item.id ? "nav-item active" : "nav-item"} onClick={() => setActive(item.id)}>
              <span className="nav-icon">{item.icon}</span><span>{item.label}</span>
            </button>
          ))}
        </nav>
        <div className="sidebar-footer"><span className="status-dot" /> System online</div>
      </aside>

      <main className="main-content">
        <header className="topbar">
          <div><span className="eyebrow">PHASE 6 · WEB CONTROL CENTER</span><h1>{activeItem.label}</h1></div>
          <div className="runtime-pill"><span className="status-dot" /> {runtimeStatus.backend} · CPU FALLBACK</div>
        </header>

        {active === "dashboard" ? <Dashboard onNavigate={setActive} /> : active === "models" ? <ModelsView model={uploadedModel} onModelReady={setUploadedModel} /> : active === "solve" ? <SolveConfigurationView config={solverConfiguration} onChange={setSolverConfiguration} /> : active === "live" ? <LiveSolver model={uploadedModel} config={solverConfiguration} onJobCreated={setLastJobId} onSolveComplete={(jobId) => { setLastJobId(jobId); setActive("results"); }} /> : active === "results" ? <ResultsExplorer jobId={lastJobId} /> : active === "gpu" ? <GpuMonitor /> : active === "benchmarks" ? <BenchmarkDashboard /> : active === "verification" ? <VerificationDashboard /> : active === "architecture" ? <ArchitectureVisualization /> : active === "reports" ? <ReportsDashboard jobId={lastJobId} /> : <Placeholder label={activeItem.label} />}

        <footer className="footer">Indigenous GPU Optimizer · React/Vite foundation · Solver logic remains authoritative in C++</footer>
      </main>
    </div>
  );
}

function Dashboard({ onNavigate }: { onNavigate: (id: SolverNavigationItem["id"]) => void }) {
  const [runtime, setRuntime] = useState<RuntimeSnapshot | null>(null);

  useEffect(() => {
    let mounted = true;
    const refresh = async () => {
      try {
        const snapshot = await getRuntimeSnapshot();
        if (mounted) setRuntime(snapshot);
      } catch {
        if (mounted) setRuntime(null);
      }
    };
    void refresh();
    const timer = window.setInterval(() => void refresh(), 2000);
    return () => { mounted = false; window.clearInterval(timer); };
  }, []);

  const execution = runtime?.executionBackend ?? "UNKNOWN";
  const cudaReady = runtime?.cudaDeviceReady ?? false;
  const productionReady = Boolean(runtime && runtime.status !== "unavailable" && runtime.numericalStable !== false);
  const gpuActive = runtime?.gpuRuntimeActive ?? false;
  const totalCalls = (runtime?.pricingCalls ?? 0) + (runtime?.ftranCalls ?? 0) + (runtime?.btranCalls ?? 0);

  const stages = [
    { key: "01", name: "Model Intake", detail: "LP / MPS", state: "READY", icon: "⬡" },
    { key: "02", name: "Presolve", detail: "Validation", state: "READY", icon: "◇" },
    { key: "03", name: "Basis Engine", detail: "FTRAN / BTRAN", state: runtime ? "ACTIVE" : "STANDBY", icon: "◈" },
    { key: "04", name: "Pricing", detail: "Sparse pricing", state: runtime?.pricingGpuActive ? "GPU" : "CPU", icon: "✦" },
    { key: "05", name: "Simplex Core", detail: "Revised simplex", state: "READY", icon: "◎" },
    { key: "06", name: "Verification", detail: "Numerical checks", state: runtime?.numericalStable === false ? "ALERT" : "PASS", icon: "✓" },
  ];

  return (
    <section className="control-tower">
      <div className="tower-header">
        <div className="tower-title">
          <span className="eyebrow">INDIGENOUS GPU OPTIMIZER · OPERATIONS CENTER</span>
          <h2>Solver Digital Twin <span>•</span> <em>Live</em></h2>
        </div>
        <div className="tower-header-actions">
          <span className="tower-chip"><i className="status-dot" /> Runtime Connected</span>
          <span className="tower-chip">Refresh 2s</span>
          <button className="tower-icon-button" onClick={() => onNavigate("live")}>↗</button>
        </div>
      </div>

      <div className="tower-layout">
        <aside className="tower-controls">
          <div className="tower-section-title">Simulation Controls <span>Live</span></div>
          <div className="control-card">
            <ControlMeter icon="⌁" label="Execution" value={execution} active />
            <ControlMeter icon="◌" label="GPU Runtime" value={gpuActive ? "ACTIVE" : "CPU"} active={gpuActive} />
            <ControlMeter icon="◈" label="Numerical" value={runtime?.numericalStable === false ? "ALERT" : "STABLE"} active={runtime?.numericalStable !== false} />
            <ControlMeter icon="⌁" label="FTRAN / BTRAN" value={totalCalls.toLocaleString()} active />
            <ControlMeter icon="◇" label="Pricing Calls" value={(runtime?.pricingCalls ?? 0).toLocaleString()} active />
            <ControlMeter icon="✓" label="Fallback" value={runtime?.fallbackCount ? String(runtime.fallbackCount) : "READY"} active />
          </div>
          <div className="control-card compact">
            <div className="tower-mini-row"><span>CUDA</span><strong>{cudaReady ? "READY" : "NOT AVAILABLE"}</strong></div>
            <div className="tower-mini-row"><span>Workspace</span><strong>{runtime?.workspacePersistent ? "PERSISTENT" : "CPU"}</strong></div>
            <div className="tower-mini-row"><span>Adaptive</span><strong>{runtime?.adaptiveGpuEligible ? "ELIGIBLE" : "CPU MODE"}</strong></div>
            <div className="tower-mini-row"><span>Bottleneck</span><strong>{runtime?.bottleneck ?? "UNKNOWN"}</strong></div>
          </div>
          <button className="tower-reset" onClick={() => onNavigate("models")}>↻ Upload / Change Model</button>
          <div className="tower-quick-links">
            <button onClick={() => onNavigate("live")}>Live Solver <span>→</span></button>
            <button onClick={() => onNavigate("gpu")}>GPU Monitor <span>→</span></button>
            <button onClick={() => onNavigate("reports")}>Reports <span>→</span></button>
          </div>
        </aside>

        <main className="twin-stage">
          <div className="twin-stage-top">
            <div>
              <span className="eyebrow">PHASE 2 → PHASE 5 PIPELINE</span>
              <h3>Revised Simplex · Complete Solver Journey</h3>
            </div>
            <div className="drag-hint">⌖ Click a node to inspect</div>
          </div>

          <div className="twin-canvas">
            <div className="grid-floor" />
            <div className="twin-glow glow-one" />
            <div className="twin-glow glow-two" />
            <div className="flow-line line-a" />
            <div className="flow-line line-b" />
            <div className="flow-line line-c" />
            <div className="flow-line line-d" />
            <div className="flow-line line-e" />

            {stages.map((stage, index) => (
              <button className={"twin-node node-" + index} key={stage.key} onClick={() => onNavigate(index === 0 ? "models" : index === 4 ? "live" : index === 5 ? "verification" : "architecture")}>
                <span className="node-badge">{stage.key}</span>
                <span className="node-icon">{stage.icon}</span>
                <strong>{stage.name}</strong>
                <small>{stage.detail}</small>
                <i className={stage.state === "ALERT" ? "node-state alert" : stage.state === "CPU" ? "node-state cpu" : "node-state"}>{stage.state}</i>
              </button>
            ))}

            <div className="twin-core">
              <span>OPTIMIZATION</span>
              <strong>INDIGENOUS<br />SOLVER</strong>
              <small>{productionReady ? "PIPELINE READY" : "CHECKING..."}</small>
            </div>
          </div>

          <div className="twin-legend">
            <span><i className="legend-dot ready" /> Ready</span>
            <span><i className="legend-dot cpu" /> CPU execution</span>
            <span><i className="legend-dot gpu" /> CUDA active</span>
            <span><i className="legend-dot alert" /> Numerical alert</span>
          </div>
        </main>

        <aside className="tower-kpis">
          <KpiCard title="Overall Solver Readiness" value={productionReady ? "READY" : "CHECK"} delta={productionReady ? "+1.0" : "—"} kind="green" detail="Pipeline preflight" />
          <KpiCard title="Execution Backend" value={execution} delta={gpuActive ? "CUDA" : "CPU fallback"} kind={gpuActive ? "blue" : "cyan"} detail="Authoritative runtime" />
          <KpiCard title="Numerical Stability" value={runtime?.numericalStable === false ? "ALERT" : "100 %"} delta={runtime?.numericalFailures ? "-" + runtime.numericalFailures : "0 failures"} kind={runtime?.numericalStable === false ? "amber" : "green"} detail="Validation checks" />
          <KpiCard title="Workspace" value={runtime?.workspacePersistent ? "PERSISTENT" : "CPU"} delta={(runtime?.workspaceReuses ?? 0) + " reuses"} kind="purple" detail="Phase 5 workspace" />
          <KpiCard title="Solver Calls" value={totalCalls.toLocaleString()} delta={(runtime?.updateCalls ?? 0) + " updates"} kind="blue" detail="Pricing + basis operations" />
        </aside>
      </div>

      <div className="tower-bottom">
        <button className="tower-action" onClick={() => onNavigate("models")}><span>⬡</span><div><strong>Model Intake</strong><small>Upload LP / MPS</small></div><b>→</b></button>
        <button className="tower-action" onClick={() => onNavigate("solve")}><span>⚙</span><div><strong>Simulation Setup</strong><small>Configure solver policy</small></div><b>→</b></button>
        <button className="tower-action" onClick={() => onNavigate("benchmarks")}><span>⌁</span><div><strong>Performance Lab</strong><small>Benchmark & compare</small></div><b>→</b></button>
        <button className="tower-action" onClick={() => onNavigate("architecture")}><span>◈</span><div><strong>Architecture</strong><small>Trace execution graph</small></div><b>→</b></button>
      </div>
    </section>
  );
}

function ControlMeter({ icon, label, value, active }: { icon: string; label: string; value: string; active: boolean }) {
  return <div className="control-meter"><span className="control-icon">{icon}</span><div><small>{label}</small><strong>{value}</strong></div><i className={active ? "meter-led on" : "meter-led"} /></div>;
}

function KpiCard({ title, value, delta, kind, detail }: { title: string; value: string; delta: string; kind: "green" | "blue" | "cyan" | "amber" | "purple"; detail: string }) {
  return <article className={"tower-kpi " + kind}><div className="kpi-title">{title}</div><strong>{value}</strong><div className="kpi-delta">↗ {delta}</div><small>{detail}</small><div className="kpi-bars">{Array.from({ length: 8 }, (_, i) => <i key={i} />)}</div></article>;
}

function StatusRow({ label, value, ok = false }: { label: string; value: string; ok?: boolean }) {
  return <div className="status-row"><span>{label}</span><strong><i className={ok ? "status-dot" : "status-dot muted"} />{value}</strong></div>;
}

function Capability({ name, state }: { name: string; state: string }) {
  return <div className="capability"><span className="cap-icon">✓</span><div><strong>{name}</strong><small>{state}</small></div></div>;
}

function Action({ title, text, target }: { title: string; text: string; target: SolverNavigationItem["id"] }) {
  return <button className="action-card" onClick={() => window.dispatchEvent(new CustomEvent("solver-nav", { detail: target }))}><strong>{title} <span>→</span></strong><small>{text}</small></button>;
}

function Placeholder({ label }: { label: string }) {
  return <section className="placeholder panel"><span className="eyebrow">PHASE 6 ROADMAP</span><h2>{label}</h2><p>This module is reserved for the next Phase 6 implementation. The dashboard foundation and navigation are ready.</p></section>;
}

export default App;

function ModelsView({ model, onModelReady }: { model: UploadedModel | null; onModelReady: (model: UploadedModel | null) => void }) {
  return <section className="dashboard">
    <div className="hero"><div><span className="eyebrow">PHASE 6.3 · MODEL INGESTION</span><h2>Model workspace</h2><p>Upload an LP or MPS model and validate it before the solver API receives it.</p></div><div className="hero-badge"><span className="status-dot" /> Parser boundary ready</div></div>
    <div className="content-grid"><article className="panel"><div className="panel-heading"><div><span className="eyebrow">MODEL INPUT</span><h3>Select optimization model</h3></div></div><ModelUpload onModelReady={onModelReady} /></article>
    <article className="panel"><div className="panel-heading"><div><span className="eyebrow">MODEL SUMMARY</span><h3>{model ? model.summary.name : "No model selected"}</h3></div>{model && <span className="badge">{model.summary.format}</span>}</div>
    {model ? <><StatusRow label="Upload status" value={model.summary.status.toUpperCase()} ok={model.summary.status === "Ready"} /><StatusRow label="File size" value={formatBytes(model.summary.sizeBytes)} /><StatusRow label="Rows / columns" value={model.summary.rows + " / " + model.summary.columns} /><StatusRow label="Nonzeros" value={String(model.summary.nonzeros)} /><p className="model-message">{model.summary.message}</p></> : <p className="empty-state">Upload a valid .lp or .mps file to populate the model summary.</p>}
    </article></div>
    {model && <article className="panel activity"><div className="panel-heading"><div><span className="eyebrow">NEXT STEP</span><h3>Model accepted</h3></div></div><p className="model-message">The browser has validated the file type and size. Exact rows, columns, and nonzero counts will come from the C++ solver API in Phase 6.5.</p></article>}
  </section>;
}

function formatBytes(bytes: number) { if (bytes < 1024) return bytes + " B"; if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"; return (bytes / (1024 * 1024)).toFixed(1) + " MB"; }

function SolveConfigurationView({ config, onChange }: { config: SolverConfiguration; onChange: (config: SolverConfiguration) => void }) {
  const [saved, setSaved] = useState(false);
  return <section className="dashboard">
    <div className="hero"><div><span className="eyebrow">PHASE 6.4 · SOLVER CONFIGURATION</span><h2>Configure a solve</h2><p>Set solver, convergence, backend, fallback, and profiling policies before the C++ API launches a solve.</p></div><div className="hero-badge"><span className="status-dot" /> {saved ? "Configuration saved" : "Draft configuration"}</div></div>
    <SolverConfigurationPanel value={config} onChange={(next) => { onChange(next); setSaved(true); }} onSave={() => setSaved(true)} />
    <article className="panel activity"><div className="panel-heading"><div><span className="eyebrow">CURRENT POLICY</span><h3>Execution policy</h3></div></div><div className="action-grid"><Capability name="Backend" state={config.backendPolicy.toUpperCase()} /><Capability name="CPU fallback" state={config.enableCpuFallback ? "ENABLED" : "DISABLED"} /><Capability name="Numerical validation" state={config.enableNumericalValidation ? "ENABLED" : "DISABLED"} /><Capability name="Profiling" state={config.enableProfiling ? "ENABLED" : "DISABLED"} /></div></article>
  </section>;
}
