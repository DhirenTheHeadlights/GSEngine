import { closeSync, constants, fdatasyncSync, fstatSync, fsyncSync, ftruncateSync,
         openSync, readFileSync, readSync, readdirSync, statSync, writeSync } from "node:fs";
import { WASI } from "node:wasi";
import { Worker, isMainThread, workerData } from "node:worker_threads";

export const TRAP_EXIT = 2, NO_INSTANCE_EXIT = 3;

const OWN_FD_BASE = 1 << 20, OWN_FD_LIMIT = 4096;
const OWN_OPEN = 1, OWN_APPEND = 2;
const RIGHT_FD_READ = 1n << 1n, RIGHT_FD_WRITE = 1n << 6n;
const OFLAG_CREAT = 1, OFLAG_DIRECTORY = 2, OFLAG_EXCL = 4, OFLAG_TRUNC = 8, FDFLAG_APPEND = 1;
const FILETYPE_REGULAR = 4, ALL_RIGHTS = 0xffffffffffffffffn;
const EINVAL = 28, EIO = 29, EMFILE = 33;
const ERRNO = { EACCES: 2, EEXIST: 20, EISDIR: 31, ENOENT: 44, ENOTDIR: 54, EPERM: 63 };

function hosting (mounts, env) {
  return new WASI ({ version: "preview1", env, ...(mounts ? { preopens: mounts } : {}) });
}

function memory (shared, initial, maximum) {
  return shared
    ? new WebAssembly.Memory ({ initial, maximum, shared: true })
    : new WebAssembly.Memory ({ initial });
}

function shared_files () {
  return { pos: new BigInt64Array (new SharedArrayBuffer (8 * OWN_FD_LIMIT)),
           state: new Int32Array (new SharedArrayBuffer (4 * OWN_FD_LIMIT)) };
}

function supply (module, mem) {
  const imports = {};
  for (const { module: ns, name, kind } of WebAssembly.Module.imports (module)) {
    imports[ns] ??= {};
    imports[ns][name] =
      kind === "memory" ? mem
      : kind === "table" ? new WebAssembly.Table ({ initial: 0, element: "anyfunc" })
      : kind === "function" ? ((x) => x)
      : kind === "global" ? new WebAssembly.Global ({ value: "i32", mutable: true }, 64 * 1024)
      : undefined;
  }
  return imports;
}

function join (base, relative) {
  const parts = (relative.startsWith ("/") ? relative : base + "/" + relative).split ("/");
  const out = [];
  for (const part of parts) {
    if (part === "" || part === ".") continue;
    if (part === "..") out.pop (); else out.push (part);
  }
  return "/" + out.join ("/");
}

function host_of (mounts, guest) {
  let best = "";
  for (const prefix of Object.keys (mounts))
    if (prefix.length > best.length
        && (prefix === "/" || guest === prefix || guest.startsWith (prefix + "/")))
      best = prefix;
  return mounts[best].replace (/[\\/]+$/, "") + "/" + guest.slice (best.length).replace (/^\/+/, "");
}

function errno (e) {
  return ERRNO[e.code] ?? EIO;
}

