import { useEffect } from "react";

export function usePdfViewport(enabled: boolean) {
  useEffect(() => {
    if (enabled) {
      document.documentElement.classList.add("pdf-viewer-open");
    }

    return () => {
      document.documentElement.classList.remove("pdf-viewer-open");
    };
  }, [enabled]);
}
