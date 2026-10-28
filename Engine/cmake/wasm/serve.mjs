import { spawn } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, statSync,
         writeFileSync } from "node:fs";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import { join, normalize, sep } from "node:path";
import { fileURLToPath } from "node:url";

const here = fileURLToPath (new URL (".", import.meta.url));
const config = JSON.parse (readFileSync (join (here, "wasm_run.json"), "utf8"));
const root = normalize (config.root);

const options = { open: false, headed: false, port: 8713, timeout: 1800,
                  collect: [], collect_out: null, run_args: [] };
for (let i = 2; i < process.argv.length; ++i) {
  const arg = process.argv[i];
  if (arg === "--") { options.run_args = process.argv.slice (i + 1); break; }
  else if (arg === "--open") options.open = true;
  else if (arg === "--headed") options.headed = true;
  else if (arg === "--port") options.port = Number (process.argv[++i]);
  else if (arg === "--timeout") options.timeout = Number (process.argv[++i]);
  else if (arg === "--collect") options.collect.push (process.argv[++i]);
  else if (arg === "--collect-out") options.collect_out = process.argv[++i];
  else {
    console.error (`serve.mjs: unknown argument ${arg}\n`
                   + "usage: node serve.mjs [--open] [--headed] [--port N] [--timeout S] "
                   + "[--collect GUEST_PATH]... [--collect-out DIR] [-- RUN_ARGS...]");
    process.exit (2);
  }
}

for (const guest of config.writable)
  mkdirSync (root + guest, { recursive: true });

const run = {
  env: { GSE_RUN_ARGS: options.run_args.join (" "), ...config.environment },
  initial_pages: config.initial_pages,
  max_pages: config.max_pages,
  collect: options.collect,
};

const pages = {
  "/": ["index.html", "text/html"],
  "/index.html": ["index.html", "text/html"],
  "/page.mjs": ["page.mjs", "text/javascript"],
  "/guest.mjs": ["guest.mjs", "text/javascript"],
  "/host.mjs": ["host.mjs", "text/javascript"],
  "/protocol.mjs": ["protocol.mjs", "text/javascript"],
};

function writable (guest) {
  return config.writable.some (w => guest === w || guest.startsWith (w + "/"));
}

function host_path (guest) {
  const path = normalize (root + guest);
  return path === root || path.startsWith (root.replace (/[\\/]+$/, "") + sep) ? path : null;
}

function listing (guest) {
  if (writable (guest)) return [];
  const path = host_path (guest);
  if (path === null) return null;
  try {
    return readdirSync (path, { withFileTypes: true }).map (entry => ({
      name: entry.name,
      dir: entry.isDirectory (),
      size: entry.isFile () ? statSync (join (path, entry.name)).size : 0,
    }));
  } catch {
    return null;
  }
}

function contents (guest) {
  if (writable (guest)) return null;
  const path = host_path (guest);
  if (path === null) return null;
  try {
    return readFileSync (path);
  } catch {
    return null;
  }
}

let browser = null, profile = null;
const collected = new Set ();

function finish (code) {
  if (browser !== null) browser.kill ();
  const missing = options.collect.filter (g => !collected.has (g));
  for (const guest of missing) console.log (`serve: NOT COLLECTED ${guest}`);
  const done = () => {
    if (profile !== null) {
      try {
        rmSync (profile, { recursive: true, force: true, maxRetries: 20, retryDelay: 250 });
      } catch (e) {
        console.log (`serve: browser profile ${profile} left behind: ${e.code}`);
      }
    }
    process.exit (code);
  };
  if (browser !== null && browser.exitCode === null) browser.once ("exit", done);
  else done ();
}

function send (response, status, body, kind) {
  response.writeHead (status, {
    "Content-Type": kind,
    "Content-Length": body.length,
    "Cross-Origin-Opener-Policy": "same-origin",
    "Cross-Origin-Embedder-Policy": "require-corp",
    "Cross-Origin-Resource-Policy": "same-origin",
    "Cache-Control": "no-store",
  });
  response.end (body);
}

