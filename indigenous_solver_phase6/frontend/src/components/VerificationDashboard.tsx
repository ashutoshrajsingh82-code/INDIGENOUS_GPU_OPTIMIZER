import { useCallback, useEffect, useState } from "react";
import { getRuntimeSnapshot, getVerificationSnapshot } from "../services/api";
import type { RuntimeSnapshot } from "../types/runtime";
import type { VerificationSnapshot, VerificationCheck } from "../types/verification";

function statusIcon(status: VerificationCheck["status"]) {
  if (status === "passed") return "✓";
  if (status === "failed") return "×";
  return "−";
}

function statusClass(status: VerificationCheck["status"]) {
  if (status === "passed") return "verification-pass";
  if (status === "failed") return "verification-fail";
  return "verification-muted";
}

function formatNumber(value: number | null | undefined, digits = 6) {
  if (value === null || value === undefined || !Number.isFinite(value)) return "-";
  return value.toFixed(digits);
}

function formatTimestamp(value: string | undefined) {
  if (!value) return "-";
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? value : date.toISOString();
}

function CheckRow({ check }: { check: VerificationCheck }) {
  const relErr = check.status === "passed" || check.status === "failed" ? "-" : "-";
  return (
    <tr>
      <td className={"verification-icon " + statusClass(check.status)}>{statusIcon(check.status)}</td>
      <td>{check.name}</td>
      <td className="verification-muted-text">{check.category}</td>
      <td className="verification-number">{check.durationMs == null ? "-" : formatNumber(check.durationMs, 3) + " ms"}</td>
      <td className="verification-number">{relErr}</td>
      <td className="verification-number">-</td>
    </tr>
  );
}

function SummaryCell({ label, value, tone = "normal" }: { label: string; value: string; tone?: "normal" | "pass" | "warn" | "fail" }) {
  return (
    <div className="verification-summary-cell">
      <span>{label}</span>
      <strong className={tone === "normal" ? "" : "verification-" + tone}>{value}</strong>
    </div>
  );
}

function EnvironmentRow({ label, value }: { label: string; value: string }) {
  return <div className="verification-env-row"><span>{label}</span><strong>{value}</strong></div>;
}

function KernelSection() {
  return (
    <section className="verification-section verification-gpu-section">
      <div className="verification-section-heading">
        <div>
          <span>GPU kernel timings</span>
          <small>GPU detected, but no kernel timing dataset was returned by the verification API.</small>
        </div>
      </div>
      <div className="verification-kernel-table-wrap">
        <table className="verification-table">
          <thead><tr><th>Kernel</th><th>Iterations</th><th>Min</th><th>Median</th><th>P95</th><th>CPU speedup</th></tr></thead>
          <tbody><tr><td>–</td><td>–</td><td>–</td><td>–</td><td>–</td><td>–</td></tr></tbody>
        </table>
      </div>
      <div className="verification-chart" aria-label="GPU kernel timing chart unavailable">
        <div className="verification-chart-axis"><span>timing</span><span>−</span></div>
        <div className="verification-chart-empty">-</div>
        <div className="verification-chart-axis"><span>iteration</span><span>−</span></div>
      </div>
    </section>
  );
}

export function VerificationDashboard() {
  const [snapshot, setSnapshot] = useState<VerificationSnapshot | null>(null);
  const [runtime, setRuntime] = useState<RuntimeSnapshot | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  const load = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const [verification, runtimeResult] = await Promise.all([
        getVerificationSnapshot(),
        getRuntimeSnapshot().catch(() => null),
      ]);
      setSnapshot(verification);
      setRuntime(runtimeResult);
    } catch (e) {
      setSnapshot(null);
      setRuntime(null);
      setError(e instanceof Error ? e.message : "Verification service unavailable.");
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void load();
  }, [load]);

  if (loading) {
    return <section className="verification-page"><div className="verification-loading">Loading verification data...</div></section>;
  }

  if (error || !snapshot) {
    return (
      <section className="verification-page">
        <header className="verification-topline">
          <span>solver / verification / run -</span>
          <button className="verification-rerun" onClick={() => void load()}>Re-run</button>
        </header>
        <div className="verification-title-row">
          <div><h2>Verification</h2><span>backend: {runtime?.executionBackend ?? "-"}</span></div>
        </div>
        <div className="verification-note">Verification data is unavailable: {error ?? "-"}</div>
      </section>
    );
  }

  const s = snapshot.summary;
  const backend = runtime?.executionBackend ?? "-";
  const gpuReady = s.gpuRuntimeReady === true || runtime?.cudaDeviceReady === true;
  const missingData = s.skipped > 0 || s.status === "degraded" || s.status === "unavailable" ||
    snapshot.checks.some((check) => check.status === "skipped" || check.status === "unavailable") ||
    [s.fallbackChecksPassed, s.regressionChecksPassed, s.ftranChecksPassed, s.btranChecksPassed, s.pricingChecksPassed, s.workspaceChecksPassed].some((v) => v === null);

  return (
    <section className="verification-page">
      <header className="verification-topline">
        <span>solver / verification / run -</span>
        <div className="verification-run-meta">
          <span>timestamp {formatTimestamp(s.lastVerifiedAt)}</span>
          <span>duration -</span>
          <span>git -</span>
          <button className="verification-rerun" onClick={() => void load()}>Re-run</button>
        </div>
      </header>

      <div className="verification-title-row">
        <div>
          <h2>Verification</h2>
          <span>backend: {backend}</span>
        </div>
      </div>

      <div className="verification-summary-strip">
        <SummaryCell label="passed" value={String(s.passed)} tone={s.passed > 0 ? "pass" : "normal"} />
        <SummaryCell label="failed" value={String(s.failed)} tone={s.failed > 0 ? "fail" : "normal"} />
        <SummaryCell label="not run" value={String(s.skipped)} tone={s.skipped > 0 ? "warn" : "normal"} />
        <SummaryCell label="max rel err" value="-" />
        <SummaryCell label="gpu status" value={gpuReady ? "ready" : "-"} tone={gpuReady ? "pass" : "normal"} />
      </div>

      {missingData && (
        <div className="verification-note">
          Some verification fields or checks were not returned by the solver API; missing values are shown as -.
        </div>
      )}

      <section className="verification-section">
        <div className="verification-section-heading">
          <div><span>Results</span><small>{snapshot.checks.length} check{snapshot.checks.length === 1 ? "" : "s"}</small></div>
        </div>
        <div className="verification-table-wrap">
          <table className="verification-table">
            <thead><tr><th>Status</th><th>Check</th><th>Area</th><th>Time</th><th>Rel err</th><th>Tolerance</th></tr></thead>
            <tbody>
              {snapshot.checks.length === 0
                ? <tr><td className="verification-muted-text" colSpan={6}>-</td></tr>
                : snapshot.checks.map((check) => <CheckRow key={check.id} check={check} />)}
            </tbody>
          </table>
        </div>
      </section>

      <section className="verification-section">
        <div className="verification-section-heading"><div><span>Environment</span><small>Values returned by the solver API</small></div></div>
        <div className="verification-env">
          <EnvironmentRow label="solver version" value={runtime?.version ?? "-"} />
          <EnvironmentRow label="precision" value="-" />
          <EnvironmentRow label="seed" value="-" />
          <EnvironmentRow label="reference impl" value="-" />
          <EnvironmentRow label="OS" value="-" />
          <EnvironmentRow label="python" value="-" />
          <EnvironmentRow label="numpy" value="-" />
          <EnvironmentRow label="cuda" value="-" />
        </div>
      </section>

      {gpuReady && <KernelSection />}
    </section>
  );
}
