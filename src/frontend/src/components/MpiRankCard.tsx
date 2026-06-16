import { useEffect, useRef } from "react";
import type { MpiRankStatus } from "../types/cipher";

export function MpiRankCard({ status }: { status: MpiRankStatus }) {
  const traceRef = useRef<HTMLDivElement>(null);
  const target = status.assignedPermutations > 0 ? status.assignedPermutations : status.permutations;
  const percent = target > 0 ? Math.min(100, status.permutations / target * 100) : 0;
  const currentPrefix = status.state === "complete" ? "finished" : status.prefix || "waiting";
  const trace = status.permutationTrace;

  useEffect(() => {
    const traceElement = traceRef.current;
    if (traceElement) traceElement.scrollTop = traceElement.scrollHeight;
  }, [trace.length]);

  return (
    <article className="rank-card">
      <div className="flex items-center justify-between">
        <span className="font-mono text-xs text-zinc-500">RANK {String(status.rank).padStart(2, "0")}</span>
        <span className={`rank-status rank-status-${status.state}`}><i />{status.state}</span>
      </div>
      <p className="field-label mt-3">Current prefix</p>
      <p className="rank-prefix">{currentPrefix}<span>{status.state === "complete" ? "" : "···"}</span></p>
      <div className="rank-progress-row">
        <div className="rank-progress"><i style={{ width: `${percent}%` }} /></div>
        <span>{percent.toFixed(1)}%</span>
      </div>
      <div className="rank-metrics">
        <div>
          <p className="text-[10px] uppercase tracking-[0.16em] text-zinc-600">Permutations</p>
          <p className="mt-1 text-sm font-medium text-zinc-300">{status.permutations.toLocaleString()} / {status.assignedPermutations.toLocaleString()}</p>
        </div>
        <div><p className="field-label">Tasks completed</p><p className="mt-1 text-sm text-zinc-300">{status.completedTasks} / {status.assignedTasks}</p></div>
        <div><p className="field-label">Valid hits</p><p className={status.hits > 0 ? "hit-badge mt-1" : "mt-1 text-sm text-zinc-700"}>{status.hits}</p></div>
      </div>
      <div className="rank-trace">
        <div className="rank-trace-header">
          <span>Permutation trace</span>
          <em>{trace.length.toLocaleString()} samples</em>
        </div>
        <div className="rank-trace-list" ref={traceRef}>
          {trace.length > 0 ? trace.map((entry, index) => (
            <div className="rank-trace-row" key={`${entry.visited}-${entry.permutation}-${index}`}>
              <span>{entry.visited.toLocaleString()}</span>
              <code>{entry.permutation}</code>
            </div>
          )) : (
            <p className="rank-trace-empty">Samples appear as this rank searches.</p>
          )}
        </div>
      </div>
    </article>
  );
}
