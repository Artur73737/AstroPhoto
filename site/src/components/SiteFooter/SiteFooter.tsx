"use client";
import { useState } from "react";
import { publicAsset } from "@/lib/paths";
import Image from "next/image";
import type { SiteFooterProps } from "./SiteFooter.types";

export function SiteFooter({ site, footer }: SiteFooterProps) {
  const [expandedGroups, setExpandedGroups] = useState<Set<number>>(() => new Set());
  return (
    <footer className="site-footer border-t border-[var(--line)]">
      <div className="mx-auto max-w-[1280px] border-x border-[var(--line)]">
        <div className="footer-grid grid gap-px bg-[var(--line)] md:grid-cols-4 lg:grid-cols-[2fr_1fr_1fr_1fr_1fr]">
          <div className="footer-intro bg-[var(--bg)] p-6 md:col-span-4 md:p-10 lg:col-span-1">
            <div className="footer-brand"><Image alt={site.brand.name} src={publicAsset("/logo.svg")} width={32} height={32} className="themed-logo h-8 w-auto" /><span aria-hidden="true">{site.brand.name}</span></div>
            <p className="mt-4 max-w-xs leading-relaxed text-[var(--muted)]">{footer.tagline}</p>
            <a href="https://github.com/Artur73737/AstroPhoto/issues" className="text-link mt-6 inline-block">Feedback &amp; issues ↗</a>
          </div>
          {footer.columns.map((column, index) => (
            <nav key={column.heading} aria-label={column.heading} className="footer-group bg-[var(--bg)] p-6 md:p-8" data-expanded={expandedGroups.has(index)}>
              <p className="footer-group-heading hud-label text-[var(--faint)]">{column.heading}</p>
              <button type="button" className="footer-group-toggle" aria-expanded={expandedGroups.has(index)} aria-controls={`footer-links-${index}`} onClick={() => setExpandedGroups(previous => {
                const next = new Set(previous);
                if (next.has(index)) next.delete(index); else next.add(index);
                return next;
              })}>{column.heading}<span aria-hidden="true">{expandedGroups.has(index) ? "−" : "+"}</span></button>
              <ul id={`footer-links-${index}`} className="mt-4 flex flex-col gap-2.5">
                {column.links.map((link) => (
                  <li key={link.label}>
                    <a href={link.href} className="text-[var(--muted)] hover:text-[var(--ink)]">
                      {link.label}
                    </a>
                  </li>
                ))}
              </ul>
            </nav>
          ))}
        </div>
        <div className="footer-legal flex flex-col gap-2 border-t border-[var(--line)] px-6 py-4 md:flex-row md:items-center md:justify-between md:px-10">
          <p className="hud-label text-[var(--faint)]">{footer.legal.copyright}</p>
          <p className="hud-label text-[var(--faint)]">{footer.legal.credit}</p>
        </div>
      </div>
    </footer>
  );
}
