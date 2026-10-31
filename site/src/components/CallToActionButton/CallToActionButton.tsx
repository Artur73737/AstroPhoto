import type { CallToActionButtonProps } from "./CallToActionButton.types";

export function CallToActionButton({ label, href, variant = "primary", leading }: CallToActionButtonProps) {
  const variantStyle =
    variant === "primary"
      ? "bg-[var(--invert-bg)] text-[var(--invert-fg)] hover:opacity-80"
      : "invert-block border border-[var(--line-strong)] bg-[var(--bg)] text-[var(--ink)] hover:border-transparent";
  return (
    <a
      href={href}
      data-cta=""
      className={`compact-cta inline-flex w-fit items-center justify-center gap-2 rounded-none font-mono text-[9px] font-medium whitespace-nowrap uppercase tracking-[0.1em] duration-150 ${variantStyle}`}
    >
      {leading}
      {label}
    </a>
  );
}