function filesystem (memory, mounts, wasi, files) {
  const wrapped = { ...wasi.getImportObject ().wasi_snapshot_preview1 };
  const paths = new Map (Object.keys (mounts ?? {}).map ((guest, i) => [3 + i, guest]));
  const view = () => new DataView (memory ().buffer);
  const text = (ptr, len) =>
    new TextDecoder ().decode (new Uint8Array (memory ().buffer, ptr, len));

  const own = (fd) =>
    fd >= OWN_FD_BASE && fd < OWN_FD_BASE + OWN_FD_LIMIT
      && (Atomics.load (files.state, fd - OWN_FD_BASE) & OWN_OPEN) ? fd - OWN_FD_BASE : -1;

  const transfer = (os, iovs, count, at, io) => {
    const v = view ();
    let total = 0;
    for (let i = 0; i < count; ++i) {
      const len = v.getUint32 (iovs + i * 8 + 4, true);
      const done = io (os, new Uint8Array (memory ().buffer, v.getUint32 (iovs + i * 8, true), len),
                       0, len, at + total);
      total += done;
      if (done < len) break;
    }
    return total;
  };

  const position = (os) => Number (Atomics.load (files.pos, os));
  const place = (os, at) => Atomics.store (files.pos, os, BigInt (at));

  const handle = (name, call) => {
    const fallback = wrapped[name];
    wrapped[name] = (fd, ...rest) => {
      const os = own (fd);
      if (os < 0) return fallback (fd, ...rest);
      try {
        return call (os, ...rest);
      } catch (e) {
        return errno (e);
      }
    };
  };

  const open = wrapped.path_open;
  wrapped.path_open = (fd, dirflags, ptr, len, oflags, base, inheriting, fdflags, out) => {
    const guest = paths.has (fd) ? join (paths.get (fd), text (ptr, len)) : undefined;
    if (guest !== undefined && !(oflags & OFLAG_DIRECTORY)) {
      const host = host_of (mounts, guest);
      let directory = false;
      try { directory = statSync (host).isDirectory (); } catch {}
      if (!directory) {
        const reading = (base & RIGHT_FD_READ) !== 0n, writing = (base & RIGHT_FD_WRITE) !== 0n;
        const mode = (writing ? (reading ? constants.O_RDWR : constants.O_WRONLY) : constants.O_RDONLY)
          | (oflags & OFLAG_CREAT ? constants.O_CREAT : 0)
          | (oflags & OFLAG_EXCL ? constants.O_EXCL : 0)
          | (oflags & OFLAG_TRUNC ? constants.O_TRUNC : 0);
        let os;
        try {
          os = openSync (host, mode);
        } catch (e) {
          return errno (e);
        }
        if (os >= OWN_FD_LIMIT) {
          closeSync (os);
          return EMFILE;
        }
        place (os, 0);
        Atomics.store (files.state, os, OWN_OPEN | (fdflags & FDFLAG_APPEND ? OWN_APPEND : 0));
        view ().setUint32 (out, OWN_FD_BASE + os, true);
        return 0;
      }
    }
    const code = open (fd, dirflags, ptr, len, oflags, base, inheriting, fdflags, out);
    if (code === 0 && guest !== undefined)
      paths.set (view ().getUint32 (out, true), guest);
    return code;
  };

  const close = wrapped.fd_close;
  wrapped.fd_close = (fd) => {
    const os = own (fd);
    if (os < 0) { paths.delete (fd); return close (fd); }
    Atomics.store (files.state, os, 0);
    try {
      closeSync (os);
    } catch (e) {
      return errno (e);
    }
    return 0;
  };

  handle ("fd_write", (os, iovs, count, written) => {
    const at = Atomics.load (files.state, os) & OWN_APPEND ? fstatSync (os).size : position (os);
    const total = transfer (os, iovs, count, at, writeSync);
    place (os, at + total);
    view ().setUint32 (written, total, true);
    return 0;
  });

  handle ("fd_read", (os, iovs, count, read) => {
    const at = position (os);
    const total = transfer (os, iovs, count, at, readSync);
    place (os, at + total);
    view ().setUint32 (read, total, true);
    return 0;
  });

  handle ("fd_pwrite", (os, iovs, count, offset, written) => {
    view ().setUint32 (written, transfer (os, iovs, count, Number (offset), writeSync), true);
    return 0;
  });

  handle ("fd_pread", (os, iovs, count, offset, read) => {
    view ().setUint32 (read, transfer (os, iovs, count, Number (offset), readSync), true);
    return 0;
  });

  handle ("fd_seek", (os, offset, whence, out) => {
    const from = whence === 0 ? 0 : whence === 1 ? position (os) : fstatSync (os).size;
    const at = from + Number (offset);
    if (at < 0) return EINVAL;
    place (os, at);
    view ().setBigUint64 (out, BigInt (at), true);
    return 0;
  });

  handle ("fd_tell", (os, out) => {
    view ().setBigUint64 (out, BigInt (position (os)), true);
    return 0;
  });

  handle ("fd_fdstat_get", (os, ptr) => {
    const v = view ();
    v.setUint32 (ptr, 0, true);
    v.setUint32 (ptr + 4, 0, true);
    v.setUint8 (ptr, FILETYPE_REGULAR);
    v.setUint16 (ptr + 2, Atomics.load (files.state, os) & OWN_APPEND ? FDFLAG_APPEND : 0, true);
    v.setBigUint64 (ptr + 8, ALL_RIGHTS, true);
    v.setBigUint64 (ptr + 16, ALL_RIGHTS, true);
    return 0;
  });

  handle ("fd_fdstat_set_flags", (os, flags) => {
    Atomics.store (files.state, os, OWN_OPEN | (flags & FDFLAG_APPEND ? OWN_APPEND : 0));
    return 0;
  });

  handle ("fd_filestat_get", (os, ptr) => {
    const stat = fstatSync (os, { bigint: true });
    const v = view ();
    for (let at = 0; at < 64; at += 8) v.setBigUint64 (ptr + at, 0n, true);
    v.setBigUint64 (ptr, stat.dev, true);
    v.setBigUint64 (ptr + 8, stat.ino, true);
    v.setUint8 (ptr + 16, FILETYPE_REGULAR);
    v.setBigUint64 (ptr + 24, stat.nlink, true);
    v.setBigUint64 (ptr + 32, stat.size, true);
    v.setBigUint64 (ptr + 40, stat.atimeNs, true);
    v.setBigUint64 (ptr + 48, stat.mtimeNs, true);
    v.setBigUint64 (ptr + 56, stat.ctimeNs, true);
    return 0;
  });

  handle ("fd_filestat_set_size", (os, size) => {
    ftruncateSync (os, Number (size));
    return 0;
  });

  handle ("fd_sync", (os) => {
    fsyncSync (os);
    return 0;
  });

  handle ("fd_datasync", (os) => {
    fdatasyncSync (os);
    return 0;
  });

  wrapped.fd_readdir = (fd, buf, len, cookie, used) => {
    const guest = paths.get (fd);
    if (guest === undefined) return 8;
    let entries;
    try {
      entries = readdirSync (host_of (mounts, guest), { withFileTypes: true });
    } catch (e) {
      return e.code === "ENOTDIR" ? 54 : 52;
    }
    const v = view ();
    const bytes = new Uint8Array (memory ().buffer);
    let at = 0;
    for (let i = Number (cookie); i < entries.length; ++i) {
      const name = new TextEncoder ().encode (entries[i].name);
      if (at + 24 + name.length > len) break;
      v.setBigUint64 (buf + at, BigInt (i + 1), true);
      v.setBigUint64 (buf + at + 8, BigInt (i + 1), true);
      v.setUint32 (buf + at + 16, name.length, true);
      v.setUint8 (buf + at + 20, entries[i].isDirectory () ? 3
                                 : entries[i].isSymbolicLink () ? 7 : 4);
      bytes.set (name, buf + at + 24);
      at += 24 + name.length;
    }
    v.setUint32 (used, at, true);
    return 0;
  };
  return { wasi_snapshot_preview1: wrapped };
}

