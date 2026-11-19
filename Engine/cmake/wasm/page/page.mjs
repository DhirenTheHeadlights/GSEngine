import {
  CTRL_BYTES, TEXT_BYTES, SYNTHETIC_CALLS,
  SURFACE_SIZE, SURFACE_VISIBLE, SURFACE_CAPTURE, SURFACE_LOCKED, SURFACE_RESIZE_SEQ, SURFACE_MARK, SURFACE_SLOTS,
  INPUT_HEAD, INPUT_TAIL, INPUT_DROPPED, INPUT_HEADER_BYTES, INPUT_RECORD_BYTES, INPUT_CAPACITY,
  INPUT_CODE_OFFSET, INPUT_CODE_BYTES, INPUT_KEY_DOWN, INPUT_KEY_UP, INPUT_BUTTON_DOWN,
  INPUT_BUTTON_UP, INPUT_MOVE, INPUT_WHEEL, INPUT_TEXT, INPUT_RAW_MOVE, bump,
  make_ring, GPU_RING_WORDS, GPU_EVENT_WORDS,
} from "./protocol.mjs";

const out = document.getElementById ("out");
const lines = [];
let pending = [];
let finished = false;
let painting = false;

function say (text) {
  for (const line of text.split ("\n")) {
    lines.push (line);
    pending.push (line);
    console.log (line);
  }
  if (painting) return;
  painting = true;
  requestAnimationFrame (() => {
    painting = false;
    out.textContent = lines.slice (-400).join ("\n");
    out.scrollTop = out.scrollHeight;
  });
}

async function flush () {
  if (pending.length === 0) return;
  const batch = pending;
  pending = [];
  await fetch ("/console", { method: "POST", body: batch.join ("\n") + "\n" });
}

let persisting = Promise.resolve ();

function segments (path) {
  return path.split ("/").filter (part => part !== "");
}

async function opfs () {
  const top = await navigator.storage.getDirectory ();
  const folder = async (path, create) => {
    let at = top;
    for (const part of segments (path)) at = await at.getDirectoryHandle (part, { create });
    return at;
  };
  const walk = async (handle, path, stored) => {
    for await (const [name, child] of handle.entries ()) {
      const at = `${path}/${name}`;
      if (child.kind === "directory") {
        stored.dirs.push (at);
        await walk (child, at, stored);
      } else {
        stored.files.push ({ path: at, bytes: new Uint8Array (await (await child.getFile ()).arrayBuffer ()) });
      }
    }
  };
  return {
    backend: "opfs",
    async load (roots) {
      const stored = { dirs: [], files: [] };
      for (const root of roots) await walk (await folder (root, true), root, stored);
      return stored;
    },
    async apply ({ op, path, bytes }) {
      const parts = segments (path);
      const name = parts.pop ();
      const parent = await folder (parts.join ("/"), true);
      if (op === "mkdir") await parent.getDirectoryHandle (name, { create: true });
      else if (op === "remove") {
        try { await parent.removeEntry (name, { recursive: true }); }
        catch (e) { if (e.name !== "NotFoundError") throw e; }
      } else {
        const writer = await (await parent.getFileHandle (name, { create: true })).createWritable ();
        await writer.write (bytes);
        await writer.close ();
      }
    },
  };
}

function settle (request) {
  return new Promise ((resolve, reject) => {
    request.onsuccess = () => resolve (request.result);
    request.onerror = () => reject (request.error);
  });
}

