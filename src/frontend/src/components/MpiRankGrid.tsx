import type { MpiRankStatus } from "../types/cipher";
import { MpiRankCard } from "./MpiRankCard";
import { Panel } from "./Panel";

export function MpiRankGrid({ ranks }: { ranks: MpiRankStatus[] }) {
  return (
    <Panel eyebrow="Workers" title="Search tasks and hits" actions={<span className="status-dot">{ranks.length || 0} workers</span>} className="rank-panel" bodyClassName="rank-panel-body">
      {ranks.length > 0 ? (
        <div className="rank-grid">
          {ranks.map((rank) => <MpiRankCard key={rank.rank} status={rank} />)}
        </div>
      ) : (
        <div className="empty-state">Run decryption to inspect work assigned to each MPI rank.</div>
      )}
    </Panel>
  );
}
