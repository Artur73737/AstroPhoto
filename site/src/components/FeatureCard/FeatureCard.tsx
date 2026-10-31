import { Reveal } from "@/components/Reveal";
import type { FeatureCardProps } from "./FeatureCard.types";
export function FeatureCard({ item }: FeatureCardProps) {
  return <Reveal className="h-full"><article className="feature-card content-card">
    <div className="flex justify-between gap-4"><p className="hud-label text-[var(--accent-ink)]">{item.category}</p><p className="hud-label text-[var(--muted)]">{item.index}</p></div>
    <h3 className="card-title">{item.title}</h3>
    <p className="card-description">{item.description}</p>
  </article></Reveal>;
}
