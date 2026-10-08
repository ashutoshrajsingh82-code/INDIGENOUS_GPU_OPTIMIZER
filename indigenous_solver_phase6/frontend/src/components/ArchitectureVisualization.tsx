import { useState } from "react";
import type { ArchitectureNode, ArchitectureNodeKind } from "../types/architecture";

const nodes: ArchitectureNode[] = [
 {id:"web",title:"React / Vite Control Center",phase:"6.1–6.12",kind:"application",status:"runtime",description:"Web interface for model ingestion, solver control, live progress, results, monitoring, verification, architecture and reports.",technologies:["React","Vite","TypeScript"]},
 {id:"api",title:"Solver API Boundary",phase:"6.5",kind:"application",status:"implemented",description:"Typed HTTP boundary between the browser and authoritative C++ solver services.",technologies:["HTTP","JSON","Typed API"]},
 {id:"production",title:"ProductionSolver",phase:"5.11",kind:"solver",status:"implemented",description:"Production integration layer coordinating model solving, pipeline preflight, results and numerical safety.",technologies:["C++","Revised Simplex"]},
 {id:"pipeline",title:"Unified GPU Solver Pipeline",phase:"5.1–5.12",kind:"solver",status:"implemented",description:"Coordinates pricing, FTRAN, BTRAN, adaptive selection, asynchronous execution, batching, profiling and fallback.",technologies:["Pipeline","Adaptive Backend","Async","Batch"]},
 {id:"pricing",title:"Sparse Pricing",phase:"3",kind:"backend",status:"implemented",description:"Reduced-cost pricing with CUDA acceleration when compiled and available, otherwise CPU fallback.",technologies:["CSC","CUDA","CPU fallback"]},
 {id:"basis",title:"Basis Solver",phase:"4.1–4.12",kind:"backend",status:"implemented",description:"Basis abstraction with CPU reference implementation and CUDA FTRAN/BTRAN infrastructure.",technologies:["SparseLU","cuSPARSE","FTRAN","BTRAN"]},
 {id:"workspace",title:"Persistent Workspace",phase:"4.8 + 5.2",kind:"workspace",status:"implemented",description:"Reusable basis, pricing and pipeline buffers designed to reduce repeated allocation overhead.",technologies:["Persistent buffers","Workspace reuse"]},
 {id:"adaptive",title:"Adaptive Backend Manager",phase:"5.4",kind:"backend",status:"implemented",description:"Chooses CPU or GPU based on device availability and workload characteristics.",technologies:["Workload thresholds","CPU/CUDA"]},
 {id:"stability",title:"Numerical Stability & Fallback",phase:"5.8",kind:"validation",status:"implemented",description:"Guards numerical operations and provides CPU recovery when GPU execution is unavailable or unsafe.",technologies:["Residual checks","Fallback","Validation"]},
 {id:"benchmark",title:"Benchmark / Verification",phase:"5.9–5.10 + 6.9–6.10",kind:"validation",status:"implemented",description:"Performance profiling, benchmark comparison, certificates and verification reporting.",technologies:["Netlib","Profiler","Certificates"]},
];
const edges=[["web","api","HTTP"],["api","production","solve"],["production","pipeline","coordinate"],["pipeline","pricing","pricing"],["pipeline","basis","FTRAN / BTRAN"],["pipeline","workspace","reuse"],["pipeline","adaptive","select"],["adaptive","pricing","backend"],["adaptive","basis","backend"],["pipeline","stability","validate"],["pipeline","benchmark","measure"]];

const kindLabel:Record<ArchitectureNodeKind,string>={application:"APPLICATION",solver:"SOLVER",backend:"BACKEND",workspace:"WORKSPACE",validation:"VALIDATION"};

export function ArchitectureVisualization() {
 const [selected,setSelected]=useState(nodes[0].id);
 const active=nodes.find(n=>n.id===selected)??nodes[0];
 return <section className="dashboard">
  <div className="hero"><div><span className="eyebrow">PHASE 6.11 · ARCHITECTURE</span><h2>Solver architecture</h2><p>Trace the web control center into the production C++ solver and through the Phase 3–5 optimization pipeline.</p></div><div className="hero-badge"><span className="status-dot"/> Architecture mapped</div></div>
  <article className="panel architecture-map">
   <div className="architecture-flow">
    {nodes.map((node,i)=><div key={node.id} className={"architecture-node "+(node.id===selected?"selected":"")} onClick={()=>setSelected(node.id)}>
      <div className="architecture-node-top"><span className="architecture-kind">{kindLabel[node.kind]}</span><span className={"architecture-state "+node.status}>{node.status}</span></div>
      <h3>{node.title}</h3><span className="architecture-phase">Phase {node.phase}</span><p>{node.description}</p>
      <div className="architecture-tech">{node.technologies.map(t=><span key={t}>{t}</span>)}</div>
      {i<nodes.length-1&&<span className="architecture-arrow">↓</span>}
    </div>)}
   </div>
  </article>
  <div className="content-grid">
   <article className="panel"><div className="panel-heading"><div><span className="eyebrow">CONNECTIONS</span><h3>Data flow</h3></div></div>
    <div className="architecture-connections">{edges.map(([from,to,label])=><div className="architecture-edge" key={from+to}><strong>{nodes.find(n=>n.id===from)?.title}</strong><span>→ {label} →</span><strong>{nodes.find(n=>n.id===to)?.title}</strong></div>)}</div>
   </article>
   <article className="panel"><div className="panel-heading"><div><span className="eyebrow">NODE DETAILS</span><h3>{active.title}</h3></div><span className="badge">{active.status.toUpperCase()}</span></div>
    <StatusLine label="Phase" value={active.phase}/><StatusLine label="Layer" value={kindLabel[active.kind]}/><p className="model-message">{active.description}</p>
    <div className="architecture-tech detail">{active.technologies.map(t=><span key={t}>{t}</span>)}</div>
   </article>
  </div>
 </section>;
}
function StatusLine({label,value}:{label:string;value:string}) {
 return <div className="status-row"><span>{label}</span><strong>{value}</strong></div>;
}
