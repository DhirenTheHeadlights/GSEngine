import {
  call, control, LOCAL_CALLS, REPORT, EXIT, SPAWN, RELAUNCH, OPEN_URL, OK,
  SURFACE_SIZE, SURFACE_VISIBLE, SURFACE_CAPTURE, SURFACE_LOCKED, SURFACE_RESIZE_SEQ, SURFACE_MARK,
  SURFACE_OUT_SLOTS,
  INPUT_HEAD, INPUT_TAIL, INPUT_HEADER_BYTES, INPUT_RECORD_BYTES, INPUT_CAPACITY, bump,
  ring, ring_write, ring_drain,
} from "./protocol.mjs";

let ctrl, names, self_tid;

function report (text) {
  call (ctrl, names, REPORT, [], text);
}

function supply (module, mem, imports) {
  for (const { module: ns, name, kind } of WebAssembly.Module.imports (module)) {
    imports[ns] ??= {};
    if (imports[ns][name] !== undefined) continue;
    imports[ns][name] =
      kind === "memory" ? mem
      : kind === "table" ? new WebAssembly.Table ({ initial: 0, element: "anyfunc" })
      : kind === "function" ? ((x) => x)
      : kind === "global" ? new WebAssembly.Global ({ value: "i32", mutable: true }, 65536)
      : undefined;
  }
  return imports;
}

function monotonic_ms () {
  return performance.timeOrigin + performance.now ();
}

const parked = new Int32Array (new SharedArrayBuffer (4));

const LONG_WAIT_MS = 10000;

function wasi_local (mem, env) {
  const bytes = () => new Uint8Array (mem().buffer);
  const view = () => new DataView (mem().buffer);
  const environ = Object.entries (env).map (([k, v]) => `${k}=${v}`);
  const encode = new TextEncoder ();
  return {
    args_sizes_get: (count, size) => {
      view ().setUint32 (count, 0, true);
      view ().setUint32 (size, 0, true);
      return OK;
    },
    args_get: () => OK,
    environ_sizes_get: (count, size) => {
      const total = environ.reduce ((n, e) => n + encode.encode (e).length + 1, 0);
      view ().setUint32 (count, environ.length, true);
      view ().setUint32 (size, total, true);
      return OK;
    },
    environ_get: (pointers, buffer) => {
      const v = view ();
      let at = buffer;
      environ.forEach ((e, i) => {
        const text = encode.encode (e + "\0");
        bytes ().set (text, at);
        v.setUint32 (pointers + i * 4, at, true);
        at += text.length;
      });
      return OK;
    },
    clock_time_get: (id, precision, out) => {
      view ().setBigUint64 (out, BigInt (Math.round (monotonic_ms () * 1e6)), true);
      return OK;
    },
    clock_res_get: (id, out) => {
      view ().setBigUint64 (out, 1000n, true);
      return OK;
    },
    random_get: (ptr, len) => {
      const fill = new Uint8Array (len);
      crypto.getRandomValues (fill);
      bytes ().set (fill, ptr);
      return OK;
    },
    sched_yield: () => OK,
    poll_oneoff: (subscriptions, events, count, produced) => {
      const v = view ();
      let written = 0;
      for (let i = 0; i < count; ++i) {
        const at = subscriptions + i * 48;
        const userdata = v.getBigUint64 (at, true);
        const tag = v.getUint8 (at + 8);
        if (tag === 0) {
          const absolute = (v.getUint16 (at + 40, true) & 1) !== 0;
          const timeout = Number (v.getBigUint64 (at + 24, true)) / 1e6;
          const ms = absolute ? Math.max (0, timeout - monotonic_ms ()) : timeout;
          if (ms > LONG_WAIT_MS) {
            report (`long sleep: thread ${self_tid} waits ${Math.round (ms)}ms `
                    + `(clock ${v.getUint32 (at + 16, true)}, `
                    + `${absolute ? "absolute" : "relative"} `
                    + `${v.getBigUint64 (at + 24, true)}ns, `
                    + `subscription ${i + 1} of ${count})`);
          }
          Atomics.wait (parked, 0, 0, ms);
        }
        const out = events + written * 32;
        v.setBigUint64 (out, userdata, true);
        v.setUint16 (out + 8, OK, true);
        v.setUint8 (out + 10, tag);
        ++written;
      }
      v.setUint32 (produced, written, true);
      return OK;
    },
  };
}

function wasi_imports (module, mem, env) {
  const local = wasi_local (mem, env);
  const wasi = {};
  for (const { module: ns, name } of WebAssembly.Module.imports (module)) {
    if (!ns.startsWith ("wasi_snapshot")) continue;
    wasi[name] = LOCAL_CALLS.has (name) && local[name] !== undefined
      ? local[name]
      : (...args) => call (ctrl, names, name, args);
  }
  return wasi;
}

