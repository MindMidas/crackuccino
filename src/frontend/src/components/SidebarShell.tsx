import { useEffect, useState, type ReactNode } from "react";
import { SidebarBrand } from "./Sidebar";
import { SidebarDrawerCloseContext } from "./sidebarDrawerContext";

interface SidebarShellProps {
  children: ReactNode;
}

export function SidebarShell({ children }: SidebarShellProps) {
  const [open, setOpen] = useState(false);
  const close = () => setOpen(false);
  const toggle = () => setOpen((current) => !current);

  useEffect(() => {
    if (!open) return undefined;
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") close();
    };
    window.addEventListener("keydown", onKey);
    document.body.style.overflow = "hidden";
    return () => {
      window.removeEventListener("keydown", onKey);
      document.body.style.overflow = "";
    };
  }, [open]);

  return (
    <SidebarDrawerCloseContext.Provider value={close}>
      <div className="sidebar-shell">
        <header className="mobile-topbar mobile-header-bar">
          <SidebarBrand className="mobile-topbar-brand" />
          <button
            type="button"
            className={`menu-toggle${open ? " is-open" : ""}`}
            aria-label={open ? "Close menu" : "Open menu"}
            aria-expanded={open}
            onClick={toggle}
          >
            <span className="menu-bar" aria-hidden="true" />
            <span className="menu-bar" aria-hidden="true" />
            <span className="menu-bar" aria-hidden="true" />
          </button>
        </header>
        <button
          type="button"
          className={`sidebar-backdrop ${open ? "open" : ""}`}
          aria-label="Close menu"
          tabIndex={open ? 0 : -1}
          onClick={close}
        />
        <div className={`sidebar-column ${open ? "open" : ""}`}>
          <div className="sidebar-drawer-content">
            {children}
          </div>
        </div>
      </div>
    </SidebarDrawerCloseContext.Provider>
  );
}
