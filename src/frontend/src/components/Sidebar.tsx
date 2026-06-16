import { useCallback, useLayoutEffect, useRef } from "react";
import { assetUrl } from "../config/site";
import type { RunSnapshot } from "../types/cipher";
import { InfoTip } from "./InfoTip";
import { useSidebarDrawerClose } from "./sidebarDrawerContext";

export type AppView = "workspace" | "dictionaries" | "upload" | "report";

interface SidebarProps {
  view: AppView;
  runs: RunSnapshot[];
  activeRun: RunSnapshot | null;
  onSelectRun: (run: RunSnapshot) => void;
  onDeleteRun: (run: RunSnapshot) => void;
  onNewRun: () => void;
  onViewDictionaries: () => void;
  onUploadDictionary: () => void;
}

function formatRuntime(run: RunSnapshot): string {
  return run.stats.runtimeSeconds > 0 ? `${run.stats.runtimeSeconds.toFixed(2)}s` : "not started";
}

function formatMatches(run: RunSnapshot): string {
  const count = run.results.length;
  return `${count} ${count === 1 ? "match" : "matches"}`;
}

export function SidebarBrand({ className = "" }: { className?: string }) {
  const titleRef = useRef<HTMLSpanElement>(null);
  const taglineRef = useRef<HTMLSpanElement>(null);

  const fitTagline = useCallback(() => {
    const title = titleRef.current;
    const tagline = taglineRef.current;
    if (!title || !tagline) return;

    const targetWidth = title.getBoundingClientRect().width;
    if (targetWidth <= 0) return;

    let size = 5;
    const maxSize = 24;
    tagline.style.fontSize = `${size}px`;
    while (tagline.scrollWidth <= targetWidth && size < maxSize) {
      size += 0.25;
      tagline.style.fontSize = `${size}px`;
    }
    if (tagline.scrollWidth > targetWidth) {
      size = Math.max(5, size - 0.25);
      tagline.style.fontSize = `${size}px`;
    }
  }, []);

  useLayoutEffect(() => {
    fitTagline();
    const title = titleRef.current;
    if (!title) return undefined;

    const observer = new ResizeObserver(fitTagline);
    observer.observe(title);
    window.addEventListener("resize", fitTagline);
    return () => {
      observer.disconnect();
      window.removeEventListener("resize", fitTagline);
    };
  }, [fitTagline]);

  return (
    <div className={`sidebar-brand${className ? ` ${className}` : ""}`}>
      <img className="sidebar-brand-logo" src={assetUrl("/assets/crackuccino.png")} alt="" width={64} height={64} />
      <div className="sidebar-brand-copy">
        <span className="sidebar-brand-title" ref={titleRef}>CRACKUCCINO</span>
        <span className="sidebar-brand-tagline" ref={taglineRef}>substitution cipher decryptor</span>
      </div>
    </div>
  );
}

function NavIcon({ kind }: { kind: "plus" | "menu" | "upload" }) {
  return (
    <span className="sidebar-nav-icon" aria-hidden="true">
      {kind === "plus" && (
        <svg viewBox="0 0 12 12" fill="none">
          <path d="M6 1.5v9M1.5 6h9" stroke="currentColor" strokeLinecap="round" strokeWidth="1.5" />
        </svg>
      )}
      {kind === "menu" && (
        <svg viewBox="0 0 12 12" fill="none">
          <path d="M1.5 3h9M1.5 6h9M1.5 9h9" stroke="currentColor" strokeLinecap="round" strokeWidth="1.5" />
        </svg>
      )}
      {kind === "upload" && (
        <svg viewBox="0 0 12 12" fill="none">
          <path d="M6 2.5v6.5M3.5 5.5 6 3l2.5 2.5M2 9.5h8" stroke="currentColor" strokeLinecap="round" strokeLinejoin="round" strokeWidth="1.5" />
        </svg>
      )}
    </span>
  );
}

export function Sidebar({
  view,
  runs,
  activeRun,
  onSelectRun,
  onDeleteRun,
  onNewRun,
  onViewDictionaries,
  onUploadDictionary,
}: SidebarProps) {
  const closeDrawer = useSidebarDrawerClose();
  const navigate = (action: () => void) => {
    action();
    closeDrawer();
  };

  return (
    <aside className="sidebar">
      <SidebarBrand />
      <nav className="sidebar-nav" aria-label="Workspace navigation">
        <button
          className={`sidebar-nav-button ${view === "workspace" && activeRun === null ? "active" : ""}`}
          onClick={() => navigate(onNewRun)}
          type="button"
        >
          <span>New workload</span>
          <NavIcon kind="plus" />
        </button>
        <button
          className={`sidebar-nav-button ${view === "dictionaries" ? "active" : ""}`}
          onClick={() => navigate(onViewDictionaries)}
          type="button"
        >
          <span>View dictionaries</span>
          <NavIcon kind="menu" />
        </button>
        <button
          className={`sidebar-nav-button ${view === "upload" ? "active" : ""}`}
          onClick={() => navigate(onUploadDictionary)}
          type="button"
        >
          <span>Upload dictionary</span>
          <NavIcon kind="upload" />
        </button>
      </nav>
      <div className="sidebar-section-heading">
        <span className="sidebar-section-title">
          Recent runs
          <InfoTip ariaLabel="About recent runs" text="Your decrypt runs this session." />
        </span>
        <em>{runs.length}</em>
      </div>
      <nav className="run-list" aria-label="Recent MPI runs">
        {runs.map((run) => (
          <div className={`run-list-item ${activeRun?.id === run.id ? "active" : ""}`} key={run.id}>
            <button className="run-select-button" onClick={() => navigate(() => onSelectRun(run))} type="button">
              <span className="run-list-top"><strong>#{run.id.slice(0, 7)}</strong><i className={`run-state run-state-${run.state}`}>{run.state}</i></span>
              <span className="run-list-message">{run.request.plaintext || run.request.ciphertext}</span>
              <span className="run-list-meta">{formatMatches(run)} · {run.stats.ranks === 1 ? "serial" : `${run.stats.ranks} ranks`} · {formatRuntime(run)} · {run.stats.progressPercent.toFixed(0)}%</span>
            </button>
            <button className="run-delete-button" aria-label={`Delete run ${run.id.slice(0, 7)}`} onClick={() => onDeleteRun(run)} type="button">×</button>
          </div>
        ))}
        {runs.length === 0 && <p className="sidebar-empty">Workloads will appear here.</p>}
      </nav>
    </aside>
  );
}
