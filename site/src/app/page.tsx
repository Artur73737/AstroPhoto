import { FeaturesSection } from "@/components/FeaturesSection";
import { HeroSection } from "@/components/HeroSection";
import { WorkspaceSection } from "@/components/WorkspaceSection";
import { AndroidSection } from "@/components/AndroidSection";
import { DownloadsSection } from "@/components/DownloadsSection";
import releaseSnapshot from "@/content/releases.json";
import { DocumentationSection } from "@/components/DocumentationSection";
import { OwnershipSection } from "@/components/OwnershipSection";
import { SiteChrome } from "@/components/SiteChrome";
import { SiteFooter } from "@/components/SiteFooter";
import { SiteNavigation } from "@/components/SiteNavigation";
import { StartSection } from "@/components/StartSection";
import {
  loadFeaturesContent,
  loadFooterContent,
  loadHeroContent,
  loadNavigationContent,
  loadOwnershipContent,
  loadSiteContent,
  loadStartContent,
} from "@/services/content";

export default async function Home() {
  const site = await loadSiteContent();
  const navigation = await loadNavigationContent();
  const hero = await loadHeroContent();
  const features = await loadFeaturesContent();
  const ownership = await loadOwnershipContent();
  const start = await loadStartContent();
  const footer = await loadFooterContent();

  return (
    <SiteChrome scrollbarLabel={site.ruler.label}>
      <div id="top">
        <SiteNavigation site={site} navigation={navigation} />
        <main id="main">
          <HeroSection content={hero} brandName={site.brand.name} />
          <WorkspaceSection />
          <AndroidSection />
          <FeaturesSection content={features} />
          <OwnershipSection content={ownership} />
          <DownloadsSection initial={releaseSnapshot} />
          <DocumentationSection />
          <StartSection content={start} />
        </main>
        <SiteFooter site={site} footer={footer} />
      </div>
    </SiteChrome>
  );
}
