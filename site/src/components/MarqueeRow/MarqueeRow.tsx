import type { MarqueeRowProps } from "./MarqueeRow.types";

export function MarqueeRow({ items, label }: MarqueeRowProps) {
  return (
    <div aria-label={label} className="marquee flex overflow-hidden border-t border-[var(--line)]">
      {[0, 1].map((copy) => (
        <ul key={copy} aria-hidden={copy === 1} className="marquee-track flex items-center">
          {items.map((item) => (
            <li key={`${copy}-${item}`} className="hud-label flex items-center px-6 py-3 whitespace-nowrap text-[var(--muted)]">
              <span aria-hidden="true" className="mr-6 text-[var(--accent-ink)]">
                ▪
              </span>
              {item}
            </li>
          ))}
        </ul>
      ))}
    </div>
  );
}
