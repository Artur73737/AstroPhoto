import Image from "next/image";
import { CallToActionButton } from "@/components/CallToActionButton";
import { Reveal } from "@/components/Reveal";
import { publicAsset } from "@/lib/paths";
import captures from "@/content/screenshots.json";
import type { HeroSectionProps } from "./HeroSection.types";

export function HeroSection({ content, brandName }: HeroSectionProps) {
  return <section id="hero" className="hero-surface">
    <div className="hero-content">
      <p className="hud-label hero-eyebrow"><span className="status-dot" />{brandName} / Canon EOS / Windows x64</p>
      <div className="hero-layout">
        <div className="hero-copy">
          <Reveal><h1 className="hero-title">{content.titleStart}<br /><span className="font-serif font-normal italic text-[var(--accent-ink)]">{content.titleAccent}</span></h1></Reveal>
          <Reveal delay={60}><p className="hero-description">{content.description}</p></Reveal>
          <Reveal delay={100}><div className="hero-actions"><CallToActionButton {...content.primaryCta} /><CallToActionButton {...content.secondaryCta} variant="secondary" /></div></Reveal>
          <p className="hud-label hero-meta">{content.platformNote}<br /><a className="text-link" href={content.sourceLink.href}>{content.sourceLink.label} ↗</a></p>
        </div>
        <Reveal delay={100} className="hero-preview">
          <figure className="app-frame">
            <div className="frame-title"><span className="hud-label">AstroPhoto / Full workspace</span><span className="hud-label">Windows x64</span></div>
            <a href={publicAsset("/screenshots/all.png")} target="_blank" rel="noreferrer" aria-label="Open the full-size AstroPhoto application screenshot"><Image src={publicAsset("/screenshots/all.png")} alt="Complete AstroPhoto interface with toolbar, connection, camera settings, image and focus tools, capture sequences, files and session, and operation log" width={captures.images["all.png"].width} height={captures.images["all.png"].height} priority className="block h-auto w-full" /></a>
            <figcaption className="frame-title"><span>Complete application workspace.</span><a className="text-link" href={publicAsset("/screenshots/all.png")} target="_blank" rel="noreferrer">Full resolution ↗</a></figcaption>
          </figure>
        </Reveal>
      </div>
      <div className="hero-specs"><span>01 / Native C++20</span><span>02 / Canon EDSDK</span><span>03 / Local capture workflow</span></div>
    </div>
  </section>;
}
