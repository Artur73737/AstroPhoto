import type { EyebrowBarProps } from "./EyebrowBar.types";

export function EyebrowBar({ eyebrow }: EyebrowBarProps) {
  return (
    <div className="section-marker sticky top-[var(--nav-height)] z-30 border-y border-[var(--line)] bg-[var(--bg)]/60 backdrop-blur-md">
      <div className="flex items-center justify-between gap-4 px-6 py-2 md:px-10">
        <p className="hud-label text-[var(--muted)]">
          <span className="text-[var(--faint)]">{eyebrow.index}</span>
          <span aria-hidden="true" className="mx-2 text-[var(--faint)]">
            /
          </span>
          {eyebrow.section}
        </p>
        <p className="hud-label hidden text-[var(--faint)] sm:block">{eyebrow.aside}</p>
      </div>
    </div>
  );
}
