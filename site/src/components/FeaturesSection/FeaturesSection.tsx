import { CrossTick } from "@/components/CrossTick";
import { EyebrowBar } from "@/components/EyebrowBar";
import { FeatureCard } from "@/components/FeatureCard";
import { Reveal } from "@/components/Reveal";
import { RuleLine } from "@/components/RuleLine";
import type { FeaturesSectionProps } from "./FeaturesSection.types";

export function FeaturesSection({ content }: FeaturesSectionProps) {
  return (
    <section id="features" className="relative">
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
        <div className="features-grid">
          {content.items.map((item) => (
            <FeatureCard key={item.id} item={item} />
          ))}
        </div>
      </div>
    </section>
  );
}
