import type { ThemeToggleProps } from "./ThemeToggle.types";

export function ThemeToggle({ label, mode, onToggle }: ThemeToggleProps) {
  const iconPath =
    mode === "light"
      ? "M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M6.34 17.66l-1.41 1.41M19.07 4.93l-1.41 1.41"
      : "M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z";
  return (
    <button
      type="button"
      aria-label={label}
      onClick={onToggle}
      className="invert-block flex w-12 items-center justify-center border-l border-[var(--line)] text-[var(--muted)]"
    >
      <svg
        xmlns="http://www.w3.org/2000/svg"
        width="24"
        height="24"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        strokeWidth="2"
        strokeLinecap="round"
        strokeLinejoin="round"
        className="h-4 w-4"
        aria-hidden="true"
      >
        {mode === "light" && <circle cx="12" cy="12" r="4" />}
        <path d={iconPath} />
      </svg>
    </button>
  );
}
