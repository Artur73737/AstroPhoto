import { Reveal } from "@/components/Reveal";
import type { OwnershipCardProps } from "./OwnershipCard.types";

export function OwnershipCard({ item }: OwnershipCardProps) {
  return (
    <Reveal className="h-full">
      <article className="ownership-card content-card">
        <p className="hud-label text-[var(--faint)]">{item.marker}</p>
        <h3 className="card-title">{item.title}</h3>
        <p className="card-description">{item.description}</p>
        {item.linkLabel && item.linkHref ? (
          <a
            href={item.linkHref}
            data-cta=""
            className="invert-block mt-auto inline-flex w-fit items-center justify-center gap-1.5 rounded-none border border-[var(--line-strong)] bg-[var(--bg)] px-3 py-1.5 font-mono text-[11px] font-medium whitespace-nowrap uppercase tracking-[0.12em] text-[var(--ink)] transition-colors duration-150 hover:border-transparent"
          >
            {item.linkLabel}
          </a>
        ) : null}
      </article>
    </Reveal>
  );
}
