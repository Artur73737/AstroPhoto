export interface ThemeToggleProps {
  label: string;
  mode: "light" | "dark";
  onToggle: () => void;
}