function body (request) {
  return new Promise ((resolve) => {
    const chunks = [];
    request.on ("data", c => chunks.push (c));
    request.on ("end", () => resolve (Buffer.concat (chunks)));
  });
}

const server = createServer (async (request, response) => {
  const url = new URL (request.url, "http://localhost");
  const path = decodeURIComponent (url.pathname);
  if (request.method === "GET") {
    if (path in pages) {
      const [name, kind] = pages[path];
      return send (response, 200, readFileSync (join (here, "page", name)), kind);
    }
    if (path === "/scenario.wasm")
      return send (response, 200, readFileSync (join (here, config.module)), "application/wasm");
    if (path === "/run.json")
      return send (response, 200, Buffer.from (JSON.stringify (run)), "application/json");
    if (path.startsWith ("/fs-list/")) {
      const entries = listing (path.slice ("/fs-list".length));
      return entries === null ? send (response, 404, Buffer.alloc (0), "text/plain")
        : send (response, 200, Buffer.from (JSON.stringify (entries)), "application/json");
    }
    if (path.startsWith ("/fs/")) {
      const bytes = contents (path.slice ("/fs".length));
      return bytes === null ? send (response, 404, Buffer.alloc (0), "text/plain")
        : send (response, 200, bytes, "application/octet-stream");
    }
    return send (response, 404, Buffer.alloc (0), "text/plain");
  }
  const data = await body (request);
  if (path === "/console") {
    process.stdout.write (data);
    return send (response, 200, Buffer.alloc (0), "text/plain");
  }
  if (path === "/relaunch") {
    console.log (`serve: page relaunching with ${data.toString ("utf8")}`);
    return send (response, 200, Buffer.alloc (0), "text/plain");
  }
  if (path === "/collect") {
    const guest = url.searchParams.get ("path") ?? "";
    collected.add (guest);
    if (options.collect_out !== null) {
      mkdirSync (options.collect_out, { recursive: true });
      writeFileSync (join (options.collect_out, guest.split ("/").pop () || "collected"), data);
    }
    return send (response, 200, Buffer.alloc (0), "text/plain");
  }
  if (path === "/result") {
    const verdict = JSON.parse (data.toString ("utf8"));
    send (response, 200, Buffer.alloc (0), "text/plain");
    console.log (`serve: entry returned ${verdict.code} (${verdict.reason})`);
    const missing = options.collect.some (g => !collected.has (g));
    return finish (verdict.code === 0 && !missing ? 0 : 1);
  }
  send (response, 404, Buffer.alloc (0), "text/plain");
});

function find_browser () {
  if (process.env.GSE_CHROME) return process.env.GSE_CHROME;
  const candidates = [
    "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
    "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
    "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
    "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
    "/usr/bin/google-chrome",
    "/usr/bin/chromium",
  ];
  const found = candidates.find (c => existsSync (c));
  if (found === undefined) {
    console.error ("serve: no Chrome or Edge found; set GSE_CHROME to a browser executable");
    process.exit (2);
  }
  return found;
}

server.listen (options.port, "127.0.0.1", () => {
  const url = `http://127.0.0.1:${options.port}/`;
  console.log (`serve: ${url} serving ${join (here, config.module)} over ${root}`);
  if (!options.open) return;
  profile = mkdtempSync (join (tmpdir (), "gse-wasm-page-"));
  const args = [`--user-data-dir=${profile}`, "--no-first-run", "--no-default-browser-check",
                "--disable-extensions", "--autoplay-policy=no-user-gesture-required", url];
  if (!options.headed) args.unshift ("--headless=new");
  browser = spawn (find_browser (), args, { stdio: "ignore" });
});

setTimeout (() => {
  console.log (`serve: no verdict within ${options.timeout}s`);
  finish (1);
}, options.timeout * 1000).unref ();
