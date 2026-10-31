"use client";

import { useEffect, useState } from "react";
import type { RulerProgress } from "./useRulerProgress.types";

export function useRulerProgress(): RulerProgress {
  const [metrics, setMetrics] = useState<RulerProgress>({ progress: 0, scrollY: 0, documentHeight: 0, viewportHeight: 0 });
  useEffect(() => {
    let frame = 0;
    const update = () => {
      if (frame) return;
      frame = requestAnimationFrame(() => {
        frame = 0;
        const documentHeight = document.documentElement.scrollHeight;
        const viewportHeight = window.innerHeight;
        const scrollY = window.scrollY;
        const total = documentHeight - viewportHeight;
        setMetrics({ progress: total > 0 ? Math.min(1, Math.max(0, scrollY / total)) : 0, scrollY, documentHeight, viewportHeight });
      });
    };
    const observer = new ResizeObserver(update);
    observer.observe(document.body);
    update();
    window.addEventListener("scroll", update, { passive: true });
    window.addEventListener("resize", update);
    return () => {
      cancelAnimationFrame(frame);
      observer.disconnect();
      window.removeEventListener("scroll", update);
      window.removeEventListener("resize", update);
    };
  }, []);
  return metrics;
}
