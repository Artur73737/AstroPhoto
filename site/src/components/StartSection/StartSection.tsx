import { CallToActionButton } from "@/components/CallToActionButton";
import { CrossTick } from "@/components/CrossTick";
import { Reveal } from "@/components/Reveal";
import { RuleLine } from "@/components/RuleLine";
import type { StartSectionProps } from "./StartSection.types";

export function StartSection({ content }: StartSectionProps) {
  return (
    <section id="start" className="relative">
      <RuleLine placement="top" />
      <div className="relative mx-auto max-w-[1280px] border-x border-[var(--line)]">
        <CrossTick position="top-left" />
        <CrossTick position="top-right" />
        <div className="start-content">
          <Reveal>
            <p className="hud-label text-[var(--faint)]">{content.caption}</p>
          </Reveal>
          <Reveal delay={80}>
            <h2 className="section-title">
              {content.headingStart} <span className="font-serif font-normal italic">{content.headingAccent}</span>
              {content.headingEnd}
            </h2>
          </Reveal>
          <Reveal delay={140}>
            <p className="section-description">{content.description}</p>
          </Reveal>
          <Reveal delay={200}>
            <div className="hero-actions">
              <CallToActionButton label={content.primaryCta.label} href={content.primaryCta.href} variant="primary" />
              <CallToActionButton label={content.secondaryCta.label} href={content.secondaryCta.href} variant="secondary" />
            </div>
          </Reveal>
        </div>
      </div>
    </section>
  );
}
