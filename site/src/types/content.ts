export interface CtaLink {
  label: string;
  href: string;
}

export interface EyebrowContent {
  index: string;
  section: string;
  aside: string;
}

export interface NavigationItem {
  label: string;
  href: string;
  active?: boolean;
  description?: string;
}

export interface NavigationContent {
  main: NavigationItem[];
  trailing: NavigationItem[];
  featured: NavigationItem;
  secondary: NavigationItem[];
  social: NavigationItem;
  actions: {
    primaryCta: CtaLink;
    themeToggleLabel: string;
    menuOpenLabel: string;
    menuCloseLabel: string;
  };
  resourcesMenu: {
    label: string;
    items: NavigationItem[];
  };
}

export interface HeroContent {
  titleStart: string;
  titleAccent: string;
  description: string;
  primaryCta: CtaLink;
  secondaryCta: CtaLink;
  sourceLink: CtaLink;
  platformNote: string;
}

export interface MarqueeContent {
  label: string;
  items: string[];
}

export interface FeatureItem {
  id: string;
  index: string;
  category: string;
  title: string;
  description: string;
}

export interface FeaturesContent {
  eyebrow: EyebrowContent;
  headingStart: string;
  headingAccent: string;
  headingEnd: string;
  description: string;
  items: FeatureItem[];
}

export interface OwnershipItem {
  id: string;
  marker: string;
  title: string;
  description: string;
  linkLabel?: string;
  linkHref?: string;
}

export interface OwnershipContent {
  eyebrow: EyebrowContent;
  headingStart: string;
  headingAccent: string;
  headingEnd: string;
  description: string;
  items: OwnershipItem[];
}

export interface StartContent {
  caption: string;
  headingStart: string;
  headingAccent: string;
  headingEnd: string;
  description: string;
  primaryCta: CtaLink;
  secondaryCta: CtaLink;
}

export interface FooterContent {
  tagline: string;
  columns: { heading: string; links: NavigationItem[] }[];
  legal: { copyright: string; credit: string };
}

export interface SiteContent {
  brand: { name: string; tagline: string; logoAlt: string };
  meta: { title: string; description: string; locale: string };
  ruler: { enabled: boolean; side: string; railWidth: number; label: string };
}
