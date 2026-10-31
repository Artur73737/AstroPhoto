"use client";
import { useRef, useState, type KeyboardEvent } from "react";
import { publicAsset } from "@/lib/paths";
import { Reveal } from "@/components/Reveal";
import captures from "@/content/screenshots.json";

const views = [
  { id: "live", label: "Live View & focus", image: "live-view.png" as const, title: "See the detail. Find your focus.", description: "Live USB preview with a luminance/RGB histogram, magenta Stars peaking, a red crosshair and the native focus sidebar. This capture shows a real camera pointed at an indoor monitor test scene; it is not a night-sky photograph." },
  { id: "camera", label: "Camera controls", image: "camera.png" as const, title: "Your Canon, with readable controls.", description: "A fresh capture of the real Camera panel: ISO 800, f/5.6, 1/15 s and RAW, as reported by the connected EOS 1100D. Exposure, color, recording and save destination remain together in one panel." },
  { id: "sequence", label: "Capture sequences", image: "sequences.png" as const, title: "Give every exposure a plan.", description: "The actual sequence editor, with the existing three-frame Light plan and its timing and camera settings. The screenshot shows an idle plan; no acquisition was started for this presentation." },
];

export function WorkspaceSection() {
  const [selected, setSelected] = useState(0);
  const tabs = useRef<HTMLDivElement>(null);
  const view = views[selected];
  const dimensions = captures.images[view.image];
  function navigate(event: KeyboardEvent<HTMLButtonElement>, index: number) {
    const next = event.key === "Home" ? 0 : event.key === "End" ? views.length - 1 : event.key === "ArrowRight" ? (index + 1) % views.length : event.key === "ArrowLeft" ? (index + views.length - 1) % views.length : null;
    if (next === null) return;
    event.preventDefault();
    setSelected(next);
    tabs.current?.querySelectorAll<HTMLButtonElement>('[role="tab"]')[next]?.focus();
  }
  return <section id="workspace" className="section-shell">
    <div className="section-eyebrow"><span>01 / Inside the application</span><span>Captured from the running app</span></div>
    <div className="section-heading"><Reveal><h2>A closer look.<br /><span className="font-serif font-normal italic">A clearer workflow.</span></h2></Reveal><p>Fresh captures of the real application, taken through its native MCP interface. Inspect each panel and open the original at full resolution.</p></div>
    <div className="workspace-grid">
      <div>
        <div ref={tabs} className="view-tabs" role="tablist" aria-label="Application panels">{views.map((item, index) => <button key={item.id} id={`tab-${item.id}`} role="tab" type="button" aria-selected={selected === index} aria-controls="workspace-panel" tabIndex={selected === index ? 0 : -1} onClick={() => setSelected(index)} onKeyDown={event => navigate(event,index)}>{item.label}</button>)}</div>
        <div role="tabpanel" id="workspace-panel" aria-labelledby={`tab-${view.id}`} tabIndex={0}>
          <figure className="detail-preview"><a href={publicAsset(`/screenshots/${view.image}`)} target="_blank" rel="noreferrer" aria-label={`Open full-resolution ${view.label} screenshot`}><img loading="lazy" decoding="async" key={view.image} src={publicAsset(`/screenshots/${view.image}`)} alt={`Fresh native MCP capture of AstroPhoto: ${view.label}`} width={dimensions.width} height={dimensions.height} className="detail-image" style={{maxWidth:dimensions.width,marginInline:"auto"}} /></a>
            <figcaption className="screenshot-open"><span>Original native UI capture · {dimensions.width} × {dimensions.height}</span><a className="text-link" href={publicAsset(`/screenshots/${view.image}`)} target="_blank" rel="noreferrer">Full resolution ↗</a></figcaption>
          </figure>
        </div>
      </div>
      <div className="detail-copy" aria-live="polite"><span className="hud-label text-[var(--accent-ink)]">{String(selected + 1).padStart(2,"0")} / Workspace detail</span><h3>{view.title}</h3><p>{view.description}</p></div>
    </div>
  </section>;
}
