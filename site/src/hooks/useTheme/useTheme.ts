"use client";
import { useCallback, useEffect, useState } from "react";
import { applyTheme, resolvePreferredTheme, storeTheme } from "@/services/theme";
import type { ThemeMode } from "@/types/theme";
export function useTheme(): { mode: ThemeMode; toggleTheme: () => void } {
  const [mode, setMode] = useState<ThemeMode>("light");
  useEffect(() => {
    const preferred = resolvePreferredTheme();
    applyTheme(preferred);
    // eslint-disable-next-line react-hooks/set-state-in-effect -- synchronize browser preference after hydration
    setMode(preferred);
  }, []);
  const toggleTheme = useCallback(() => {
    const next = document.documentElement.dataset.theme === "dark" ? "light" : "dark";
    applyTheme(next);
    storeTheme(next);
    setMode(next);
  }, []);
  return { mode, toggleTheme };
}
