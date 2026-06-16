import { useEffect, useId, useRef, useState } from "react";
import { InfoTip } from "./InfoTip";

export type SelectOption = readonly [string, string];

interface SelectFieldProps {
  label: string;
  tip?: string | undefined;
  value: string;
  options: SelectOption[];
  onChange: (value: string) => void;
  disabled?: boolean | undefined;
}

export function SelectField({ label, tip, value, options, onChange, disabled = false }: SelectFieldProps) {
  const listboxId = useId();
  const rootRef = useRef<HTMLDivElement>(null);
  const [open, setOpen] = useState(false);
  const selectedLabel = options.find(([optionValue]) => optionValue === value)?.[1] ?? value;

  useEffect(() => {
    if (!open) return;

    const closeOnOutsideClick = (event: MouseEvent) => {
      if (!rootRef.current?.contains(event.target as Node)) setOpen(false);
    };
    const closeOnEscape = (event: KeyboardEvent) => {
      if (event.key === "Escape") setOpen(false);
    };

    document.addEventListener("mousedown", closeOnOutsideClick);
    document.addEventListener("keydown", closeOnEscape);
    return () => {
      document.removeEventListener("mousedown", closeOnOutsideClick);
      document.removeEventListener("keydown", closeOnEscape);
    };
  }, [open]);

  return (
    <div className="select-field" ref={rootRef}>
      <span className="field-label field-label-row" id={`${listboxId}-label`}>
        <span>{label}</span>
        {tip ? <InfoTip ariaLabel={`About ${label.toLowerCase()}`} text={tip} /> : null}
      </span>
      <button
        aria-expanded={open}
        aria-haspopup="listbox"
        aria-labelledby={`${listboxId}-label`}
        className="select-trigger"
        disabled={disabled}
        onClick={() => setOpen((current) => !current)}
        type="button"
      >
        <span className="select-trigger-label">{selectedLabel}</span>
        <span aria-hidden className="select-chevron">
          <svg viewBox="0 0 12 12" fill="none">
            <path d="M3 4.5 6 7.5 9 4.5" stroke="currentColor" strokeLinecap="round" strokeLinejoin="round" strokeWidth="1.5" />
          </svg>
        </span>
      </button>
      {open && !disabled && (
        <ul aria-labelledby={`${listboxId}-label`} className="select-menu" id={listboxId} role="listbox">
          {options.map(([optionValue, optionLabel]) => (
            <li key={optionValue} role="none">
              <button
                aria-selected={optionValue === value}
                className={optionValue === value ? "active" : ""}
                onClick={() => {
                  onChange(optionValue);
                  setOpen(false);
                }}
                role="option"
                type="button"
              >
                {optionLabel}
              </button>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
