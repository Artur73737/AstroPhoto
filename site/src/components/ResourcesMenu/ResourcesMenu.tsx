"use client";
import { useRef } from "react";
import type { ResourcesMenuProps } from "./ResourcesMenu.types";
export function ResourcesMenu({ menu, open, onToggle }: ResourcesMenuProps) {
  const toggle = useRef<HTMLButtonElement>(null);
  return <li className="resources-menu" onKeyDown={event => {
    if (event.key === "Escape" && open) { event.stopPropagation(); onToggle(); toggle.current?.focus(); }
    if (event.key === "ArrowDown" && event.target === toggle.current) {
      event.preventDefault();
      if (!open) onToggle();
      requestAnimationFrame(() => toggle.current?.parentElement?.querySelector<HTMLAnchorElement>("ul a")?.focus());
    }
  }}>
    <button ref={toggle} type="button" aria-expanded={open} aria-controls="resources-nav" onClick={onToggle}>{menu.label}<svg aria-hidden="true" width="12" height="12" viewBox="0 0 12 12" fill="none"><path d="m3 4.5 3 3 3-3" stroke="currentColor" strokeWidth="1.3"/></svg></button>
    {open && <ul id="resources-nav" className="resources-popover">{menu.items.map(item => <li key={item.href}><a href={item.href} onClick={onToggle}><span>{item.label}</span><small>{item.description}</small></a></li>)}</ul>}
  </li>;
}