async function indexeddb () {
  const opening = indexedDB.open ("gse-writable", 1);
  opening.onupgradeneeded = () => opening.result.createObjectStore ("entries");
  const db = await settle (opening);
  const entries = (mode) => db.transaction ("entries", mode).objectStore ("entries");
  return {
    backend: "indexeddb",
    async load (roots) {
      const stored = { dirs: [], files: [] };
      const store = entries ("readonly");
      const keys = await settle (store.getAllKeys ());
      const values = await settle (store.getAll ());
      keys.forEach ((path, i) => {
        if (!roots.some (root => path === root || path.startsWith (root + "/"))) return;
        if (values[i].dir) stored.dirs.push (path);
        else stored.files.push ({ path, bytes: values[i].bytes });
      });
      return stored;
    },
    async apply ({ op, path, bytes }) {
      const store = entries ("readwrite");
      if (op === "remove") {
        await settle (store.delete (path));
        await settle (store.delete (IDBKeyRange.bound (path + "/", path + "/￿")));
      } else {
        await settle (store.put (op === "mkdir" ? { dir: true } : { dir: false, bytes }, path));
      }
    },
  };
}

async function open_store () {
  for (const backend of [opfs, indexeddb]) {
    try {
      return await backend ();
    } catch (e) {
      say (`page: persistence backend ${backend.name} unavailable: ${e.message}`);
    }
  }
  return {
    backend: "none",
    async load () { return { dirs: [], files: [] }; },
    async apply () {},
  };
}

async function finish (code, reason) {
  if (finished) return;
  finished = true;
  await persisting;
  await flush ();
  await fetch ("/result", {
    method: "POST",
    body: JSON.stringify ({ code, reason, lines: lines.length }),
  });
}

function observe_surface (canvas, wake) {
  const surface = new BigInt64Array (new SharedArrayBuffer (SURFACE_SLOTS * 8));
  const backing = { width: canvas.width, height: canvas.height };
  globalThis.gse_canvas_backing = backing;
  const visibility = () => {
    Atomics.store (surface, SURFACE_VISIBLE, document.visibilityState === "visible" ? 1n : 0n);
    bump (wake);
  };
  new ResizeObserver (([entry]) => {
    const exact = entry.devicePixelContentBoxSize?.[0];
    const css = entry.contentBoxSize[0];
    const width = exact ? exact.inlineSize : Math.round (css.inlineSize * devicePixelRatio);
    const height = exact ? exact.blockSize : Math.round (css.blockSize * devicePixelRatio);
    backing.width = width;
    backing.height = height;
    say (`page: canvas backing ${width}x${height} css ${css.inlineSize}x${css.blockSize} `
         + `dpr ${devicePixelRatio} exact ${exact !== undefined}`);
    Atomics.store (surface, SURFACE_SIZE, (BigInt (width) << 32n) | BigInt (height));
    Atomics.add (surface, SURFACE_RESIZE_SEQ, 1n);
    bump (wake);
  }).observe (canvas);
  globalThis.gse_gate_mark = () => {
    Atomics.add (surface, SURFACE_MARK, 1n);
    bump (wake);
  };
  document.addEventListener ("visibilitychange", visibility);
  visibility ();
  return { surface, backing };
}

const WHEEL_PIXELS_PER_NOTCH = 100, WHEEL_LINES_PER_NOTCH = 3;

function observe_pointer_lock (canvas, surface, wake) {
  const wanted = () => Atomics.load (surface, SURFACE_CAPTURE) !== 0n;
  const locked = () => document.pointerLockElement === canvas;
  const request = () => {
    if (!wanted () || locked ()) return;
    try {
      Promise.resolve (canvas.requestPointerLock ())
        .catch (e => say (`page: pointer lock denied: ${e.name}: ${e.message}`));
    } catch (e) {
      say (`page: pointer lock denied: ${e.name}: ${e.message}`);
    }
  };
  const apply = () => {
    if (wanted ()) request ();
    else if (locked ()) document.exitPointerLock ();
  };
  const follow = async () => {
    while (true) {
      const seen = Atomics.load (surface, SURFACE_CAPTURE);
      apply ();
      const { async, value } = Atomics.waitAsync (surface, SURFACE_CAPTURE, seen);
      if (async) await value;
    }
  };
  document.addEventListener ("pointerlockchange", () => {
    say (`page: pointer lock ${locked () ? "acquired" : "released"}`);
    Atomics.store (surface, SURFACE_LOCKED, locked () ? 1n : 0n);
    bump (wake);
  });
  document.addEventListener ("pointerlockerror", () => {
    say ("page: pointer lock denied: pointerlockerror event");
  });
  follow ();
  return { request, locked };
}

