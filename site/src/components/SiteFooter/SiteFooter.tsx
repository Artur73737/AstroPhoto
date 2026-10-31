import { publicAsset } from "@/lib/paths";
import Image from "next/image";
import type { SiteFooterProps } from "./SiteFooter.types";

export function SiteFooter({ site, footer }: SiteFooterProps) {
  return (
    <footer className="site-footer border-t border-[var(--line)]">
      <div className="mx-auto max-w-[1280px] border-x border-[var(--line)]">
        <div className="grid gap-px bg-[var(--line)] md:grid-cols-4 lg:grid-cols-[2fr_1fr_1fr_1fr_1fr]">
          <div className="bg-[var(--bg)] p-6 md:col-span-4 md:p-10 lg:col-span-1">
            <Image alt={site.brand.name} src={publicAsset("/logo.svg")} width={32} height={32} className="themed-logo h-8 w-auto" />
            <p className="mt-4 max-w-xs leading-relaxed text-[var(--muted)]">{footer.tagline}</p>
            <a href="https://github.com/Artur73737/AstroPhoto/issues" className="text-link mt-6 inline-block">Feedback &amp; issues ↗</a>
          </div>
          {footer.columns.map((column) => (
            <nav key={column.heading} aria-label={column.heading} className="bg-[var(--bg)] p-6 md:p-8">
              <p className="hud-label text-[var(--faint)]">{column.heading}</p>
              <ul className="mt-4 flex flex-col gap-2.5">
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
        <div className="flex flex-col gap-2 border-t border-[var(--line)] px-6 py-4 md:flex-row md:items-center md:justify-between md:px-10">
          <p className="hud-label text-[var(--faint)]">{footer.legal.copyright}</p>
          <p className="hud-label text-[var(--faint)]">{footer.legal.credit}</p>
        </div>
      </div>
    </footer>
  );
}
