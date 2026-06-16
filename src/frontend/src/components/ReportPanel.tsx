import { REPORT_URL } from "../config/site";
import { usePdfViewport } from "../utils/usePdfViewport";
import { Panel } from "./Panel";

interface ReportPanelProps {
  onClose: () => void;
}

export function ReportPanel({ onClose }: ReportPanelProps) {
  usePdfViewport(true);

  return (
    <section className="report-panel" aria-label="Project write-up">
      <Panel
        actions={(
          <button className="panel-close-button" aria-label="Close write-up" onClick={onClose} type="button">
            ×
          </button>
        )}
        eyebrow="Reference"
        title="Project write-up"
      >
        <div className="report-view">
          <iframe src={REPORT_URL} title="Crackuccino project write-up" />
        </div>
      </Panel>
    </section>
  );
}