function trace_thread (text) {
  writeSync (1, text + "\n");
}

const RELAUNCH_BYTES = 8192;

function shared_requests () {
  return { length: new Int32Array (new SharedArrayBuffer (4)).fill (-1),
           text: new Uint8Array (new SharedArrayBuffer (RELAUNCH_BYTES)) };
}

function relaunch_requested (requests) {
  const length = Atomics.load (requests.length, 0);
  return length < 0 ? null : new TextDecoder ().decode (requests.text.slice (0, length));
}

function host_env (imports, memory, requests) {
  const bytes = (ptr, len) => new Uint8Array (memory ().buffer, ptr, len).slice ();
  imports.env ??= {};
  imports.env.gse_host_relaunch = (ptr, len) => {
    const line = bytes (ptr, Math.min (len, RELAUNCH_BYTES));
    requests.text.set (line);
    Atomics.store (requests.length, 0, line.length);
  };
  imports.env.gse_host_open_url = (ptr, len) => {
    trace_thread (`open_url: ${new TextDecoder ().decode (bytes (ptr, len))} (no browser under node)`);
  };
  return imports;
}

function spawner (module, mem, ids, mounts, env, files, requests) {
  return (arg) => {
    const tid = Atomics.add (ids, 0, 1) + 1;
    const worker = new Worker (new URL (import.meta.url),
                               { workerData: { module, mem, ids, tid, arg, mounts, env, files, requests } });
    worker.unref ();
    trace_thread (`thread ${tid} spawned`);
    return tid;
  };
}

