import { spawn } from "node:child_process";
import { mkdirSync, readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { run } from "./node_host.mjs";

function relaunch (line) {
  const args = line.split (" ").filter (a => a !== "");
  console.log (`relaunch: ${args.join (" ")}`);
  return new Promise ((resolve) => {
    spawn (process.execPath, [fileURLToPath (import.meta.url), ...args], { stdio: "inherit" })
      .on ("exit", (code) => resolve (code ?? 1));
  });
}

const config = JSON.parse (readFileSync (new URL ("./wasm_run.json", import.meta.url), "utf8"));

for (const guest of config.writable)
  mkdirSync (config.root + guest, { recursive: true });

const mounts = {};
if (process.env.LOCALAPPDATA) {
  const logs = process.env.LOCALAPPDATA + "/GSE/logs";
  mkdirSync (logs, { recursive: true });
  mounts[config.environment.GSE_STATE_DIR + "/logs"] = logs;
}

await run ({
  module: new URL (config.module, import.meta.url),
  shared: true,
  root: config.root,
  mounts,
  env: { GSE_RUN_ARGS: process.argv.slice (2).join (" "), ...config.environment },
  initial_pages: config.initial_pages,
  max_pages: config.max_pages,
  relaunch,
});