function host_env (mem, surface, input, wake, gpu, events) {
  let seen = Atomics.load (wake, 0);
  const commands = gpu ? ring (gpu) : null;
  const replies = events ? ring (events) : null;
  const text = (ptr, len) =>
    new TextDecoder ().decode (new Uint8Array (mem ().buffer, ptr, len).slice ());
  const header = new Int32Array (input, 0, INPUT_HEADER_BYTES / 4);
  const records = new Uint8Array (input, INPUT_HEADER_BYTES);
  return {
    gse_host_surface: (out) => {
      const size = Atomics.load (surface, SURFACE_SIZE);
      const slots = new Int32Array (mem ().buffer, out, SURFACE_OUT_SLOTS);
      slots[0] = Number (size >> 32n);
      slots[1] = Number (size & 0xffffffffn);
      slots[2] = Number (Atomics.load (surface, SURFACE_VISIBLE));
      slots[3] = Number (Atomics.load (surface, SURFACE_LOCKED));
      slots[4] = Number (Atomics.load (surface, SURFACE_RESIZE_SEQ));
      slots[5] = Number (Atomics.load (surface, SURFACE_MARK));
    },
    gse_host_capture: (want) => {
      Atomics.store (surface, SURFACE_CAPTURE, BigInt (want));
      Atomics.notify (surface, SURFACE_CAPTURE);
    },
    gse_host_input: (out, max) => {
      const tail = Atomics.load (header, INPUT_TAIL);
      const count = Math.min ((Atomics.load (header, INPUT_HEAD) - tail) | 0, max);
      const into = new Uint8Array (mem ().buffer, out, count * INPUT_RECORD_BYTES);
      for (let i = 0; i < count; ++i) {
        const at = ((tail + i) & (INPUT_CAPACITY - 1)) * INPUT_RECORD_BYTES;
        into.set (records.subarray (at, at + INPUT_RECORD_BYTES), i * INPUT_RECORD_BYTES);
      }
      Atomics.store (header, INPUT_TAIL, (tail + count) | 0);
      return count;
    },
    gse_host_wait: (ms) => {
      Atomics.wait (wake, 0, seen, ms);
      seen = Atomics.load (wake, 0);
    },
    gse_host_wake: () => bump (wake),
    gse_host_gpu: (ptr, count, tail, tail_bytes) => {
      if (commands === null) throw new Error ("gpu wire: this page has no WebGPU worker");
      ring_write (commands, new Uint32Array (mem ().buffer, ptr, count),
                  tail_bytes > 0 ? new Uint8Array (mem ().buffer, tail, tail_bytes) : undefined);
    },
    gse_host_gpu_poll: (out, max) =>
      replies === null ? 0 : ring_drain (replies, new Uint32Array (mem ().buffer, out, max), max),
    gse_host_relaunch: (ptr, len) => { call (ctrl, names, RELAUNCH, [], text (ptr, len)); },
    gse_host_open_url: (ptr, len) => { call (ctrl, names, OPEN_URL, [], text (ptr, len)); },
  };
}

function spawner (ids) {
  return (arg) => {
    const tid = Atomics.add (ids, 0, 1) + 1;
    report (`thread ${tid} spawned`);
    call (ctrl, names, SPAWN, [tid, arg]);
    return tid;
  };
}

onmessage = async (e) => {
  const { kind, module, mem, text, env, ids, tid, arg, surface, input, wake, gpu, events } = e.data;
  self_tid = tid ?? 1;
  ctrl = control (e.data.ctrl, text, self_tid);
  names = e.data.names;

  if (kind === "thread") report (`thread ${tid} live`);

  let instance;
  const memory = () => instance?.exports?.memory ?? mem;
  const imports = supply (module, mem, {
    wasi_snapshot_preview1: wasi_imports (module, memory, env),
    wasi: { "thread-spawn": spawner (ids) },
    env: host_env (memory, surface, input, wake, gpu, events),
  });
  try {
    instance = await WebAssembly.instantiate (module, imports);
  } catch (error) {
    report (`${kind === "thread" ? `thread ${tid}` : "initial thread"} `
            + `did not instantiate: ${error.message}`);
    call (ctrl, names, EXIT, [3], "did not instantiate");
    return;
  }
  if (kind === "thread") {
    report (`thread ${tid} entering`);
    try {
      instance.exports.wasi_thread_start (tid, arg);
    } catch (error) {
      report (`thread ${tid} trapped: ${error.message}\n${error.stack}`);
      call (ctrl, names, EXIT, [2], `thread ${tid} trapped`);
      return;
    }
    report (`thread ${tid} returned`);
    return;
  }

  let returned;
  try {
    instance.exports._initialize?.();
    if (instance.exports._initialize === undefined)
      instance.exports.__wasm_call_ctors?.();
    returned = instance.exports.entry ();
    instance.exports.__wasm_call_dtors?.();
  } catch (error) {
    report (`trapped: ${error.message}\n${error.stack}`);
    call (ctrl, names, EXIT, [2], "trapped");
    return;
  }
  call (ctrl, names, EXIT, [returned], `entry returned ${returned}`);
};
