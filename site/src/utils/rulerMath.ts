export const RULER_TICK_STEP = 48.96;
export const RULER_MAJOR_EVERY = 5;

export interface RulerTick {
  top: number;
  major: boolean;
  label: string | null;
}

export function buildRulerTicks(documentHeight: number): RulerTick[] {
  if (!Number.isFinite(documentHeight) || documentHeight <= 0) return [];
  const count = Math.ceil(documentHeight / RULER_TICK_STEP);
  return Array.from({ length: count + 1 }, (_, index) => ({
    top: index * RULER_TICK_STEP,
    major: index % RULER_MAJOR_EVERY === 0,
    label: index % RULER_MAJOR_EVERY === 0 ? String(index) : null,
  }));
}

export function percentOf(progress: number): number {
  if (!Number.isFinite(progress)) return 0;
  return Math.round(Math.min(1, Math.max(0, progress)) * 100);
}

export function scrollToRatio(ratio: number, documentHeight: number, viewportHeight: number): void {
  const max = Math.max(0, documentHeight - viewportHeight);
  const top = Math.min(1, Math.max(0, ratio)) * max;
  const smooth = !window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  window.scrollTo({ top, behavior: smooth ? "smooth" : "auto" });
}
