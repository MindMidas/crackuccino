import {
  FloatingPortal,
  autoUpdate,
  flip,
  offset,
  shift,
  useFloating,
  useFocus,
  useHover,
  useInteractions,
  useRole,
} from "@floating-ui/react";
import { useState } from "react";

interface InfoTipProps {
  text: string;
  ariaLabel: string;
}

export function InfoTip({ text, ariaLabel }: InfoTipProps) {
  const [open, setOpen] = useState(false);

  const { refs, floatingStyles, context } = useFloating({
    open,
    onOpenChange: setOpen,
    placement: "top",
    whileElementsMounted: autoUpdate,
    middleware: [
      offset(8),
      flip({ padding: 8 }),
      shift({ padding: 8 }),
    ],
  });

  const hover = useHover(context, { move: false });
  const focus = useFocus(context);
  const role = useRole(context, { role: "tooltip" });
  const { getReferenceProps, getFloatingProps } = useInteractions([hover, focus, role]);

  return (
    <>
      <button
        ref={refs.setReference}
        aria-label={ariaLabel}
        className="info-tip"
        type="button"
        {...getReferenceProps()}
      >
        i
      </button>
      {open && (
        <FloatingPortal>
          <span
            ref={refs.setFloating}
            className="info-tip-popover"
            style={floatingStyles}
            {...getFloatingProps()}
          >
            {text}
          </span>
        </FloatingPortal>
      )}
    </>
  );
}
