import { spawn } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { deflateSync, inflateSync } from "node:zlib";
import { fileURLToPath } from "node:url";

const here = fileURLToPath (new URL (".", import.meta.url));

const options = { repeats: 3, headed: false, port: 8715, debug_port: 9335, timeout: 180,
                  only: null, native: null, native_textured: null, dump: false };
for (let i = 2; i < process.argv.length; ++i) {
  const arg = process.argv[i];
  if (arg === "--repeats") options.repeats = Number (process.argv[++i]);
  else if (arg === "--headed") options.headed = true;
  else if (arg === "--port") options.port = Number (process.argv[++i]);
  else if (arg === "--debug-port") options.debug_port = Number (process.argv[++i]);
  else if (arg === "--timeout") options.timeout = Number (process.argv[++i]);
  else if (arg === "--only") options.only = process.argv[++i].split (",");
  else if (arg === "--native") options.native = process.argv[++i];
  else if (arg === "--native-textured") options.native_textured = process.argv[++i];
  else if (arg === "--dump") options.dump = true;
  else {
    console.error (`host_gate.mjs: unknown argument ${arg}\n`
                   + "usage: node host_gate.mjs [--repeats N] [--headed] [--port N] "
                   + "[--debug-port N] [--timeout S] [--only window,input,pointer,persist,render,textured] "
                   + "[--native <png>] [--native-textured <png>] [--dump]");
    process.exit (2);
  }
}

const stamp = new Date ().toISOString ().replace (/[:.]/g, "-");
const out = join (here, "host_gate", stamp);
mkdirSync (out, { recursive: true });

const SKIPPED_KEYS = { F5: "reloads the page" };
const UNMAPPED_KEYS = ["world_1", "f25", "unknown"];
const RESIZE_FRAME_BUDGET = 4;
const RESIZED = { width: 1000, height: 700 };
const STATE_DUMP_NAME = "lighting_parity.gstates";

const sleep = (ms) => new Promise (r => setTimeout (r, ms));

function find_browser () {
  if (process.env.GSE_CHROME) return process.env.GSE_CHROME;
  const found = [
    "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
    "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
    "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
    "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
    "/usr/bin/google-chrome",
    "/usr/bin/chromium",
  ].find (c => existsSync (c));
  if (found === undefined) {
    console.error ("host_gate: no Chrome or Edge found; set GSE_CHROME to a browser executable");
    process.exit (2);
  }
  return found;
}

