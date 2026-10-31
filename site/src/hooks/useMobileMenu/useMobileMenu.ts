"use client";

import { useCallback, useState } from "react";

export function useMobileMenu(): {
  open: boolean;
  openMenu: () => void;
  closeMenu: () => void;
  toggleMenu: () => void;
} {
  const [open, setOpen] = useState(false);
  const openMenu = useCallback(() => setOpen(true), []);
  const closeMenu = useCallback(() => setOpen(false), []);
  const toggleMenu = useCallback(() => setOpen((previous) => !previous), []);
  return { open, openMenu, closeMenu, toggleMenu };
}
