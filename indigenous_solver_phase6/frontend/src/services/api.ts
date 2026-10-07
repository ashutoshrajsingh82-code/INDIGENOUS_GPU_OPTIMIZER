export interface ApiConfig {
  baseUrl: string;
  timeoutMs: number;
}

export const apiConfig: ApiConfig = {
  baseUrl: import.meta.env.VITE_SOLVER_API_URL ?? "http://127.0.0.1:8080",
  timeoutMs: 15000,
};

export async function apiHealth(): Promise<boolean> {
  try {
    const controller = new AbortController();
    const timer = window.setTimeout(() => controller.abort(), apiConfig.timeoutMs);
    const response = await fetch(apiConfig.baseUrl + "/health", {
      signal: controller.signal,
    });
    window.clearTimeout(timer);
    return response.ok;
  } catch {
    return false;
  }
}
