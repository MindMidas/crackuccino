import type { DecryptionResult, RunState } from "../types/cipher";
import { Panel } from "./Panel";

function normalize(value: string): string {
  return value.trim().toLowerCase().replace(/\s+/g, " ");
}

interface ResultsPanelProps {
  results: DecryptionResult[];
  expectedPlaintext: string;
  state: RunState | null;
}

function emptyMessage(state: RunState | null): string {
  if (state === "complete") return "No valid dictionary matches found.";
  if (state === "failed") return "No matches were produced because the run failed.";
  if (state === "canceled") return "No matches were found before the run was canceled.";
  if (state === "queued" || state === "running") return "Searching for valid dictionary matches.";
  return "Valid plaintext candidates will appear here.";
}

export function ResultsPanel({ results, expectedPlaintext, state }: ResultsPanelProps) {
  return (
    <Panel eyebrow="Output" title="Valid dictionary matches" actions={<span className="status-dot">{results.length} FOUND</span>} className="results-panel" bodyClassName="results-body">
      {results.length > 0 ? (
        <div className="results-scroll">
          {results.map((result) => {
            const matchesInput = Boolean(expectedPlaintext) && normalize(result.plaintext) === normalize(expectedPlaintext);
            return <article className={`result-card ${matchesInput ? "matches-input" : ""}`} key={`${result.rank}-${result.permutation}-${result.plaintext}`}>
              <div className="flex flex-wrap items-center gap-2">
                <span className="hit-badge">rank {result.rank}</span>
                <code>{result.permutation}</code>
                {matchesInput && <span className="match-badge">original input</span>}
              </div>
              <p>{result.plaintext}</p>
            </article>;
          })}
        </div>
      ) : <div className="empty-state">{emptyMessage(state)}</div>}
    </Panel>
  );
}
