"use client";

import { useReveal } from "@/hooks/useReveal";
import type { RuleLineProps } from "./RuleLine.types";

export function RuleLine({ placement = "top" }: RuleLineProps) {
  const { reference, visible } = useReveal<HTMLSpanElement>();
  const placementStyle = placement === "top" ? "top-0" : "bottom-0";
  return (
    <span
      ref={reference}
      aria-hidden="true"
      data-visible={visible}
      className={`draw-rule absolute inset-x-0 ${placementStyle} h-px origin-left bg-[var(--line)]`}
    />
  );
}
