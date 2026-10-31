import type { ReactNode } from "react";

export interface CallToActionButtonProps {
  label: string;
  href: string;
  variant?: "primary" | "secondary";
  leading?: ReactNode;
}
