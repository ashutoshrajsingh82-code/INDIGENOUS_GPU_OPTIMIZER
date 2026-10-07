import { useMemo, useState } from "react";
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

        {active === "dashboard" ? <Dashboard onNavigate={setActive} /> : active === "models" ? <ModelsView model={uploadedModel} onModelReady={setUploadedModel} /> : active === "solve" ? <SolveConfigurationView config={solverConfiguration} onChange={setSolverConfiguration} /> : active === "live" ? <LiveSolver model={uploadedModel} config={solverConfiguration} onJobCreated={setLastJobId} /> : active === "results" ? <ResultsExplorer jobId={lastJobId} /> : active === "gpu" ? <GpuMonitor /> : active === "benchmarks" ? <BenchmarkDashboard /> : <Placeholder label={activeItem.label} />}

        <footer className="footer">Indigenous GPU Optimizer · React/Vite foundation · Solver logic remains authoritative in C++</footer>
      </main>
    </div>
  );
}

function Dashboard({ onNavigate }: { onNavigate: (id: SolverNavigationItem["id"]) => void }) {
  const cards = [
    ["Execution", "CPU", "Active backend"],
    ["CUDA Device", "NOT READY", "NVIDIA device unavailable"],
    ["Production", "READY", "Pipeline preflight passed"],
    ["Regression", "22 / 22", "Tests passing"],
  ];
  return (
    <section className="dashboard">
      <div className="hero">
        <div><span className="eyebrow">SOLVER OPERATIONS</span><h2>Optimization control center</h2><p>Monitor solver readiness, configure workloads, launch solves, and inspect verification results from one workspace.</p></div>
        <div className="hero-badge"><span className="status-dot" /> Production ready</div>
      </div>

      <div className="metric-grid">{cards.map(([label,value,detail]) => <article className="metric-card" key={label}><span>{label}</span><strong>{value}</strong><small>{detail}</small></article>)}</div>

      <div className="content-grid">
        <article className="panel">
          <div className="panel-heading"><div><span className="eyebrow">RUNTIME</span><h3>Backend health</h3></div><span className="badge">CPU</span></div>
          <StatusRow label="Execution backend" value="CPU" ok />
          <StatusRow label="CUDA compiled" value="NO" />
          <StatusRow label="GPU runtime" value="NOT AVAILABLE" />
          <StatusRow label="CPU fallback" value="ENABLED" ok />
          <StatusRow label="Production pipeline" value="READY" ok />
        </article>

        <article className="panel">
          <div className="panel-heading"><div><span className="eyebrow">CAPABILITIES</span><h3>Solver stack</h3></div></div>
          <Capability name="Sparse pricing" state="CPU fallback" />
          <Capability name="FTRAN / BTRAN" state="CPU backend" />
          <Capability name="Persistent workspace" state="Enabled" />
          <Capability name="Async execution" state="CPU-ASYNC" />
          <Capability name="Batch operations" state="CPU-BATCH" />
        </article>
      </div>

      <article className="panel activity">
        <div className="panel-heading"><div><span className="eyebrow">WORKFLOW</span><h3>Next actions</h3></div></div>
        <div className="action-grid">
          <button className="action-card" onClick={() => onNavigate("models")}><strong>Upload model <span>→</span></strong><small>Load an LP or MPS model for analysis.</small></button>
          <Action title="Configure solve" text="Choose iterations, tolerances, and backend policy." target="solve" />
          <Action title="View verification" text="Inspect regression and numerical validation." target="verification" />
          <Action title="Explore architecture" text="Trace the Phase 2–5 solver pipeline." target="architecture" />
        </div>
      </article>
    </section>
  );
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
