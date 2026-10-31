import type { NextConfig } from "next";

// Compile CSS through postcss.config.mjs so stylesheet edits invalidate the dev output.
const nextConfig: NextConfig = {
  output: "export",
  trailingSlash: true,
  basePath: process.env.NEXT_PUBLIC_BASE_PATH || "",
  images: { unoptimized: true },
};

export default nextConfig;
