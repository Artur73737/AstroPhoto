"use client";

import { useEffect, useRef, useState, type PointerEvent } from "react";
import { RulerDigits } from "@/components/RulerDigits";
import { clamp, pointerProgress, scrollbarGeometry } from "@/lib/scrollbar";
import type { SideRulerProps } from "./SideRuler.types";

const ticks = Array.from({ length: 21 }, (_, index) => index * 5);

export function SideRuler({ progress, documentHeight, viewportHeight, label }: SideRulerProps) {
  const track = useRef<HTMLDivElement>(null);
  const drag = useRef<{ pointerId: number; offset: number } | null>(null);
  const [trackHeight, setTrackHeight] = useState(0);
  const [dragging, setDragging] = useState(false);
  const { thumbHeight, travel } = scrollbarGeometry(trackHeight, viewportHeight, documentHeight);
  const value = Math.round(clamp(progress) * 100);
  const disabled = documentHeight <= viewportHeight;

  useEffect(() => {
    const element = track.current;
    if (!element) return;
    const observer = new ResizeObserver(() => setTrackHeight(element.clientHeight));
    observer.observe(element);
    return () => observer.disconnect();
  }, []);

  function scrollTo(value: number) {
    window.scrollTo({ top: clamp(value) * Math.max(0, document.documentElement.scrollHeight - window.innerHeight), behavior: "instant" });
  }

  function move(event: PointerEvent<HTMLDivElement>, offset: number) {
    const rect = event.currentTarget.getBoundingClientRect();
    const geometry = scrollbarGeometry(rect.height, window.innerHeight, document.documentElement.scrollHeight);
    scrollTo(pointerProgress(event.clientY, rect.top, offset, geometry.travel));
  }

  function finish(event: PointerEvent<HTMLDivElement>) {
    if (drag.current?.pointerId !== event.pointerId) return;
    drag.current = null;
    setDragging(false);
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
  }

  return <aside className="page-ruler" aria-label="Page navigation" data-dragging={dragging}>
    <div ref={track} role="scrollbar" tabIndex={0} aria-label={label} aria-controls="main" aria-orientation="vertical"
      aria-valuemin={0} aria-valuemax={100} aria-valuenow={value} aria-valuetext={`${value}% of page`} aria-disabled={disabled}
      className="ruler-track"
      onPointerDown={event => {
        if (disabled || !event.isPrimary || event.button !== 0) return;
        event.preventDefault();
        event.currentTarget.focus({ preventScroll: true });
        const thumb = event.currentTarget.querySelector<HTMLElement>(".ruler-thumb")!;
        const offset = thumb.contains(event.target as Node)
          ? event.clientY - thumb.getBoundingClientRect().top
          : thumb.getBoundingClientRect().height / 2;
        drag.current = { pointerId: event.pointerId, offset };
        setDragging(true);
        event.currentTarget.setPointerCapture(event.pointerId);
        move(event, offset);
      }}
      onPointerMove={event => { if (drag.current?.pointerId === event.pointerId) move(event, drag.current.offset); }}
      onPointerUp={finish} onPointerCancel={finish}
      onLostPointerCapture={() => { drag.current = null; setDragging(false); }}
      onKeyDown={event => {
        const page = viewportHeight / Math.max(1, documentHeight - viewportHeight);
        const values: Record<string, number> = { ArrowDown: progress + .02, ArrowUp: progress - .02, PageDown: progress + page, PageUp: progress - page, Home: 0, End: 1 };
        if (event.key in values) { event.preventDefault(); scrollTo(values[event.key]); }
      }}>
      <div className="ruler-scale" aria-hidden="true">{ticks.map(tick => <span key={tick} className="ruler-tick" data-major={tick % 25 === 0} style={{ top: `${tick}%` }}>{tick % 25 === 0 && <span>{String(tick).padStart(2, "0")}</span>}</span>)}</div>
      <div className="ruler-thumb" aria-hidden="true" style={{ height: thumbHeight, transform: `translateY(${clamp(progress) * travel}px)` }} />
    </div>
    <div className="ruler-readout" aria-hidden="true"><RulerDigits value={value} /><span>%</span></div>
  </aside>;
}
