import type { ReactNode } from "react";
import { InfoTip } from "./InfoTip";

interface FieldLabelProps {
  children: ReactNode;
  htmlFor?: string;
  tip?: string | undefined;
  tipLabel?: string | undefined;
}

export function FieldLabel({ children, htmlFor, tip, tipLabel }: FieldLabelProps) {
  const content = (
    <>
      <span>{children}</span>
      {tip ? <InfoTip ariaLabel={tipLabel ?? (typeof children === "string" ? children : "Field info")} text={tip} /> : null}
    </>
  );

  if (htmlFor) {
    return (
      <label className="field-label field-label-row" htmlFor={htmlFor}>
        {content}
      </label>
    );
  }

  return <p className="field-label field-label-row">{content}</p>;
}
