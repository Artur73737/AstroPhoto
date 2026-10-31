"use client";
import { useRef } from "react";
import type { MobileMenuProps } from "./MobileMenu.types";
export function MobileMenu({ open, content, onClose, onToggle, openLabel, closeLabel }: MobileMenuProps) {
  const toggle = useRef<HTMLButtonElement>(null);
  const links = [...content.main, ...content.resourcesMenu.items, content.featured];
  return <div className="mobile-navigation" onKeyDown={event => { if (event.key === "Escape" && open) { event.stopPropagation(); onClose(); toggle.current?.focus(); } }}>
    <button ref={toggle} type="button" className="mobile-menu-toggle" aria-expanded={open} aria-controls="mobile-nav" aria-label={open ? closeLabel : openLabel} onClick={onToggle}>
      <svg width="16" height="16" viewBox="0 0 20 20" fill="none" aria-hidden="true">{open ? <path d="m5 5 10 10M15 5 5 15" stroke="currentColor" strokeWidth="1.4"/> : <path d="M3 6h14M3 10h14M3 14h14" stroke="currentColor" strokeWidth="1.4"/>}</svg>
    </button>
    {open && <nav id="mobile-nav" aria-label="Mobile main menu"><ul>{links.map(link => <li key={link.href}><a href={link.href} onClick={onClose}>{link.label}<span aria-hidden="true">↗</span></a></li>)}</ul></nav>}
  </div>;
}
