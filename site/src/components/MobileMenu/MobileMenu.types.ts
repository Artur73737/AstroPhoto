import type { NavigationContent } from "@/types/content";

export interface MobileMenuProps {
  open: boolean;
  content: NavigationContent;
  onClose: () => void;
  onToggle: () => void;
  openLabel: string;
  closeLabel: string;
}
