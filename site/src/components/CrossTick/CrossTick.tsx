import type { CrossTickProps } from "./CrossTick.types";

const POSITION_STYLES: Record<CrossTickProps["position"], string> = {
  "top-left": "-top-[5px] -left-[6px]",
  "top-right": "-top-[5px] -right-[5px]",
  "bottom-left": "-bottom-[5px] -left-[6px]",
  "bottom-right": "-bottom-[5px] -right-[5px]",
};

export function CrossTick({ position }: CrossTickProps) {
  return (
    <span
      aria-hidden="true"
      className={`tick tick-half z-[31] ${POSITION_STYLES[position]}`}
    />
  );
}