function observe_input (canvas, size, typing, wake, lock) {
  const buffer = new SharedArrayBuffer (INPUT_HEADER_BYTES + INPUT_CAPACITY * INPUT_RECORD_BYTES);
  const header = new Int32Array (buffer, 0, INPUT_HEADER_BYTES / 4);
  const view = new DataView (buffer);
  const bytes = new Uint8Array (buffer);
  const encode = new TextEncoder ();

  const push = (kind, value, x = 0, y = 0, code = "") => {
    const head = Atomics.load (header, INPUT_HEAD);
    if (((head - Atomics.load (header, INPUT_TAIL)) | 0) >= INPUT_CAPACITY) {
      const dropped = Atomics.add (header, INPUT_DROPPED, 1) + 1;
      say (`page: input ring full, ${dropped} events dropped`);
      return;
    }
    const at = INPUT_HEADER_BYTES + (head & (INPUT_CAPACITY - 1)) * INPUT_RECORD_BYTES;
    bytes.fill (0, at, at + INPUT_RECORD_BYTES);
    view.setInt32 (at, kind, true);
    view.setInt32 (at + 4, value, true);
    view.setFloat64 (at + 8, x, true);
    view.setFloat64 (at + 16, y, true);
    const name = encode.encode (code).subarray (0, INPUT_CODE_BYTES);
    view.setUint8 (at + 24, name.length);
    bytes.set (name, at + INPUT_CODE_OFFSET);
    Atomics.store (header, INPUT_HEAD, (head + 1) | 0);
    bump (wake);
  };

  const backing = (e) => {
    const box = canvas.getBoundingClientRect ();
    return [
      (e.clientX - box.left) * size.width / box.width,
      (e.clientY - box.top) * size.height / box.height,
    ];
  };
  let cursor = [size.width / 2, size.height / 2];
  const pointer = (e) => {
    if (!lock.locked ()) cursor = backing (e);
    return cursor;
  };
  const glide = (e) => {
    const box = canvas.getBoundingClientRect ();
    cursor = [
      Math.min (Math.max (cursor[0] + e.movementX * size.width / box.width, 0), size.width),
      Math.min (Math.max (cursor[1] + e.movementY * size.height / box.height, 0), size.height),
    ];
    return cursor;
  };
  const button_of = [0, 2, 1, 3, 4];

  window.addEventListener ("keydown", (e) => {
    if (e.key.length !== 1 || e.ctrlKey || e.metaKey) e.preventDefault ();
    lock.request ();
    push (INPUT_KEY_DOWN, 0, 0, 0, e.code);
  });
  window.addEventListener ("keyup", (e) => {
    e.preventDefault ();
    push (INPUT_KEY_UP, 0, 0, 0, e.code);
  });
  typing.addEventListener ("beforeinput", (e) => {
    e.preventDefault ();
    if (e.data === null) return;
    for (const character of e.data) push (INPUT_TEXT, character.codePointAt (0));
  });
  canvas.addEventListener ("mousedown", (e) => {
    e.preventDefault ();
    typing.focus ();
    lock.request ();
    if (button_of[e.button] === undefined) return;
    push (INPUT_BUTTON_DOWN, button_of[e.button], ...pointer (e));
  });
  window.addEventListener ("mouseup", (e) => {
    if (button_of[e.button] === undefined) return;
    push (INPUT_BUTTON_UP, button_of[e.button], ...pointer (e));
  });
  window.addEventListener ("mousemove", (e) => {
    push (INPUT_MOVE, 0, ...(lock.locked () ? glide (e) : pointer (e)));
    if (lock.locked ()) {
      push (INPUT_RAW_MOVE, 0, e.movementX * devicePixelRatio, e.movementY * devicePixelRatio);
    }
  });
  canvas.addEventListener ("contextmenu", (e) => e.preventDefault ());
  canvas.addEventListener ("wheel", (e) => {
    e.preventDefault ();
    const per_notch = e.deltaMode === WheelEvent.DOM_DELTA_PIXEL ? WHEEL_PIXELS_PER_NOTCH
                    : e.deltaMode === WheelEvent.DOM_DELTA_LINE ? WHEEL_LINES_PER_NOTCH
                    : 1;
    push (INPUT_WHEEL, 0, 0 - e.deltaX / per_notch, 0 - e.deltaY / per_notch);
  }, { passive: false });
  typing.focus ();
  return buffer;
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
  const wake = new Int32Array (new SharedArrayBuffer (4));
  const canvas = document.getElementById ("surface");
  const { surface, backing } = observe_surface (canvas, wake);
  const lock = observe_pointer_lock (canvas, surface, wake);
  const input = observe_input (canvas, backing, document.getElementById ("typing"), wake, lock);
  const gpu = navigator.gpu ? make_ring (GPU_RING_WORDS) : null;
  const events = navigator.gpu ? make_ring (GPU_EVENT_WORDS) : null;
  if (gpu !== null) {
    const device = new Worker ("gpu.mjs", { type: "module" });
    device.onerror = (e) => say (`page: gpu worker error: ${e.message} (${e.filename}:${e.lineno})`);
    device.onmessage = (e) => {
      if (e.data.kind === "console") say (e.data.text);
      else if (e.data.kind === "presenting") out.style.display = "none";
    };
    const offscreen = canvas.transferControlToOffscreen ();
    device.postMessage ({ canvas: offscreen, mem, gpu, events }, [offscreen]);
  } else {
    say ("page: navigator.gpu is unavailable, so the run has no WebGPU device and passes --no-engine-gpu");
    env.GSE_RUN_ARGS = `--no-engine-gpu ${env.GSE_RUN_ARGS}`;
  }
  const ids = new Int32Array (new SharedArrayBuffer (4));
  ids[0] = 1;

  const threads = [];
  const start = (init) => {
    const worker = new Worker ("guest.mjs", { type: "module" });
    worker.onerror = (e) => say (`page: thread ${init.tid ?? 1} worker error: `
                                 + `${e.message} (${e.filename}:${e.lineno})`);
    worker.postMessage ({ module, mem, ctrl, text, names, env, ids, surface, input, wake, gpu, events, ...init });
    threads.push (worker);
  };

  const host = new Worker ("host.mjs", { type: "module" });
  host.onerror = (e) => say (`page: host worker error: ${e.message}`);

  host.onmessage = async (e) => {
    if (e.data.kind === "console") say (e.data.text);
    else if (e.data.kind === "persist") {
      const op = e.data;
      persisting = persisting.then (() => store.apply (op))
        .catch (error => say (`page: could not persist ${op.op} ${op.path}: ${error.message}`));
    }
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
        await persisting;
        say (`page: relaunching with ${e.data.relaunch}`);
        await flush ();
        await fetch ("/relaunch", { method: "POST", body: e.data.relaunch });
        location.replace (`?args=${encodeURIComponent (e.data.relaunch)}`);
        return;
      }
      await finish (e.data.code, "entry returned");
    }
  };
  const store = await open_store ();
  const stored = await store.load (run.writable);
  say (`page: persistence backend ${store.backend}, ${stored.files.length} stored files `
       + `under ${run.writable.join (", ")}`);
  host.postMessage ({ mem, ctrl, text, names, collect: run.collect,
                      writable: run.writable, stored });
}

main ().catch (async (e) => {
  say (`page: ${e.message}\n${e.stack}`);
  await finish (3, "page failed");
});
