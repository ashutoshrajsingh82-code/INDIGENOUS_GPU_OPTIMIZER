import { useEffect, useState } from "react";
import type { BackendPolicy, SolverConfiguration } from "../types/solverConfig";
import { defaultSolverConfiguration } from "../types/solverConfig";
import { validateSolverConfiguration } from "../services/solverConfig";

interface Props {
  value: SolverConfiguration;
  onChange: (value: SolverConfiguration) => void;
  onSave: () => void;
}

export function SolverConfigurationPanel({ value, onChange, onSave }: Props) {
  const [draft, setDraft] = useState(value);
  const validation = validateSolverConfiguration(draft);

  useEffect(() => setDraft(value), [value]);

  const update = <K extends keyof SolverConfiguration>(key: K, next: SolverConfiguration[K]) => {
    setDraft((current) => ({ ...current, [key]: next }));
  };

  const commit = () => {
    if (validation.valid) {
      onChange(draft);
      onSave();
    }
  };

  return <div className="config-layout">
    <article className="panel config-panel">
      <div className="panel-heading"><div><span className="eyebrow">SOLVER METHOD</span><h3>Optimization engine</h3></div><span className="badge">C++</span></div>
      <label className="field"><span>Method</span><select value={draft.method} onChange={(e) => update("method", e.target.value as SolverConfiguration["method"])}><option value="revised-simplex">Revised Simplex</option></select></label>
      <label className="field"><span>Backend policy</span><select value={draft.backendPolicy} onChange={(e) => update("backendPolicy", e.target.value as BackendPolicy)}><option value="auto">Auto — adaptive CPU/GPU</option><option value="cpu">CPU only</option><option value="cuda">CUDA preferred / required</option></select></label>
      <p className="field-help">Auto lets the Phase 5 adaptive backend choose CPU or CUDA when a CUDA device is actually available.</p>
    </article>

    <article className="panel config-panel">
      <div className="panel-heading"><div><span className="eyebrow">ITERATION CONTROL</span><h3>Convergence settings</h3></div></div>
      <label className="field"><span>Maximum iterations</span><input type="number" min="1" max="10000000" value={draft.maxIterations} onChange={(e) => update("maxIterations", Number(e.target.value))} /></label>
      <label className="field"><span>Relative tolerance</span><input type="number" step="any" min="0" value={draft.relativeTolerance} onChange={(e) => update("relativeTolerance", Number(e.target.value))} /></label>
      <label className="field"><span>Absolute tolerance</span><input type="number" step="any" min="0" value={draft.absoluteTolerance} onChange={(e) => update("absoluteTolerance", Number(e.target.value))} /></label>
      <label className="field"><span>Pivot tolerance</span><input type="number" step="any" min="0" value={draft.pivotTolerance} onChange={(e) => update("pivotTolerance", Number(e.target.value))} /></label>
    </article>

    <article className="panel config-panel">
      <div className="panel-heading"><div><span className="eyebrow">SAFETY & OBSERVABILITY</span><h3>Runtime safeguards</h3></div></div>
      <Toggle label="CPU fallback" description="Allow automatic fallback when CUDA is unavailable or numerically unsafe." checked={draft.enableCpuFallback} onChange={(v) => update("enableCpuFallback", v)} />
      <Toggle label="Numerical validation" description="Validate solver outputs and residuals before accepting GPU results." checked={draft.enableNumericalValidation} onChange={(v) => update("enableNumericalValidation", v)} />
      <Toggle label="Performance profiling" description="Collect Phase 5 stage timing and bottleneck information." checked={draft.enableProfiling} onChange={(v) => update("enableProfiling", v)} />
    </article>

    <article className="panel config-summary">
      <div><span className="eyebrow">CONFIGURATION STATUS</span><h3>{validation.valid ? "Ready to solve" : "Configuration needs attention"}</h3><p>{validation.valid ? "These settings are ready for the future solver API. The browser does not execute the C++ solver." : "Fix the highlighted validation rules before saving."}</p></div>
      <div className="config-actions"><button className="secondary-button" onClick={() => setDraft(defaultSolverConfiguration)}>Reset defaults</button><button className="primary-button" disabled={!validation.valid} onClick={commit}>Save configuration</button></div>
      {!validation.valid && <div className="validation-errors">{Object.values(validation.errors).filter(Boolean).map((error) => <div key={error}>{error}</div>)}</div>}
    </article>
  </div>;
}

function Toggle({ label, description, checked, onChange }: { label: string; description: string; checked: boolean; onChange: (value: boolean) => void }) {
  return <label className="toggle-row"><span><strong>{label}</strong><small>{description}</small></span><input type="checkbox" checked={checked} onChange={(e) => onChange(e.target.checked)} /></label>;
}
