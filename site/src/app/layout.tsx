import type { Metadata } from "next";
import captures from "@/content/screenshots.json";
import { Archivo, Geist_Mono, Instrument_Serif, Inter } from "next/font/google";
import "./globals.css";

const displayFont = Archivo({
  subsets: ["latin"],
  variable: "--font-display",
  weight: ["400", "500", "600", "700"],
});

const bodyFont = Inter({
  subsets: ["latin"],
  variable: "--font-body",
  weight: ["400", "500", "600"],
});

const serifFont = Instrument_Serif({
  subsets: ["latin"],
  variable: "--font-serif",
  weight: ["400"],
});

const monoFont = Geist_Mono({
  subsets: ["latin"],
  variable: "--font-mono",
  weight: ["400", "500", "700"],
});

export const metadata: Metadata = {
  metadataBase: new URL("https://artur73737.github.io/AstroPhoto/"),
  title: "AstroPhoto — Your camera. Your night sky.",
  description: "Canon EOS control on Windows and Android: Turbo Live View, point autofocus, capture sequences and MSI/APK releases with GitHub SHA-256 hashes.",
  icons: { icon: `${process.env.NEXT_PUBLIC_BASE_PATH || ""}/logo.svg` },
  openGraph: {
    title: "AstroPhoto — Your camera. Your night sky.",
    description: "A focused Windows workspace for Canon EOS astrophotography, written in C++20.",
    images: [{ url: "screenshots/all.png", width: captures.images["all.png"].width, height: captures.images["all.png"].height, alt: "Complete AstroPhoto application workspace" }],
    type: "website",
  },
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body
        className={`${displayFont.variable} ${bodyFont.variable} ${serifFont.variable} ${monoFont.variable} antialiased`}
      >
        <a href="#main" className="skip-link">Skip to content</a>
        {children}
      </body>
    </html>
  );
}
