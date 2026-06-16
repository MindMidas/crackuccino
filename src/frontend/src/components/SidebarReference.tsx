import { GITHUB_URL } from "../config/site";
import type { AppView } from "./Sidebar";
import { useSidebarDrawerClose } from "./sidebarDrawerContext";

interface SidebarReferenceProps {
  view: AppView;
  onOpenReport: () => void;
  onCloseReport: () => void;
}

function DocumentIcon() {
  return (
    <svg aria-hidden="true" viewBox="0 0 16 16" fill="currentColor">
      <path d="M4.5 0A1.5 1.5 0 0 0 3 1.5v13A1.5 1.5 0 0 0 4.5 16h7a1.5 1.5 0 0 0 1.5-1.5V4.621a1.5 1.5 0 0 0-.44-1.06L9.94.439A1.5 1.5 0 0 0 8.878 0H4.5zM8.5 1v3a1 1 0 0 0 1 1h3.002L8.5 1z" />
    </svg>
  );
}

function GitHubIcon() {
  return (
    <svg aria-hidden="true" viewBox="0 0 16 16" fill="currentColor">
      <path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0 0 16 8c0-4.42-3.58-8-8-8z" />
    </svg>
  );
}

export function SidebarReference({ view, onOpenReport, onCloseReport }: SidebarReferenceProps) {
  const reportOpen = view === "report";
  const closeDrawer = useSidebarDrawerClose();

  const handleReport = () => {
    if (reportOpen) onCloseReport();
    else onOpenReport();
    closeDrawer();
  };

  return (
    <section className="sidebar-reference" aria-label="Project reference">
      <p className="sidebar-reference-kicker">Reference</p>
      <div className="sidebar-reference-block">
        <button
          className={`sidebar-labeled-button secondary-button${reportOpen ? " active" : ""}`}
          onClick={handleReport}
          type="button"
        >
          <DocumentIcon />
          <span>{reportOpen ? "Back to workspace" : "Project write-up"}</span>
        </button>
      </div>
      <div className="sidebar-reference-block">
        <a className="sidebar-labeled-button secondary-button button-link" href={GITHUB_URL} rel="noopener noreferrer" target="_blank">
          <GitHubIcon />
          <span>Source code</span>
        </a>
      </div>
    </section>
  );
}
