import { useState } from "react";
import { navigation, runtimeStatus } from "./types/solver";

function App() {
  const [active, setActive] = useState("dashboard");

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand">
          <div className="brand-mark">IG</div>
          <div>
            <div className="brand-name">INDIGENOUS</div>
            <div className="brand-subtitle">GPU OPTIMIZER</div>
          </div>
        </div>

        <div className="nav-section-title">SOLVER</div>
        <nav>
          {navigation.map((item) => (
            <button
              key={item.id}
              className={active === item.id ? "nav-item active" : "nav-item"}
              onClick={() => setActive(item.id)}
            >
              <span>{item.label}</span>
              <small>{item.phase}</small>
            </button>
          ))}
        </nav>

        <div className="sidebar-footer">
          <span className="status-dot" />
          CPU fallback enabled
        </div>
      </aside>

      <main className="main">
        <header className="topbar">
          <div>
            <div className="eyebrow">PHASE 6.1 • WEB FOUNDATION</div>
            <h1>Solver Control Center</h1>
          </div>
          <div className="runtime-pill">
            <span className="status-dot" />
            <span>{runtimeStatus.backend}</span>
            <span className="runtime-muted">GPU unavailable</span>
          </div>
        </header>

        <section className="hero">
          <div>
            <span className="tag">FOUNDATION READY</span>
            <h2>Indigenous GPU Optimizer</h2>
            <p>
              A production-oriented web interface for the Phase 2–5 solver
              pipeline. The UI keeps CPU fallback and CUDA runtime state
              explicit.
            </p>
          </div>
          <div className="hero-version">
            <span>WEB STACK</span>
            <strong>React + Vite + TypeScript</strong>
          </div>
        </section>

        <section className="metrics">
          <Metric title="Execution" value="CPU" detail="Current runtime backend" />
          <Metric title="CUDA" value="NOT READY" detail="No NVIDIA device detected" />
          <Metric title="Production" value="READY" detail="Phase 5 pipeline validated" />
          <Metric title="Regression" value="22 / 22" detail="Latest Phase 5/4 test suite" />
        </section>

        <section className="content-grid">
          <article className="panel">
            <div className="panel-heading">
              <div>
                <span className="eyebrow">CURRENT MODULE</span>
                <h3>{navigation.find((item) => item.id === active)?.label ?? "Dashboard"}</h3>
              </div>
              <span className="phase-badge">{navigation.find((item) => item.id === active)?.phase}</span>
            </div>
            <div className="placeholder">
              <div className="placeholder-icon">◆</div>
              <strong>Module foundation is ready</strong>
              <p>
                This navigation surface is intentionally a foundation in 6.1.
                Functional solver modules will be introduced phase by phase.
              </p>
            </div>
          </article>

          <article className="panel">
            <div className="panel-heading">
              <div>
                <span className="eyebrow">RUNTIME</span>
                <h3>Backend status</h3>
              </div>
              <span className="live-badge">LIVE MODEL</span>
            </div>
            <StatusRow label="Execution backend" value="CPU" />
            <StatusRow label="CUDA compiled" value="NO" />
            <StatusRow label="CUDA device" value="NOT AVAILABLE" />
            <StatusRow label="CPU fallback" value="ENABLED" />
          </article>
        </section>

        <footer className="footer">
          <span>INDIGENOUS GPU OPTIMIZER</span>
          <span>Phase 6.1 React/Vite foundation</span>
        </footer>
      </main>
    </div>
  );
}

function Metric({ title, value, detail }: { title: string; value: string; detail: string }) {
  return (
    <article className="metric">
      <span>{title}</span>
      <strong>{value}</strong>
      <small>{detail}</small>
    </article>
  );
}

function StatusRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="status-row">
      <span>{label}</span>
      <strong>{value}</strong>
    </div>
  );
}

export default App;
