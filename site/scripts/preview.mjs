import http from "node:http";
import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
const root = path.resolve(fileURLToPath(new URL("../out/", import.meta.url)));
const base = (process.env.NEXT_PUBLIC_BASE_PATH || "").replace(/\/$/, "");
const types = { ".html": "text/html; charset=utf-8", ".css": "text/css", ".js": "text/javascript", ".json": "application/json", ".svg": "image/svg+xml", ".png": "image/png", ".woff2": "font/woff2", ".txt": "text/plain" };
const server = http.createServer(async (req,res) => {
  try {
    const url = new URL(req.url, "http://localhost");
    if (base && !url.pathname.startsWith(base + "/")) { res.writeHead(404).end(); return; }
    const file = path.resolve(root, "." + decodeURIComponent(url.pathname.slice(base.length)));
    if (file !== root && !file.startsWith(root + path.sep)) { res.writeHead(403).end(); return; }
    const info = await stat(file);
    const target = info.isDirectory() ? path.join(file, "index.html") : file;
    res.writeHead(200, { "Content-Type": types[path.extname(target)] || "application/octet-stream" });
    res.end(await readFile(target));
  } catch { res.writeHead(404).end("Not found"); }
});
server.listen(Number(process.env.PORT || 4173), "127.0.0.1", () => console.log(`Local: http://127.0.0.1:${server.address().port}${base}/`));
