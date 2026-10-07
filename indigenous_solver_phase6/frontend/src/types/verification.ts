export type VerificationStatus = "ok" | "degraded" | "failed" | "unavailable";
export interface VerificationCheck { id:string; category:string; name:string; status:"passed"|"failed"|"skipped"|"unavailable"; severity:"info"|"warning"|"error"; message:string; durationMs?:number|null; }
export interface VerificationSummary {
 status:VerificationStatus; passed:number; total:number; failed:number; skipped:number;
 numericalStable:boolean|null; certificatePassCount:number|null; fallbackChecksPassed:number|null;
 regressionChecksPassed:number|null; ftranChecksPassed:number|null; btranChecksPassed:number|null;
 pricingChecksPassed:number|null; workspaceChecksPassed:number|null; gpuRuntimeReady:boolean|null;
 lastVerifiedAt?:string; message?:string;
}
export interface VerificationSnapshot { summary:VerificationSummary; checks:VerificationCheck[]; }
