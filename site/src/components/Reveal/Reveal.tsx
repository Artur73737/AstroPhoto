"use client";

import { useReveal } from "@/hooks/useReveal";
import { classNames } from "@/utils";
import type { RevealProps } from "./Reveal.types";

export function Reveal({ children, className, delay = 0 }: RevealProps) {
  const { reference, visible } = useReveal<HTMLDivElement>();
  return (
    <div
      ref={reference}
      data-visible={visible}
      className={classNames("reveal-item", className)}
      style={delay ? { animationDelay: `${delay}ms` } : undefined}
    >
      {children}
    </div>
  );
}
