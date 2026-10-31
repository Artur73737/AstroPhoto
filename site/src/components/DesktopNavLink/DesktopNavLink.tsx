import type { DesktopNavLinkProps } from "./DesktopNavLink.types";

export function DesktopNavLink({ item }: DesktopNavLinkProps) {
  return (
    <li className="flex items-stretch">
      <a
        href={item.href}
        aria-current={item.active ? "page" : undefined}
        className={`group flex h-full items-center px-2.5 hud-label whitespace-nowrap ${item.active ? "text-[var(--ink)]" : "text-[var(--muted)] hover:text-[var(--ink)]"}`}
      >
        <span
          className={`flex items-center gap-1.5 px-1.5 py-0.5 group-hover:bg-[var(--invert-bg)] group-hover:text-[var(--invert-fg)] ${item.active ? "bg-[var(--invert-bg)] text-[var(--invert-fg)]" : ""}`}
        >
          {item.label}
        </span>
      </a>
    </li>
  );
}
