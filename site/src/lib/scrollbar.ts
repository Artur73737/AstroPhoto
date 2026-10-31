export const clamp = (value: number, min = 0, max = 1): number =>
  Math.min(max, Math.max(min, Number.isFinite(value) ? value : min));

export function scrollbarGeometry(trackHeight: number, viewportHeight: number, documentHeight: number) {
  const height = Math.max(0, trackHeight);
  const fraction = documentHeight > 0 ? clamp(viewportHeight / documentHeight) : 1;
  const thumbHeight = Math.min(height, Math.max(56, height * fraction));
  return { thumbHeight, travel: Math.max(0, height - thumbHeight) };
}

export function pointerProgress(clientY: number, trackTop: number, grabOffset: number, travel: number): number {
  return travel > 0 ? clamp((clientY - trackTop - grabOffset) / travel) : 0;
}
