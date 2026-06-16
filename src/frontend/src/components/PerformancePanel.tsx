import type { BenchmarkStats, RunState } from "../types/cipher";
import { Panel } from "./Panel";

export function PerformancePanel({ stats, state }: { stats: BenchmarkStats | null; state: RunState | null }) {
  const complete = state === "complete";
  const metrics = [
    ["Runtime", stats ? `${stats.runtimeSeconds.toFixed(2)}s` : "—"],
    ["Progress", stats ? `${stats.progressPercent.toFixed(1)}%` : "—"],
    ["Visited", stats ? stats.visitedPermutations.toLocaleString() : "—"],
    ["Valid hits", stats ? stats.validHits.toLocaleString() : "—"],
    ["Speedup", complete && stats ? `${stats.speedup.toFixed(2)}×` : "—"],
    ["Efficiency", complete && stats ? `${stats.efficiencyPercent.toFixed(0)}%` : "—"],
  ];
  return (
    <Panel eyebrow="Benchmark" title="Live metrics" actions={<span className="status-dot">{state ?? "idle"}</span>} className="metrics-panel">
      <div className="metrics-grid">
        {metrics.map(([label, value]) => (
          <div key={label}>
            <p className="field-label">{label}</p>
            <p className="mt-2 text-xl font-semibold tracking-tight text-white">{value}</p>
          </div>
        ))}
      </div>
      <div className="metric-progress">
        <div className="mb-2 flex justify-between text-xs text-zinc-500"><span>Overall permutation progress</span><span>{stats?.progressPercent.toFixed(1) ?? "0.0"}%</span></div>
        <div className="progress-bar"><div className="progress-bar-fill" style={{ width: `${stats?.progressPercent ?? 0}%` }} /></div>
      </div>
    </Panel>
  );
}
