import { CrossTick } from "@/components/CrossTick";
import { EyebrowBar } from "@/components/EyebrowBar";
import { OwnershipCard } from "@/components/OwnershipCard";
import { Reveal } from "@/components/Reveal";
import { RuleLine } from "@/components/RuleLine";
import type { OwnershipSectionProps } from "./OwnershipSection.types";

export function OwnershipSection({ content }: OwnershipSectionProps) {
  return (
    <section id="ownership" className="relative">
      <RuleLine placement="top" />
      <div className="relative mx-auto max-w-[1280px] border-x border-[var(--line)]">
        <CrossTick position="top-left" />
        <CrossTick position="top-right" />
        <EyebrowBar eyebrow={content.eyebrow} />
        <div className="section-intro">
          <Reveal>
            <h2 className="section-title">
              {content.headingStart} <span className="font-serif font-normal italic">{content.headingAccent}</span> {content.headingEnd}
            </h2>
          </Reveal>
          <Reveal delay={90}>
            <p className="section-description">{content.description}</p>
          </Reveal>
        </div>
        <div className="grid gap-px border-t border-[var(--line)] bg-[var(--line)] md:grid-cols-3">
          {content.items.map((item) => (
            <OwnershipCard key={item.id} item={item} />
          ))}
        </div>
      </div>
    </section>
  );
}
