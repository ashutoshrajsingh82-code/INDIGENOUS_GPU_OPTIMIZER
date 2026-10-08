import { useCallback, useEffect, useState } from "react";
import { getVerificationSnapshot } from "../services/api";
import type { VerificationSnapshot, VerificationCheck } from "../types/verification";

function CheckRow({check}:{check:VerificationCheck}) {
 const tone=check.status==="passed"?"ok":check.status==="failed"?"error":"muted";
 return <div className="verification-check"><div><strong>{check.name}</strong><small>{check.category} · {check.message}</small></div><span className={"verification-status "+tone}>{check.status.toUpperCase()}</span></div>;
}
function Coverage({label,value}:{label:string;value:number|null}) {
 return <div className="status-row"><span>{label}</span><strong>{value===null?"NOT REPORTED":value}</strong></div>;
}
function StatusLine({label,value,ok=false}:{label:string;value:string;ok?:boolean}) {
 return <div className="status-row"><span>{label}</span><strong><i className={ok?"status-dot":"status-dot muted"}/>{value}</strong></div>;
}
export function VerificationDashboard() {
 const [snapshot,setSnapshot]=useState<VerificationSnapshot|null>(null);
 const [error,setError]=useState<string|null>(null);
 const [loading,setLoading]=useState(true);
 const load=useCallback(async()=>{setLoading(true);setError(null);try{setSnapshot(await getVerificationSnapshot());}catch(e){setSnapshot(null);setError(e instanceof Error?e.message:"Verification service unavailable.");}finally{setLoading(false);}},[]);
 useEffect(()=>{void load();},[load]);
 if(loading)return <section className="dashboard"><article className="panel empty-state">Loading authoritative verification results…</article></section>;
 if(error||!snapshot)return <section className="dashboard"><div className="hero"><div><span className="eyebrow">PHASE 6.10 · VERIFICATION</span><h2>Verification dashboard</h2><p>The C++ verification service is not currently reachable.</p></div><div className="hero-badge"><span className="status-dot muted"/> Unavailable</div></div><article className="panel unavailable-panel"><h3>Verification data unavailable</h3><p>{error??"No verification snapshot was returned."}</p><button className="secondary-button" onClick={()=>void load()}>Retry verification</button></article></section>;
 const s=snapshot.summary; const percent=s.total>0?Math.round(s.passed/s.total*100):0;
 return <section className="dashboard">
  <div className="hero"><div><span className="eyebrow">PHASE 6.10 · VERIFICATION</span><h2>Verification dashboard</h2><p>Authoritative regression, numerical, backend, and workspace verification reported by the solver service.</p></div><div className="hero-badge"><span className="status-dot"/> {s.status.toUpperCase()}</div></div>
  <div className="metric-grid">
   <article className="metric-card"><span>Checks</span><strong>{s.passed} / {s.total}</strong><small>{percent}% passed</small></article>
   <article className="metric-card"><span>Failed</span><strong>{s.failed}</strong><small>{s.skipped} skipped</small></article>
   <article className="metric-card"><span>Numerical stability</span><strong>{s.numericalStable===null?"UNKNOWN":s.numericalStable?"STABLE":"FAILED"}</strong><small>Solver-reported validation</small></article>
   <article className="metric-card"><span>GPU runtime</span><strong>{s.gpuRuntimeReady===null?"UNKNOWN":s.gpuRuntimeReady?"READY":"NOT READY"}</strong><small>Actual runtime capability</small></article>
  </div>
  <div className="content-grid">
   <article className="panel"><div className="panel-heading"><div><span className="eyebrow">VALIDATION AREAS</span><h3>Subsystem coverage</h3></div></div>
    <Coverage label="Certificates" value={s.certificatePassCount}/><Coverage label="Fallback" value={s.fallbackChecksPassed}/><Coverage label="Regression" value={s.regressionChecksPassed}/><Coverage label="FTRAN" value={s.ftranChecksPassed}/><Coverage label="BTRAN" value={s.btranChecksPassed}/><Coverage label="Pricing" value={s.pricingChecksPassed}/><Coverage label="Workspace" value={s.workspaceChecksPassed}/>
   </article>
   <article className="panel"><div className="panel-heading"><div><span className="eyebrow">RUN STATUS</span><h3>Verification state</h3></div><span className="badge">{s.status.toUpperCase()}</span></div>
    <StatusLine label="Passed checks" value={String(s.passed)} ok={s.failed===0}/><StatusLine label="Failed checks" value={String(s.failed)} ok={s.failed===0}/><StatusLine label="Skipped checks" value={String(s.skipped)}/><StatusLine label="Last verified" value={s.lastVerifiedAt?new Date(s.lastVerifiedAt).toLocaleString():"Not reported"}/>{s.message&&<p className="model-message">{s.message}</p>}
   </article>
  </div>
  <article className="panel activity"><div className="panel-heading"><div><span className="eyebrow">CHECK DETAILS</span><h3>Verification checks</h3></div><button className="secondary-button" onClick={()=>void load()}>Refresh</button></div><div className="verification-list">{snapshot.checks.map(check=><CheckRow key={check.id} check={check}/>)}</div></article>
 </section>;
}
