"use client";

import { useEffect } from "react";
import { SideRuler } from "@/components/SideRuler";
import { useRulerProgress } from "@/hooks/useRulerProgress";
import type { SiteChromeProps } from "./SiteChrome.types";

export function SiteChrome({ children, scrollbarLabel }: SiteChromeProps) {
  useEffect(() => {
    document.documentElement.dataset.ruler = "on";
    return () => { delete document.documentElement.dataset.ruler; };
  }, []);
  const { progress, scrollY, documentHeight, viewportHeight } = useRulerProgress();
  return (
    <>
      <SideRuler
        progress={progress}
        scrollY={scrollY}
        documentHeight={documentHeight}
        viewportHeight={viewportHeight}
        label={scrollbarLabel}
      />
      {children}
    </>
  );
}
