import type { ThemeMode } from "@/types/theme";
const STORAGE_KEY = "astrophoto-theme";
export function getStoredTheme(): ThemeMode | null {
  if (typeof window === "undefined") return null;
  try {
    const stored = window.localStorage.getItem(STORAGE_KEY);
    return stored === "light" || stored === "dark" ? stored : null;
  } catch { return null; }
}
export function storeTheme(mode: ThemeMode): void {
  try { window.localStorage.setItem(STORAGE_KEY, mode); } catch { /* Theme still works when browser storage is blocked. */ }
}
export function resolvePreferredTheme(): ThemeMode {
  return getStoredTheme() || (typeof window !== "undefined" && window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light");
}
export function applyTheme(mode: ThemeMode): void {
  document.documentElement.dataset.theme = mode;
}