class log_stream {
  constructor () {
    this.lines = [];
    this.partial = "";
    this.waiters = [];
  }
  feed (chunk) {
    const parts = (this.partial + chunk).replace (/\x1b\[[0-9;]*m/g, "").split (/\r?\n/);
    this.partial = parts.pop ();
    for (const line of parts) this.lines.push (line);
    for (const waiter of [...this.waiters]) waiter ();
  }
  find (pattern, from = 0) {
    for (let i = from; i < this.lines.length; ++i) {
      const match = this.lines[i].match (pattern);
      if (match) return { index: i, match };
    }
    return null;
  }
  until (pattern, ms, from = 0) {
    return new Promise ((resolve, reject) => {
      const check = () => {
        const hit = this.find (pattern, from);
        if (hit === null) return false;
        this.waiters = this.waiters.filter (w => w !== check);
        clearTimeout (timer);
        resolve (hit);
        return true;
      };
      const timer = setTimeout (() => {
        this.waiters = this.waiters.filter (w => w !== check);
        reject (new Error (`no line matching ${pattern} within ${ms} ms`));
      }, ms);
      if (!check ()) this.waiters.push (check);
    });
  }
}

async function connect (port) {
  for (let attempt = 0; attempt < 100; ++attempt) {
    try {
      const targets = await (await fetch (`http://127.0.0.1:${port}/json`)).json ();
      const page = targets.find (t => t.type === "page");
      if (page !== undefined) {
        const ws = new WebSocket (page.webSocketDebuggerUrl);
        await new Promise ((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
        let id = 0;
        const waiting = new Map ();
        ws.onmessage = (m) => {
          const reply = JSON.parse (m.data);
          if (!waiting.has (reply.id)) return;
          waiting.get (reply.id) (reply);
          waiting.delete (reply.id);
        };
        const send = (method, params = {}) => new Promise ((resolve, reject) => {
          waiting.set (++id, (reply) => reply.error
            ? reject (new Error (`${method}: ${reply.error.message}`)) : resolve (reply.result));
          ws.send (JSON.stringify ({ id, method, params }));
        });
        const evaluate = async (expression) => {
          const result = await send ("Runtime.evaluate", { expression, returnByValue: true });
          return result.result.value;
        };
        return { send, evaluate, target: page.id, close: () => ws.close () };
      }
    } catch {}
    await sleep (100);
  }
  throw new Error (`no page target on debug port ${port}`);
}

async function run_case (name, scenario, drive, gpu, collect, dump) {
  const log = new log_stream ();
  const captures = join (out, `${name}-captures`);
  const guest_dump = `${run_config.environment.GSE_STATE_DIR}/${STATE_DUMP_NAME}`;
  const args = [join (here, "serve.mjs"), "--port", String (options.port),
                "--timeout", String (options.timeout),
                ...(collect ? ["--collect", `${run_config.environment.GSE_STATE_DIR}/captures/screenshots/`] : []),
                ...(dump ? ["--collect", guest_dump] : []),
                "--collect", `${run_config.environment.GSE_STATE_DIR}/profile/bench/`,
                "--collect-out", captures, "--",
                "--engine-bench-scenario", scenario, ...(gpu ? [] : ["--no-engine-gpu"]),
                ...(dump ? ["--engine-bench-state-dump-out", guest_dump] : []),
                "--engine-setting", "Dev.global_level=debug",
                "--engine-setting", "Dev.general_level=debug"];
  const serve = spawn (process.execPath, args, { stdio: ["ignore", "pipe", "pipe"] });
  serve.stdout.on ("data", d => log.feed (d.toString ("utf8")));
  serve.stderr.on ("data", d => log.feed (d.toString ("utf8")));
  const exited = new Promise (r => serve.once ("exit", code => r (code)));

  const result = { name, scenario, checks: [], notes: [], exit: null, log: null };
  const check = (label, pass, detail) => result.checks.push ({ label, pass, detail });
  let browser = null, profile = null, cdp = null;
  try {
    await log.until (/^serve: http/, 10000);
    profile = mkdtempSync (join (tmpdir (), "gse-host-gate-"));
    const chrome = [`--user-data-dir=${profile}`, "--no-first-run", "--no-default-browser-check",
                    "--disable-extensions", `--remote-debugging-port=${options.debug_port}`,
                    `http://127.0.0.1:${options.port}/`];
    if (!options.headed) chrome.unshift ("--headless=new");
    else chrome.unshift ("--window-size=1280,720", "--window-position=0,0", "--force-device-scale-factor=1",
                         "--disable-background-timer-throttling", "--disable-renderer-backgrounding",
                         "--disable-backgrounding-occluded-windows");
    browser = spawn (find_browser (), chrome, { stdio: "ignore" });
    cdp = await connect (options.debug_port);
    await drive ({ cdp, log, check, captures, note: (text) => result.notes.push (text) });
  } catch (e) {
    check ("driver", false, e.message);
  }
  result.exit = await Promise.race ([exited, sleep (options.timeout * 1000).then (() => null)]);
  if (result.exit === null) serve.kill ();
  check ("run exited 0", result.exit === 0,
         `serve exit ${result.exit}; ${log.find (/^serve: entry returned .*/)?.match[0] ?? "no verdict"}`);
  cdp?.close ();
  if (browser !== null && browser.exitCode === null) {
    browser.kill ();
    await new Promise (r => browser.once ("exit", r));
  }
  if (profile !== null) {
    try { rmSync (profile, { recursive: true, force: true, maxRetries: 20, retryDelay: 250 }); }
    catch {}
  }
  log.feed ("\n");
  result.log = join (out, `${name}.log`);
  writeFileSync (result.log, log.lines.join ("\n") + "\n");
  result.pass = result.checks.every (c => c.pass);
  console.log (`host_gate: ${name} ${result.pass ? "PASS" : "FAIL"}`);
  for (const c of result.checks.filter (c => !c.pass)) console.log (`  ${c.label}: ${c.detail}`);
  return result;
}

const BACKING = /page: canvas backing (\d+)x(\d+) css ([\d.]+)x([\d.]+) dpr ([\d.]+) exact (\w+)/;

const canvas_state = `(() => {
  const canvas = document.getElementById ("surface");
  const box = canvas.getBoundingClientRect ();
  const backing = globalThis.gse_canvas_backing;
  return { width: backing.width, height: backing.height, css_width: box.width,
           css_height: box.height, left: box.left, top: box.top, dpr: devicePixelRatio,
           visibility: document.visibilityState };
}) ()`;

async function window_case ({ cdp, log, check, note }) {
  const first = await log.until (/scenario: web host frame \d+ viewport (\d+)x(\d+) minimized (\w+)/, 60000);
  const backing = await log.until (BACKING, 1000);
  const state = await cdp.evaluate (canvas_state);
  const [vw, vh] = [Number (first.match[1]), Number (first.match[2])];
  const css_dpr = `${Math.round (state.css_width * state.dpr)}x${Math.round (state.css_height * state.dpr)}`;
  note (`boot backing ${state.width}x${state.height} (devicePixelContentBoxSize `
        + `${backing.match[6] === "true" ? "available" : "unavailable"}), css ${state.css_width}x`
        + `${state.css_height} x dpr ${state.dpr} = ${css_dpr}`);
  check ("boot viewport = canvas devicePixelContentBoxSize", vw === state.width && vh === state.height,
         `viewport ${vw}x${vh}, canvas ${state.width}x${state.height}`);
  check ("boot minimized follows visibility", (first.match[3] === "true") === (state.visibility !== "visible"),
         `minimized ${first.match[3]}, visibilityState ${state.visibility}`);

  const warm = await log.until (/bench: warmup complete, measuring/, 60000).catch (() => null);
  if (warm === null) {
    check ("warmup settled before resize", false, "no `bench: warmup complete, measuring` line within 60 s");
    return;
  }
  const settled = await log.until (/scenario: web host frame (\d+)/, 10000, warm.index + 1).catch (() => null);
  check ("warmup settled before resize", settled !== null,
         settled === null ? "no probe frame after `bench: warmup complete`"
                          : `settle signal: \`bench: warmup complete, measuring\` then probe frame ${settled.match[1]}`);
  if (settled === null) return;

  const mark = log.lines.length;
  await cdp.send ("Emulation.setDeviceMetricsOverride",
                  { ...RESIZED, deviceScaleFactor: state.dpr, mobile: false });
  await cdp.evaluate ("gse_gate_mark ()");
  let resized = null;
  for (let i = 0; i < 100 && resized === null; ++i) {
    await sleep (50);
    const now = await cdp.evaluate (canvas_state);
    if (now.width !== state.width || now.height !== state.height) resized = now;
  }
  if (resized === null) {
    check ("resize reaches canvas", false, "canvas size unchanged 5 s after setDeviceMetricsOverride");
  } else {
    await sleep (300);
    resized = await cdp.evaluate (canvas_state);
    const size = new RegExp (`window: tick (\\d+) resize seq (\\d+) \\d+x\\d+ -> ${resized.width}x${resized.height}$`);
    const seen = await log.until (size, 10000, mark).catch (() => null);
    const marked = await log.until (/window: tick (\d+) mark \d+$/, 10000, mark).catch (() => null);
    const viewport = await log.until (new RegExp (`scenario: web host frame \\d+ viewport ${resized.width}x${resized.height} minimized`),
                                      10000, mark).catch (() => null);
    if (seen === null || marked === null || viewport === null) {
      check (`resize viewport within ${RESIZE_FRAME_BUDGET} frames`, false,
             `canvas ${resized.width}x${resized.height}; mark line ${marked !== null}, `
             + `resize line ${seen !== null}, probe viewport line ${viewport !== null}`);
    } else {
      const frames = Number (seen.match[1]) - Number (marked.match[1]);
      const seq = (() => {
        for (let i = mark; i < seen.index; ++i) {
          const m = log.lines[i].match (/window: tick (\d+) resize seq (\d+)$/);
          if (m) return Number (m[1]);
        }
        return Number (seen.match[1]);
      }) ();
      check (`resize viewport within ${RESIZE_FRAME_BUDGET} frames`, frames <= RESIZE_FRAME_BUDGET,
             `canvas ${resized.width}x${resized.height}, mark tick ${marked.match[1]}, `
             + `first seq tick ${seq}, resize tick ${seen.match[1]}, ${frames} frame(s) after the override`);
      note (`resize latency ${frames} tick(s) (mark ${marked.match[1]}, seq ${seq}, size ${seen.match[1]})`);
    }
  }

  const hidden_mark = log.lines.length;
  await cdp.send ("Emulation.setFocusEmulationEnabled", { enabled: false });
  const other = await cdp.send ("Target.createTarget", { url: "about:blank" });
  await sleep (500);
  const hidden = await cdp.evaluate ("document.visibilityState");
  if (hidden !== "hidden") {
    check ("hidden -> minimized", false,
           `could not hide the page: visibilityState stays ${hidden} with another tab in front`);
  } else {
    const seen = await log.until (/scenario: web host frame (\d+) viewport \d+x\d+ minimized true/, 10000, hidden_mark).catch (() => null);
    check ("hidden -> minimized", seen !== null, seen ? `engine frame ${seen.match[1]}` : "no minimized true line within 10 s");
  }
  const shown_mark = log.lines.length;
  await cdp.send ("Target.closeTarget", { targetId: other.targetId });
  await cdp.send ("Target.activateTarget", { targetId: cdp.target }).catch (() => {});
  await cdp.send ("Page.bringToFront").catch (() => {});
  await sleep (500);
  const shown = await cdp.evaluate ("document.visibilityState");
  if (shown !== "visible") {
    check ("visible -> not minimized", false, `visibilityState stays ${shown} after bringToFront`);
  } else if (hidden === "hidden") {
    const seen = await log.until (/scenario: web host frame (\d+) viewport \d+x\d+ minimized false/, 10000, shown_mark).catch (() => null);
    check ("visible -> not minimized", seen !== null, seen ? `engine frame ${seen.match[1]}` : "no minimized false line within 10 s");
  }
}

const INPUT = /\binput ((?:key_pressed|key_released|mouse_\w+|text_entered)\b.*)$/;
const format = (n) => String (n);

async function input_case ({ cdp, log, check, note }) {
  const first = await log.until (/scenario: web host frame \d+ viewport (\d+)x(\d+)/, 60000);
  const keymap = log.lines.map (l => l.match (/\bkeymap (\S+) (\S+)$/)).filter (Boolean).map (m => [m[1], m[2]]);
  check ("keymap logged", keymap.length > 0, `${keymap.length} DOM codes`);
  await cdp.evaluate (`document.getElementById ("surface").requestPointerLock = () =>
    Promise.reject (new DOMException ("held off by the host gate's input case", "NotAllowedError"))`);
  note ("pointer lock refused for this case, so injected motion keeps absolute coordinates; the pointer case covers lock");
  const state = await cdp.evaluate (canvas_state);
  const [w, h] = [Number (first.match[1]), Number (first.match[2])];
  const expected = [`mouse_moved ${format (w / 2)} ${format (h - h / 2)}`];

  const skipped = [];
  const keys = [];
  for (const [code, name] of keymap) {
    if (code in SKIPPED_KEYS) { skipped.push (`${name} (${code}: ${SKIPPED_KEYS[code]})`); continue; }
    keys.push (cdp.send ("Input.dispatchKeyEvent", { type: "rawKeyDown", code, key: code }),
               cdp.send ("Input.dispatchKeyEvent", { type: "keyUp", code, key: code }));
    expected.push (`key_pressed ${name}`, `key_released ${name}`);
  }
  await Promise.all (keys);
  for (const name of UNMAPPED_KEYS) skipped.push (`${name} (no DOM code)`);
  note (`keys injected ${keymap.length - Object.keys (SKIPPED_KEYS).length}; skipped ${skipped.join (", ")}`);

  const backing = (x, y) => {
    const bx = (x - state.left) * state.width / state.css_width;
    const by = (y - state.top) * state.height / state.css_height;
    return `${format (bx)} ${format (h - by)}`;
  };
  const at = (fx, fy) => [Math.round (state.left + state.css_width * fx),
                          Math.round (state.top + state.css_height * fy)];
  const mouse = (params) => cdp.send ("Input.dispatchMouseEvent", params);
  for (const [fx, fy] of [[0.25, 0.25], [0.75, 0.4], [0.5, 0.8]]) {
    const [x, y] = at (fx, fy);
    await mouse ({ type: "mouseMoved", x, y });
    expected.push (`mouse_moved ${backing (x, y)}`);
  }
  const buttons = [["left", 1, "button_1"], ["right", 2, "button_2"], ["middle", 4, "button_3"],
                   ["back", 8, "button_4"], ["forward", 16, "button_5"]];
  const [bx, by] = at (0.3, 0.6);
  await mouse ({ type: "mouseMoved", x: bx, y: by });
  expected.push (`mouse_moved ${backing (bx, by)}`);
  for (const [button, mask, name] of buttons) {
    await mouse ({ type: "mousePressed", x: bx, y: by, button, buttons: mask, clickCount: 1 });
    await mouse ({ type: "mouseReleased", x: bx, y: by, button, buttons: 0, clickCount: 1 });
    expected.push (`mouse_button_pressed ${name} ${backing (bx, by)}`,
                   `mouse_button_released ${name} ${backing (bx, by)}`);
  }
  await mouse ({ type: "mouseWheel", x: bx, y: by, deltaX: 100, deltaY: 0 });
  await mouse ({ type: "mouseWheel", x: bx, y: by, deltaX: 0, deltaY: -200 });
  expected.push ("mouse_scrolled -1 0", "mouse_scrolled 0 2");
  await cdp.send ("Input.insertText", { text: "a\u{1F600}" });
  expected.push ("text_entered 97", "text_entered 128512");

  await log.until (/serve: entry returned/, options.timeout * 1000);
  const actual = log.lines.map (l => l.match (INPUT)).filter (Boolean).map (m => m[1]);
  let diverge = -1;
  for (let i = 0; i < Math.max (expected.length, actual.length); ++i) {
    if (expected[i] !== actual[i]) { diverge = i; break; }
  }
  check ("input sequence exact", diverge === -1,
         diverge === -1 ? `${actual.length} events`
           : `${expected.length} expected, ${actual.length} logged; first difference at #${diverge}: `
             + `expected "${expected[diverge] ?? "(end)"}", got "${actual[diverge] ?? "(end)"}"`);
}

async function pointer_case ({ cdp, log, check, note }) {
  await log.until (/scenario: web pointer requested capture/, 60000);
  const state = await cdp.evaluate (canvas_state);
  const denied = await log.until (/page: pointer lock denied.*/, 3000).catch (() => null);
  if (denied) note (`boot request: ${denied.match[0]}`);
  const mouse = (params) => cdp.send ("Input.dispatchMouseEvent", params);
  const [x, y] = [Math.round (state.left + state.css_width / 2), Math.round (state.top + state.css_height / 2)];
  await mouse ({ type: "mouseMoved", x, y });
  await mouse ({ type: "mousePressed", x, y, button: "left", buttons: 1, clickCount: 1 });
  await mouse ({ type: "mouseReleased", x, y, button: "left", buttons: 0, clickCount: 1 });
  const acquired = await log.until (/page: pointer lock acquired/, 5000).catch (() => null);
  const element = await cdp.evaluate ("String (document.pointerLockElement && document.pointerLockElement.id)");
  if (acquired === null) {
    const why = log.find (/page: pointer lock denied.*/, 0);
    check ("pointer lock acquired", false,
           `pointerLockElement ${element}; ${why ? why.match[0] : "no denial logged"}`);
    return;
  }
  check ("pointer lock acquired", true, `pointerLockElement ${element}`);
  await sleep (300);
  const mark = log.lines.length;
  const moves = 5;
  for (let i = 1; i <= moves; ++i) {
    await mouse ({ type: "mouseMoved", x: x + i * 10, y: y + i * 5 });
    await sleep (50);
  }
  const want = `mouse_raw_moved ${format (10 * state.dpr)} ${format (5 * state.dpr)}`;
  await log.until (/serve: entry returned/, options.timeout * 1000);
  const raw = log.lines.slice (mark).map (l => l.match (INPUT)).filter (Boolean).map (m => m[1])
    .filter (e => e.startsWith ("mouse_raw_moved"));
  check ("raw deltas exact", raw.length === moves && raw.every (e => e === want),
         `expected ${moves} x "${want}", got [${raw.join (", ")}]`);
  const moved = log.lines.slice (mark).map (l => l.match (/\binput mouse_moved (\S+) (\S+)$/)).filter (Boolean)
    .map (m => [Number (m[1]), Number (m[2])]);
  const step = [10 * state.width / state.css_width, 5 * state.height / state.css_height];
  const glides = moved.slice (1).every ((at, i) =>
    Math.abs (Math.abs (at[0] - moved[i][0]) - step[0]) < 1e-6
    && Math.abs (Math.abs (at[1] - moved[i][1]) - step[1]) < 1e-6);
  check ("locked cursor glides", moved.length === moves && glides,
         `expected ${moves} positions ${step.join (", ")} apart, got [${moved.map (p => p.join (" ")).join (", ")}]`);
}

async function persist_case ({ log, check }) {
  const wrote = await log.until (/scenario: web persist wrote .*/, 60000);
  check ("run A wrote", /\(ok\).*\(\d+ bytes\)/.test (wrote.match[0]), wrote.match[0]);
  const relaunched = await log.until (/page: relaunched with .*/, 30000).catch (() => null);
  check ("page reloaded", relaunched !== null, relaunched?.match[0] ?? "no reload");
  const read = await log.until (/scenario: web persist settings .*/, 60000);
  check ("run B read back", /settings match .* save match/.test (read.match[0]), read.match[0]);
  const backend = log.find (/page: persistence backend (\w+), (\d+) stored files/, relaunched?.index ?? 0);
  check ("reload loaded stored files", backend !== null && Number (backend.match[2]) > 0,
         backend ? backend.match[0] : "no backend line after reload");
}

const LIGHTING_HASH = "0x350203be5257f128";
const TEXTURED_HASH = "0x265fb1ac45c30181";
const GPU_ERROR = /webgpu (?:\w+ )?error|failed validation|webgpu device lost|^gpu: .* threw|pump stopped|trapped/;
const LIT_FRACTION = 0.5;
const PARITY_EXTENT = { width: 1280, height: 720 };
const PARITY_MEAN = 2.0, PARITY_DELTA = 8, PARITY_OVER_PERCENT = 1.0;
const run_config = JSON.parse (readFileSync (join (here, "wasm_run.json"), "utf8"));
const parity_out = join (run_config.root, ".generated", "parity_reports", stamp);
const parity_targets = {
  render: { title: "B1 image parity", scenario: "lighting_parity", hash: LIGHTING_HASH, digest: true,
            native_file: options.native, runs: [], hashes: [] },
  textured: { title: "B3 textured image parity", scenario: "textured_parity", hash: TEXTURED_HASH, digest: false,
              native_file: options.native_textured, runs: [], hashes: [] },
};
for (const target of Object.values (parity_targets)) {
  target.native = target.native_file === null ? null : png_pixels (readFileSync (target.native_file));
}
const any_native = Object.values (parity_targets).some (t => t.native !== null);
const DIGEST_BUDGET_MS = 250;
const digest_runs = [];
let browser_version = "browser version unknown";
if (any_native) mkdirSync (parity_out, { recursive: true });

function png_pixels (png) {
  let at = 8, width = 0, height = 0, depth = 0, kind = 0;
  const data = [];
  while (at < png.length) {
    const length = png.readUInt32BE (at), type = png.toString ("ascii", at + 4, at + 8);
    const body = png.subarray (at + 8, at + 8 + length);
    if (type === "IHDR") [width, height, depth, kind] = [body.readUInt32BE (0), body.readUInt32BE (4), body[8], body[9]];
    if (type === "IDAT") data.push (body);
    at += length + 12;
  }
  if (depth !== 8 || (kind !== 2 && kind !== 6)) throw new Error (`png: unsupported depth ${depth} colour type ${kind}`);
  const channels = kind === 6 ? 4 : 3, stride = width * channels;
  const raw = inflateSync (Buffer.concat (data));
  const out = Buffer.alloc (height * stride);
  for (let y = 0; y < height; ++y) {
    const filter = raw[y * (stride + 1)];
    for (let x = 0; x < stride; ++x) {
      const value = raw[y * (stride + 1) + 1 + x];
      const left = x >= channels ? out[y * stride + x - channels] : 0;
      const up = y > 0 ? out[(y - 1) * stride + x] : 0;
      const corner = x >= channels && y > 0 ? out[(y - 1) * stride + x - channels] : 0;
      const p = left + up - corner, pa = Math.abs (p - left), pb = Math.abs (p - up), pc = Math.abs (p - corner);
      const predicted = filter === 1 ? left : filter === 2 ? up : filter === 3 ? (left + up) >> 1
        : filter === 4 ? (pa <= pb && pa <= pc ? left : pb <= pc ? up : corner) : 0;
      out[y * stride + x] = (value + predicted) & 0xff;
    }
  }
  return { width, height, channels, pixels: out };
}

function png_encode (width, height, rgb) {
  const crc_table = Array.from ({ length: 256 }, (_, n) => {
    let c = n;
    for (let k = 0; k < 8; ++k) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
  });
  const crc = (bytes) => {
    let c = 0xffffffff;
    for (const b of bytes) c = crc_table[(c ^ b) & 0xff] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
  };
  const chunk = (type, body) => {
    const head = Buffer.alloc (8), tail = Buffer.alloc (4);
    head.writeUInt32BE (body.length, 0);
    head.write (type, 4, "ascii");
    tail.writeUInt32BE (crc (Buffer.concat ([head.subarray (4), body])), 0);
    return Buffer.concat ([head, body, tail]);
  };
  const ihdr = Buffer.alloc (13);
  ihdr.writeUInt32BE (width, 0);
  ihdr.writeUInt32BE (height, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  const raw = Buffer.alloc (height * (width * 3 + 1));
  for (let y = 0; y < height; ++y) rgb.copy (raw, y * (width * 3 + 1) + 1, y * width * 3, (y + 1) * width * 3);
  return Buffer.concat ([Buffer.from ([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
                         chunk ("IHDR", ihdr), chunk ("IDAT", deflateSync (raw)), chunk ("IEND", Buffer.alloc (0))]);
}

function image_diff (browser, native) {
  const count = browser.width * browser.height;
  const sums = [0, 0, 0];
  let over = 0;
  const heat = Buffer.alloc (count * 3);
  for (let p = 0; p < count; ++p) {
    let worst = 0;
    for (let c = 0; c < 3; ++c) {
      const delta = Math.abs (browser.pixels[p * browser.channels + c] - native.pixels[p * native.channels + c]);
      sums[c] += delta;
      worst = Math.max (worst, delta);
    }
    if (worst > PARITY_DELTA) ++over;
    heat[p * 3] = Math.min (255, worst * 8);
    heat[p * 3 + 1] = worst > PARITY_DELTA ? 0 : Math.min (255, worst * 8);
    heat[p * 3 + 2] = 0;
  }
  return { mean: sums.map (s => s / count), over_percent: over * 100 / count,
           heat: png_encode (browser.width, browser.height, heat) };
}

const parity_case = (target) => async ({ cdp, log, check, captures, note }) => {
  const native_image = target.native;
  const parity_runs = target.runs;
  browser_version = (await (await fetch (`http://127.0.0.1:${options.debug_port}/json/version`)).json ()).Browser;
  await cdp.send ("Emulation.setDeviceMetricsOverride", { ...PARITY_EXTENT, deviceScaleFactor: 1, mobile: false });
  const adapter = await log.until (/gpu: adapter .*/, 60000).catch (() => null);
  check ("browser WebGPU adapter", adapter !== null, adapter?.match[0] ?? "no adapter line within 60 s");
  if (adapter === null) return;
  await log.until (new RegExp (`scenario: ${target.scenario} capture frame \\d+`), 180000);
  await sleep (500);
  const box = await cdp.evaluate (canvas_state);
  check (`canvas backing ${PARITY_EXTENT.width}x${PARITY_EXTENT.height}`,
         box.width === PARITY_EXTENT.width && box.height === PARITY_EXTENT.height,
         `backing ${box.width}x${box.height}, css ${box.css_width}x${box.css_height} dpr ${box.dpr}`);
  const shot = await cdp.send ("Page.captureScreenshot", {
    format: "png", clip: { x: box.left, y: box.top, width: box.css_width, height: box.css_height, scale: 1 } });
  const png = Buffer.from (shot.data, "base64");
  const file = join (out, `render-${Date.now ()}.png`);
  writeFileSync (file, png);
  const image = png_pixels (png);
  const late = native_image !== null && image.width === native_image.width && image.height === native_image.height
    ? image_diff (image, native_image) : null;
  if (late !== null) {
    note (`CDP screenshot ~500 ms after the capture frame (report only): mean r ${late.mean[0].toFixed (3)} `
          + `g ${late.mean[1].toFixed (3)} b ${late.mean[2].toFixed (3)}, ${late.over_percent.toFixed (3)}% over ${PARITY_DELTA}`);
  }
  let lit = 0;
  for (let i = 0; i < image.pixels.length; i += image.channels) {
    if (image.pixels[i] + image.pixels[i + 1] + image.pixels[i + 2] > 24) ++lit;
  }
  const fraction = lit / (image.width * image.height);
  check (`canvas at least ${LIT_FRACTION * 100}% lit`, fraction >= LIT_FRACTION,
         `${(fraction * 100).toFixed (1)}% of ${image.width}x${image.height} pixels above black, screenshot \`${file}\``);
  await log.until (/serve: entry returned/, options.timeout * 1000);
  if (native_image !== null) {
    const shots = existsSync (captures) ? readdirSync (captures).filter (f => f.endsWith (".png")) : [];
    check ("engine readback of the capture frame collected", shots.length === 1,
           `${shots.length} screenshot(s) under \`${captures}\``);
    if (shots.length === 1) {
      const readback_file = join (captures, shots[0]);
      const readback = png_pixels (readFileSync (readback_file));
      const same = readback.width === native_image.width && readback.height === native_image.height;
      check ("browser and native frame sizes match", same,
             `browser ${readback.width}x${readback.height}, native ${native_image.width}x${native_image.height}`);
      if (same) {
        const diff = image_diff (readback, native_image);
        const heat = join (parity_out, `heat-${target.scenario}-${parity_runs.length + 1}.png`);
        writeFileSync (heat, diff.heat);
        const pass = diff.mean.every (m => m <= PARITY_MEAN) && diff.over_percent <= PARITY_OVER_PERCENT;
        parity_runs.push ({ ...diff, pass, browser_png: readback_file, heat, late });
        check (`${target.title.split (" ")[0]} parity (mean <= ${PARITY_MEAN}, <= ${PARITY_OVER_PERCENT}% over ${PARITY_DELTA})`, pass,
               `mean r ${diff.mean[0].toFixed (3)} g ${diff.mean[1].toFixed (3)} b ${diff.mean[2].toFixed (3)}, `
               + `${diff.over_percent.toFixed (3)}% over ${PARITY_DELTA}, readback \`${readback_file}\`, heat \`${heat}\``);
      }
    }
  }
  const errors = log.lines.filter (l => GPU_ERROR.test (l));
  check ("no WebGPU errors", errors.length === 0,
         errors.length === 0 ? "0 lines" : `${errors.length} lines, first: ${errors[0].slice (0, 300)}`);
  const hash = log.find (/world-state hash (0x[0-9a-f]+)/);
  check (`hash ${target.hash} (native)`, hash?.match[1] === target.hash,
         hash ? hash.match[1] : "no hash line");
  const earlier = target.hashes.find (h => h !== hash?.match[1]);
  check ("hash stable across runs", earlier === undefined,
         earlier === undefined ? `${target.hashes.length + 1} run(s) agree` : `earlier run hashed ${earlier}`);
  target.hashes.push (hash?.match[1] ?? "none");
  note (adapter.match[0].slice (0, 200));
  if (!target.digest) return;
  const digest = log.find (/shader source digest: (\d+) files in ([\d.]+) ?ms/);
  const digest_ms = digest ? Number (digest.match[2]) : null;
  if (digest) digest_runs.push ({ files: Number (digest.match[1]), ms: digest_ms });
  check (`B4 shader source digest <= ${DIGEST_BUDGET_MS} ms`, digest_ms !== null && digest_ms <= DIGEST_BUDGET_MS,
         digest ? `${digest.match[1]} files in ${digest_ms} ms` : "no digest line");
};

const cases = [
  { key: "window", scenario: "web_host_probe", drive: window_case, required: true },
  { key: "input", scenario: "web_host_probe", drive: input_case, required: true },
  { key: "persist", scenario: "web_persist_write", drive: persist_case, required: true },
  { key: "pointer", scenario: "web_pointer_probe", drive: pointer_case, required: false, once: true },
  { key: "render", scenario: "lighting_parity", drive: parity_case (parity_targets.render), required: true, gpu: true },
  { key: "textured", scenario: "textured_parity", drive: parity_case (parity_targets.textured), required: true, gpu: true },
].filter (c => options.only === null || options.only.includes (c.key));

const MASK = (1n << 64n) - 1n;

function hash_combine (seed, value) {
  seed = ((seed ^ value) + 0x9E3779B97F4A7C15n) & MASK;
  seed = ((seed ^ seed >> 30n) * 0xBF58476D1CE4E5B9n) & MASK;
  seed = ((seed ^ seed >> 27n) * 0x94D049BB133111EBn) & MASK;
  return seed ^ seed >> 31n;
}

function hash_units (bytes) {
  let h = 14695981039346656037n;
  for (const b of bytes) h = ((h ^ BigInt (b)) * 1099511628211n) & MASK;
  return h;
}

function shader_source_digest (root) {
  const files = [];
  const walk = (relative) => {
    for (const entry of readdirSync (join (root, ...relative), { withFileTypes: true })) {
      const at = [...relative, entry.name];
      if (entry.isDirectory ()) { if (!(at.length === 1 && entry.name === "Wgsl")) walk (at); }
      else if (entry.isFile ()) files.push (at);
    }
  };
  walk ([]);
  files.sort ((a, b) => {
    for (let i = 0; i < Math.min (a.length, b.length); ++i) {
      if (a[i] !== b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return a.length - b.length;
  });
  let h = 0n;
  for (const file of files) {
    const named = hash_combine (0n, hash_units (Buffer.from (file.join ("/"), "utf8")));
    h = hash_combine (h, hash_combine (named, hash_units (readFileSync (join (root, ...file)))));
  }
  return h.toString (16).padStart (16, "0");
}

if (cases.some (c => c.gpu)) {
  const shaders = join (run_config.root, "Engine", "Resources", "Shaders");
  const stamp = join (shaders, "Wgsl", "digest");
  const baked = existsSync (stamp) ? readFileSync (stamp, "utf8").trim () : "none";
  const current = shader_source_digest (shaders);
  if (baked !== current) {
    console.error (`host_gate: the wgsl pack is stale (baked ${baked}, sources ${current}); `
                   + "re-bake it with a native run under Graphics.backend=webgpu");
    process.exit (1);
  }
}

const results = [];
for (let rep = 1; rep <= options.repeats; ++rep) {
  for (const c of cases) {
    if (c.once && rep > 1) continue;
    const r = await run_case (`${c.key}-${rep}`, c.scenario, c.drive, c.gpu === true,
                             (parity_targets[c.key]?.native ?? null) !== null, c.key === "render" && options.dump);
    results.push ({ ...r, key: c.key, rep, required: c.required });
  }
}

const required_pass = results.filter (r => r.required).every (r => r.pass);
const parity = Object.values (parity_targets).filter (t => t.native !== null).flatMap (t => [
  `## ${t.title} (${t.scenario})`, "",
  `${browser_version}, ${options.headed ? "headed" : "headless"}. Native reference \`${t.native_file}\` `
  + `(${t.native.width}x${t.native.height}). Pass: per-channel mean absolute difference <= ${PARITY_MEAN} `
  + `and <= ${PARITY_OVER_PERCENT}% of pixels with a max channel delta over ${PARITY_DELTA}, on every run. `
  + `World-state hashes: ${t.hashes.join (", ") || "none"} (expected ${t.hash} on every run).`, "",
  "The browser frame is the engine's own readback of the capture frame (CaptureRenderer, collected from the wasm "
  + "filesystem). The last column is the old CDP screenshot taken ~500 ms later, for comparison only.", "",
  "| run | mean r | mean g | mean b | % over " + PARITY_DELTA + " | verdict | heat | CDP % over " + PARITY_DELTA + " |",
  "|---|---|---|---|---|---|---|---|",
  ...t.runs.map ((p, i) => `| ${i + 1} | ${p.mean.map (m => m.toFixed (3)).join (" | ")} | `
    + `${p.over_percent.toFixed (3)} | ${p.pass ? "pass" : "FAIL"} | \`${p.heat}\` | `
    + `${p.late === null ? "-" : p.late.over_percent.toFixed (3)} |`),
  "",
]);
const boot_cost = digest_runs.length === 0 ? [] : [
  "## B4 shader source digest", "",
  `Boot time the wasm engine spends on gpu::shader_source_digest. Pass: <= ${DIGEST_BUDGET_MS} ms on every run.`, "",
  "| run | files | ms | verdict |", "|---|---|---|---|",
  ...digest_runs.map ((d, i) => `| ${i + 1} | ${d.files} | ${d.ms} | ${d.ms <= DIGEST_BUDGET_MS ? "pass" : "FAIL"} |`),
  "",
];
const report = [
  "# host gate", "",
  `Run ${stamp}, ${options.repeats} repeat(s), ${options.headed ? "headed" : "headless"} Chrome. `
  + `Verdict: **${required_pass ? "PASS" : "FAIL"}** (window, input, persist, render and textured required on every repeat; pointer is report-only).`, "",
  "Window: the expected viewport is the canvas devicePixelContentBoxSize (the backing size the page observes and publishes as gse_canvas_backing, since the canvas itself belongs to the gpu worker). "
  + `The first resize waits for the engine line \`bench: warmup complete, measuring\` and the next probe frame; budget ${RESIZE_FRAME_BUDGET} ticks.`, "",
  "| case | " + Array.from ({ length: options.repeats }, (_, i) => `run ${i + 1}`).join (" | ") + " |",
  "|---|" + "---|".repeat (options.repeats),
  ...cases.map (c => `| ${c.key}${c.required ? "" : " (report)"} | `
    + Array.from ({ length: options.repeats }, (_, i) => {
        const r = results.find (r => r.key === c.key && r.rep === i + 1);
        return r === undefined ? "-" : r.pass ? "pass" : "FAIL";
      }).join (" | ") + " |"),
  "",
  ...parity,
  ...boot_cost,
  ...results.flatMap (r => [
    `## ${r.name} (${r.scenario}): ${r.pass ? "pass" : "FAIL"}`, "",
    ...r.checks.map (c => `- ${c.pass ? "pass" : "FAIL"} ${c.label}: ${c.detail}`),
    ...r.notes.map (n => `- note: ${n}`),
    `- log: \`${r.log}\``, "",
  ]),
].join ("\n");
writeFileSync (join (out, "report.md"), report);
writeFileSync (join (here, "host_gate", "report.md"), report);
if (any_native) writeFileSync (join (parity_out, "report.md"), report);
console.log (`host_gate: ${required_pass ? "PASS" : "FAIL"}, report ${join (out, "report.md")}`);
process.exit (required_pass ? 0 : 1);
