import { createContext, useContext } from "react";

export const SidebarDrawerCloseContext = createContext<() => void>(() => {});

export function useSidebarDrawerClose() {
  return useContext(SidebarDrawerCloseContext);
}
