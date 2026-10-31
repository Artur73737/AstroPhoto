import type { NavigationContent } from "@/types/content";

export interface ResourcesMenuProps {
  menu: NavigationContent["resourcesMenu"];
  open: boolean;
  onToggle: () => void;
}
