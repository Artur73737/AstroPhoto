"use client";

import { useEffect, useRef, useState } from "react";

export function useReveal<T extends HTMLElement>(threshold = 0.15): {
  reference: React.RefObject<T | null>;
  visible: boolean;
} {
  const reference = useRef<T | null>(null);
  // Always start hidden so server and first client render match.
  const [visible, setVisible] = useState(false);

  useEffect(() => {
    const node = reference.current;
    if (!node) return;
    if (typeof IntersectionObserver === "undefined") {
      // eslint-disable-next-line react-hooks/set-state-in-effect -- fallback when observer is unavailable
      setVisible(true);
      return;
    }
    const observer = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) {
          if (entry.isIntersecting) {
            setVisible(true);
            observer.disconnect();
          }
        }
      },
      { threshold },
    );
    observer.observe(node);
    return () => observer.disconnect();
  }, [threshold]);

  return { reference, visible };
}
