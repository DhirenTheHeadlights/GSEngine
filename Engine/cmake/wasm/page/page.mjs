import { CTRL_BYTES, TEXT_BYTES, SYNTHETIC_CALLS } from "./protocol.mjs";

const out = document.getElementById ("out");
const lines = [];
let pending = [];
let finished = false;

function say (text) {
  for (const line of text.split ("\n")) {
    lines.push (line);
    pending.push (line);
    console.log (line);
  }
  out.textContent = lines.slice (-400).join ("\n");
  out.scrollTop = out.scrollHeight;
}

async function flush () {
  if (pending.length === 0) return;
  const batch = pending;
  pending = [];
  await fetch ("/console", { method: "POST", body: batch.join ("\n") + "\n" });
}

async function finish (code, reason) {
  if (finished) return;
  finished = true;
  await flush ();
  await fetch ("/result", {
    method: "POST",
    body: JSON.stringify ({ code, reason, lines: lines.length }),
  });
}

async function main () {
  setInterval (flush, 250);
  if (!self.crossOriginIsolated) {
    say ("page: not cross-origin isolated, so SharedArrayBuffer is unavailable "
         + "-- the server must send COOP and COEP");
    await finish (3, "not cross-origin isolated");
    return;
  }

  const run = await fetch ("run.json").then (r => r.json ());
  const env = run.env;
  const relaunched = new URLSearchParams (location.search).get ("args");
  if (relaunched !== null) {
    env.GSE_RUN_ARGS = relaunched;
    say (`page: relaunched with ${relaunched}`);
  }
  const module = await WebAssembly.compileStreaming (
    fetch ("scenario.wasm", { headers: { "accept": "application/wasm" } }));

  const imports = WebAssembly.Module.imports (module);
  const declared = imports.find (i => i.kind === "memory");
  const mem = new WebAssembly.Memory ({ initial: run.initial_pages,
                                        maximum: run.max_pages, shared: true });
  say (`page: module compiled, memory ${declared ? "imported" : "exported"}, `
       + `${run.initial_pages} initial pages of ${run.max_pages}`);

  const names = [...imports.filter (i => i.module.startsWith ("wasi_snapshot"))
                          .map (i => i.name),
                 ...SYNTHETIC_CALLS];
  const ctrl = new SharedArrayBuffer (CTRL_BYTES);
  const text = new SharedArrayBuffer (TEXT_BYTES);
  const ids = new Int32Array (new SharedArrayBuffer (4));
  ids[0] = 1;

  const threads = [];
  const start = (init) => {
    const worker = new Worker ("guest.mjs", { type: "module" });
    worker.onerror = (e) => say (`page: thread ${init.tid ?? 1} worker error: `
                                 + `${e.message} (${e.filename}:${e.lineno})`);
    worker.postMessage ({ module, mem, ctrl, text, names, env, ids, ...init });
    threads.push (worker);
  };

  const host = new Worker ("host.mjs", { type: "module" });
  host.onerror = (e) => say (`page: host worker error: ${e.message}`);

  host.onmessage = async (e) => {
    if (e.data.kind === "console") say (e.data.text);
    else if (e.data.kind === "ready") {
      say ("page: host ready, files are fetched as the run opens them");
      start ({ kind: "main", tid: 1 });
    } else if (e.data.kind === "spawn") {
      start ({ kind: "thread", tid: e.data.tid, arg: e.data.arg });
    } else if (e.data.kind === "open_url") {
      say (`page: open_url ${e.data.url}`);
      window.open (e.data.url, "_blank", "noopener");
    } else if (e.data.kind === "exit") {
      for (const file of e.data.files ?? []) {
        say (`page: collecting ${file.path} (${file.bytes.length} bytes)`);
        await fetch (`/collect?path=${encodeURIComponent (file.path)}`,
                     { method: "POST", body: file.bytes });
      }
      if (e.data.relaunch !== null) {
        say (`page: relaunching with ${e.data.relaunch}`);
        await flush ();
        await fetch ("/relaunch", { method: "POST", body: e.data.relaunch });
        location.replace (`?args=${encodeURIComponent (e.data.relaunch)}`);
        return;
      }
      await finish (e.data.code, "entry returned");
    }
  };
  host.postMessage ({ mem, ctrl, text, names, collect: run.collect });
}

main ().catch (async (e) => {
  say (`page: ${e.message}\n${e.stack}`);
  await finish (3, "page failed");
});
