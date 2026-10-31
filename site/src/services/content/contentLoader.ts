import { parse } from "yaml";
import { readFile } from "node:fs/promises";
import { join } from "node:path";
import type {
  FeaturesContent,
  FooterContent,
  HeroContent,
  MarqueeContent,
  NavigationContent,
  OwnershipContent,
  SiteContent,
  StartContent,
} from "@/types/content";

const CONTENT_DIR = join(process.cwd(), "src", "content");

async function loadYamlFile<T>(fileName: string): Promise<T> {
  const raw = await readFile(join(CONTENT_DIR, fileName), "utf-8");
  return parse(raw) as T;
}

export async function loadSiteContent(): Promise<SiteContent> {
  return loadYamlFile<SiteContent>("site.yaml");
}

export async function loadNavigationContent(): Promise<NavigationContent> {
  return loadYamlFile<NavigationContent>("navigation.yaml");
}

export async function loadHeroContent(): Promise<HeroContent> {
  return loadYamlFile<HeroContent>("hero.yaml");
}

export async function loadMarqueeContent(): Promise<MarqueeContent> {
  return loadYamlFile<MarqueeContent>("marquee.yaml");
}

export async function loadFeaturesContent(): Promise<FeaturesContent> {
  return loadYamlFile<FeaturesContent>("features.yaml");
}

export async function loadOwnershipContent(): Promise<OwnershipContent> {
  return loadYamlFile<OwnershipContent>("ownership.yaml");
}

export async function loadStartContent(): Promise<StartContent> {
  return loadYamlFile<StartContent>("start.yaml");
}

export async function loadFooterContent(): Promise<FooterContent> {
  return loadYamlFile<FooterContent>("footer.yaml");
}
