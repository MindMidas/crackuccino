import type { ReactNode } from "react";

interface PanelProps {
  title: string;
  eyebrow: string;
  actions?: ReactNode;
  children: ReactNode;
  className?: string;
  bodyClassName?: string;
}

export function Panel({ title, eyebrow, actions, children, className = "", bodyClassName = "" }: PanelProps) {
  return (
    <section className={`panel ${className}`}>
      <header>
        <div>
          <p className="eyebrow">{eyebrow}</p>
          <h2 className="mt-1 text-base font-semibold leading-tight text-white">{title}</h2>
        </div>
        {actions}
      </header>
      <div className={`panel-body ${bodyClassName}`}>{children}</div>
    </section>
  );
}
