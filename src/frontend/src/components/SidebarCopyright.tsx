import { GITHUB_URL } from "../config/site";

interface SidebarCopyrightProps {
  className?: string;
}

export function SidebarCopyright({ className = "sidebar-copyright" }: SidebarCopyrightProps) {
  return (
    <p className={className}>
      © 2026 MindMidas ·{" "}
      <a href={GITHUB_URL} rel="noopener noreferrer" target="_blank">GitHub</a>
    </p>
  );
}