if (!isMainThread && workerData?.tid !== undefined) {
  const { module, mem, ids, tid, arg, mounts, env, files, requests } = workerData;
  const wasi = hosting (mounts, env);
  let instance;
  const imports = host_env ({
    ...supply (module, mem),
    ...filesystem (() => instance?.exports?.memory ?? mem, mounts, wasi, files),
    wasi: { "thread-spawn": spawner (module, mem, ids, mounts, env, files, requests) },
  }, () => instance?.exports?.memory ?? mem, requests);
  try {
    instance = await WebAssembly.instantiate (module, imports);
  } catch (e) {
    trace_thread (`thread ${tid} did not instantiate: ${e.message}`);
    process.exit (NO_INSTANCE_EXIT);
  }
  wasi.initialize ({ exports: { memory: instance.exports.memory ?? mem } });
  try {
    instance.exports.wasi_thread_start (tid, arg);
  } catch (e) {
    trace_thread (`thread ${tid} trapped: ${e.message}\n${e.stack}`);
    process.exit (TRAP_EXIT);
  }
  trace_thread (`thread ${tid} returned`);
  process.exit (0);
}

export async function run ({ module: path, want = 0, shared, root, mounts = {}, env = {},
                             initial_pages, max_pages, entry = "entry", relaunch }) {
  let instance, wasi;
  const requests = shared_requests ();
  try {
    const module = new WebAssembly.Module (readFileSync (path));
    const mem = memory (shared, initial_pages, max_pages);
    const ids = new Int32Array (new SharedArrayBuffer (4));
    ids[0] = 1;
    const mounted = root ? { "/": root, ...mounts } : null;
    const files = shared_files ();
    wasi = WebAssembly.Module.imports (module).some (i => i.module.startsWith ("wasi_"))
           ? hosting (mounted, env) : null;
    instance = await WebAssembly.instantiate (module, host_env ({
      ...supply (module, mem),
      ...(wasi ? filesystem (() => instance?.exports?.memory ?? mem, mounted, wasi, files) : {}),
      wasi: { "thread-spawn": spawner (module, mem, ids, mounted, env, files, requests) },
    }, () => instance?.exports?.memory ?? mem, requests));
  } catch (e) {
    console.log (`did not instantiate: ${e.message}`);
    process.exit (NO_INSTANCE_EXIT);
  }

  let got;
  try {
    if (wasi)
      wasi.initialize (instance);
    else
      instance.exports.__wasm_call_ctors?.();
    got = instance.exports[entry] ();
    instance.exports.__wasm_call_dtors?.();
  } catch (e) {
    console.log (`trapped: ${e.message}\n${e.stack}`);
    process.exit (TRAP_EXIT);
  }
  console.log (got === want ? `ok ${got}` : `got ${got} want ${want}`);
  const line = relaunch_requested (requests);
  if (line !== null && relaunch !== undefined)
    process.exit (await relaunch (line));
  process.exit (got === want ? 0 : 1);
}
