"use client";
import Image from "next/image";
import { useCallback, useEffect, useRef, useState } from "react";
import { publicAsset } from "@/lib/paths";
import { useTheme } from "@/hooks/useTheme";
import { useMobileMenu } from "@/hooks/useMobileMenu";
import { MobileMenu } from "@/components/MobileMenu";
import { ResourcesMenu } from "@/components/ResourcesMenu";
import { ThemeToggle } from "@/components/ThemeToggle";
import type { SiteNavigationProps } from "./SiteNavigation.types";

export function SiteNavigation({ site, navigation }: SiteNavigationProps) {
  const { mode, toggleTheme } = useTheme();
  const menu = useMobileMenu();
  const [resourcesOpen, setResourcesOpen] = useState(false);
  const header = useRef<HTMLElement>(null);
  const toggleResources = useCallback(() => setResourcesOpen(value => !value), []);
  useEffect(() => {
    const closeOutside = (event: PointerEvent) => {
      if (!header.current?.contains(event.target as Node)) { setResourcesOpen(false); menu.closeMenu(); }
    };
    const closeOnResize = () => { setResourcesOpen(false); menu.closeMenu(); };
    document.addEventListener("pointerdown", closeOutside);
    window.addEventListener("resize", closeOnResize);
    return () => { document.removeEventListener("pointerdown", closeOutside); window.removeEventListener("resize", closeOnResize); };
  }, [menu.closeMenu]);
  return <header ref={header} id="site-nav" className="site-navigation" onKeyDown={event => { if (event.key === "Escape") { setResourcesOpen(false); menu.closeMenu(); } }}>
    <div className="navigation-inner">
      <a className="brand-link" href="#top" aria-label={site.brand.logoAlt}><Image src={publicAsset("/logo.svg")} width={20} height={20} alt="" /></a>
      <nav className="desktop-navigation" aria-label="Main"><ul>
        {navigation.main.map(item => <li key={item.href}><a href={item.href}>{item.label}</a></li>)}
        <ResourcesMenu menu={navigation.resourcesMenu} open={resourcesOpen} onToggle={toggleResources} />
        <li><a href={navigation.featured.href}>{navigation.featured.label}</a></li>
      </ul></nav>
      <div className="navigation-actions">
        <ThemeToggle label={navigation.actions.themeToggleLabel} mode={mode} onToggle={toggleTheme} />
        <a className="navigation-download" href={navigation.actions.primaryCta.href}>{navigation.actions.primaryCta.label}<span aria-hidden="true">↓</span></a>
        <MobileMenu open={menu.open} content={navigation} onClose={menu.closeMenu} onToggle={menu.toggleMenu} openLabel={navigation.actions.menuOpenLabel} closeLabel={navigation.actions.menuCloseLabel} />
      </div>
    </div>
  </header>;
}
