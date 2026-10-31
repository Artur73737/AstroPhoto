import type { RulerDigitsProps } from "./RulerDigits.types";

export function RulerDigits({ value }: RulerDigitsProps) {
  const percent = Math.round(Math.min(100, Math.max(0, Number.isFinite(value) ? value : 0)));
  return <span className="ruler-digits">{String(percent).padStart(2, "0")}</span>;
}
