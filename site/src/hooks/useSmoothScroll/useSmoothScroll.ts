"use client";

import { useEffect } from "react";

export function useSmoothScroll(): void {
  useEffect(() => {
    let disposed = false;
    let cleanup: (() => void) | undefined;
    void import("lenis").then(({ default: Lenis }) => {
      if (disposed || window.matchMedia("(prefers-reduced-motion: reduce)").matches) return;
      const lenis = new Lenis({ lerp: 0.12, anchors: true });
      let frame = 0;
      const loop = (time: number): void => {
        lenis.raf(time);
        frame = requestAnimationFrame(loop);
      };
      frame = requestAnimationFrame(loop);
      cleanup = () => {
        cancelAnimationFrame(frame);
        lenis.destroy();
      };
    });
    return () => { disposed = true; cleanup?.(); };
  }, []);
}
