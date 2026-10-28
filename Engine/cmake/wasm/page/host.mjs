import {
  control, LOCK, STATE, RET, OP, TID, WAITING, IDLE, REQUEST, RESPONSE,
  IDLE_REPORT_MS,
  OK, EBADF, EEXIST, EINVAL, EIO, EISDIR, ENOENT, ENOSYS, ENOTDIR, ENOTEMPTY,
  FILETYPE_DIRECTORY, FILETYPE_REGULAR,
  OFLAG_CREAT, OFLAG_DIRECTORY, OFLAG_EXCL, OFLAG_TRUNC, FDFLAG_APPEND,
  PREOPEN_FD, FIRST_FD, PREOPEN_PATH,
} from "./protocol.mjs";

let mem, ctrl, names;

const files = new Map ();
const dirs = new Map ([["/", null]]);
const open = new Map ();
let next_fd = FIRST_FD;

let wanted = [];
let relaunch = null;

function fetch_sync (url, binary) {
  const request = new XMLHttpRequest ();
  request.open ("GET", url, false);
  if (binary) request.responseType = "arraybuffer";
  request.send ();
  return request.status === 200 ? request.response : null;
}

function served (prefix, path) {
  return prefix + path.split ("/").map (encodeURIComponent).join ("/");
}

function listed (path) {
  if (!is_dir (path)) return undefined;
  if (dirs.get (path) !== null) return dirs.get (path);
  const children = new Set ();
  dirs.set (path, children);
  const entries = fetch_sync (served ("/fs-list", path), false);
  for (const child of entries === null ? [] : JSON.parse (entries)) {
    const at = join (path, child.name);
    children.add (child.name);
    if (child.dir) { if (!dirs.has (at)) dirs.set (at, null); }
    else if (!files.has (at)) files.set (at, { bytes: null, size: child.size });
  }
  return children;
}

function is_dir (path) {
  if (dirs.has (path)) return true;
  if (files.has (path) || path === "/") return false;
  listed (parent_of (path));
  return dirs.has (path);
}

function file_at (path) {
  is_dir (path);
  return files.get (path);
}

function contents (file, path) {
  if (file.bytes !== null) return file;
  const bytes = fetch_sync (served ("/fs", path), true);
  file.bytes = bytes === null ? new Uint8Array (0) : new Uint8Array (bytes);
  file.size = file.bytes.length;
  return file;
}

function say (text) {
  postMessage ({ kind: "console", text });
}

function parent_of (path) {
  const at = path.lastIndexOf ("/");
  return at <= 0 ? "/" : path.slice (0, at);
}

function name_of (path) {
  return path.slice (path.lastIndexOf ("/") + 1);
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

function view () {
  return new DataView (mem.buffer);
}

function text (ptr, len) {
  return new TextDecoder ().decode (new Uint8Array (mem.buffer, ptr, len).slice ());
}

function said (len) {
  return new TextDecoder ().decode (ctrl.text.slice (0, len));
}

function resolve (fd, ptr, len) {
  const held = open.get (fd);
  const base = fd === PREOPEN_FD ? PREOPEN_PATH : held?.dir ? held.path : undefined;
  return base === undefined ? undefined : join (base, text (ptr, len));
}

function grow (file, needed) {
  if (file.bytes.length >= needed) return;
  const capacity = Math.max (needed, file.bytes.length * 2, 4096);
  const grown = new Uint8Array (capacity);
  grown.set (file.bytes.subarray (0, file.size));
  file.bytes = grown;
}

function iovecs (ptr, count) {
  const v = view ();
  const out = [];
  for (let i = 0; i < count; ++i)
    out.push ([v.getUint32 (ptr + i * 8, true), v.getUint32 (ptr + i * 8 + 4, true)]);
  return out;
}

function filestat (ptr, path) {
  const v = view ();
  const directory = is_dir (path);
  const size = directory ? 0 : files.get (path).size;
  for (let at = 0; at < 64; at += 8) v.setBigUint64 (ptr + at, 0n, true);
  v.setUint8 (ptr + 16, directory ? FILETYPE_DIRECTORY : FILETYPE_REGULAR);
  v.setBigUint64 (ptr + 24, 1n, true);
  v.setBigUint64 (ptr + 32, BigInt (size), true);
  return OK;
}

function harvest () {
  const out = [];
  for (const path of wanted) {
    const file = files.get (path);
    if (file === undefined || file.bytes === null) {
      say (`host: ${path} was asked for but the run never wrote it`);
      continue;
    }
    out.push ({ path, bytes: file.bytes.slice (0, file.size) });
  }
  return out;
}

const calls = {
  fd_write (fd, iovs, count, written) {
    const parts = iovecs (iovs, count);
    let total = 0;
    for (const [ptr, len] of parts) total += len;
    if (fd === 1 || fd === 2) {
      let out = "";
      for (const [ptr, len] of parts) out += text (ptr, len);
      note_role (out);
      say (out.replace (/\n$/, ""));
    } else {
      const held = open.get (fd);
      if (held === undefined || held.dir) return held === undefined ? EBADF : EISDIR;
      const file = contents (files.get (held.path), held.path);
      const at = held.append ? file.size : held.pos;
      grow (file, at + total);
      let cursor = at;
      for (const [ptr, len] of parts) {
        file.bytes.set (new Uint8Array (mem.buffer, ptr, len), cursor);
        cursor += len;
      }
      file.size = Math.max (file.size, cursor);
      held.pos = cursor;
    }
    view ().setUint32 (written, total, true);
    return OK;
  },

  fd_read (fd, iovs, count, read) {
    const held = open.get (fd);
    if (held === undefined) return EBADF;
    if (held.dir) return EISDIR;
    const file = contents (files.get (held.path), held.path);
    let total = 0;
    for (const [ptr, len] of iovecs (iovs, count)) {
      const take = Math.min (len, file.size - held.pos);
      if (take <= 0) break;
      new Uint8Array (mem.buffer, ptr, take)
        .set (file.bytes.subarray (held.pos, held.pos + take));
      held.pos += take;
      total += take;
    }
    view ().setUint32 (read, total, true);
    return OK;
  },

  fd_seek (fd, offset, whence, out) {
    const held = open.get (fd);
    if (held === undefined) return EBADF;
    const size = held.dir ? 0 : files.get (held.path).size;
    const base = whence === 0 ? 0 : whence === 1 ? held.pos : size;
    held.pos = Math.max (0, base + Number (offset));
    view ().setBigUint64 (out, BigInt (held.pos), true);
    return OK;
  },

  fd_tell (fd, out) {
    const held = open.get (fd);
    if (held === undefined) return EBADF;
    view ().setBigUint64 (out, BigInt (held.pos), true);
    return OK;
  },

  fd_close (fd) {
    return open.delete (fd) ? OK : EBADF;
  },

  fd_renumber (from, to) {
    const held = open.get (from);
    if (held === undefined) return EBADF;
    open.set (to, held);
    open.delete (from);
    return OK;
  },

  fd_fdstat_get (fd, ptr) {
    const v = view ();
    const held = open.get (fd);
    const directory = fd === PREOPEN_FD || (held?.dir ?? false);
    if (fd > 2 && fd !== PREOPEN_FD && held === undefined) return EBADF;
    v.setUint8 (ptr, fd <= 2 ? 2 : directory ? FILETYPE_DIRECTORY : FILETYPE_REGULAR);
    v.setUint16 (ptr + 2, held?.append ? FDFLAG_APPEND : 0, true);
    v.setBigUint64 (ptr + 8, ~0n, true);
    v.setBigUint64 (ptr + 16, ~0n, true);
    return OK;
  },

  fd_fdstat_set_flags (fd, flags) {
    const held = open.get (fd);
    if (held === undefined) return EBADF;
    held.append = (flags & FDFLAG_APPEND) !== 0;
    return OK;
  },

  fd_filestat_get (fd, ptr) {
    if (fd === PREOPEN_FD) return filestat (ptr, PREOPEN_PATH);
    const held = open.get (fd);
    if (held === undefined) return EBADF;
    return filestat (ptr, held.path);
  },

  fd_filestat_set_size (fd, size) {
    const held = open.get (fd);
    if (held === undefined) return EBADF;
    if (held.dir) return EISDIR;
    const file = contents (files.get (held.path), held.path);
    grow (file, Number (size));
    file.size = Number (size);
    return OK;
  },

  fd_sync () { return OK; },
  fd_datasync () { return OK; },
  fd_advise () { return OK; },
  fd_allocate () { return OK; },

  fd_prestat_get (fd, ptr) {
    if (fd !== PREOPEN_FD) return EBADF;
    const v = view ();
    v.setUint8 (ptr, 0);
    v.setUint32 (ptr + 4, PREOPEN_PATH.length, true);
    return OK;
  },

  fd_prestat_dir_name (fd, ptr, len) {
    if (fd !== PREOPEN_FD) return EBADF;
    if (len < PREOPEN_PATH.length) return EINVAL;
    new Uint8Array (mem.buffer, ptr, PREOPEN_PATH.length)
      .set (new TextEncoder ().encode (PREOPEN_PATH));
    return OK;
  },

  fd_readdir (fd, buf, len, cookie, used) {
    const held = open.get (fd);
    const path = fd === PREOPEN_FD ? PREOPEN_PATH : held?.path;
    if (path === undefined) return EBADF;
    if (!is_dir (path)) return ENOTDIR;
    const entries = [...listed (path)].sort ();
    const v = view ();
    const bytes = new Uint8Array (mem.buffer);
    let at = 0;
    for (let i = Number (cookie); i < entries.length; ++i) {
      const name = new TextEncoder ().encode (entries[i]);
      if (at + 24 + name.length > len) break;
      const child = join (path, entries[i]);
      v.setBigUint64 (buf + at, BigInt (i + 1), true);
      v.setBigUint64 (buf + at + 8, BigInt (i + 1), true);
      v.setUint32 (buf + at + 16, name.length, true);
      v.setUint8 (buf + at + 20, dirs.has (child) ? FILETYPE_DIRECTORY : FILETYPE_REGULAR);
      bytes.set (name, buf + at + 24);
      at += 24 + name.length;
    }
    v.setUint32 (used, at, true);
    return OK;
  },

  path_open (fd, dirflags, ptr, len, oflags, base_rights, inheriting, fdflags, out) {
    const path = resolve (fd, ptr, len);
    if (path === undefined) return EBADF;
    const directory = is_dir (path);
    let file = files.get (path);
    if (directory && (oflags & OFLAG_CREAT)) return EEXIST;
    if (!directory && file === undefined) {
      if (!(oflags & OFLAG_CREAT)) return ENOENT;
      if (!is_dir (parent_of (path))) return ENOENT;
      file = { bytes: new Uint8Array (0), size: 0 };
      files.set (path, file);
      listed (parent_of (path)).add (name_of (path));
    } else if ((oflags & OFLAG_EXCL) && (oflags & OFLAG_CREAT)) {
      return EEXIST;
    }
    if (!directory && (oflags & OFLAG_DIRECTORY)) return ENOTDIR;
    if (!directory && (oflags & OFLAG_TRUNC)) { file.bytes = new Uint8Array (0); file.size = 0; }
    const opened = next_fd++;
    open.set (opened, { path, pos: 0, dir: directory,
                        append: (fdflags & FDFLAG_APPEND) !== 0 });
    view ().setUint32 (out, opened, true);
    return OK;
  },

  path_filestat_get (fd, flags, ptr, len, buf) {
    const path = resolve (fd, ptr, len);
    if (path === undefined) return EBADF;
    if (!is_dir (path) && !files.has (path)) return ENOENT;
    return filestat (buf, path);
  },

  path_create_directory (fd, ptr, len) {
    const path = resolve (fd, ptr, len);
    if (path === undefined) return EBADF;
    if (is_dir (path) || files.has (path)) return EEXIST;
    if (!is_dir (parent_of (path))) return ENOENT;
    listed (parent_of (path)).add (name_of (path));
    dirs.set (path, new Set ());
    return OK;
  },

  path_remove_directory (fd, ptr, len) {
    const path = resolve (fd, ptr, len);
    if (path === undefined) return EBADF;
    if (!is_dir (path)) return ENOTDIR;
    if (listed (path).size > 0) return ENOTEMPTY;
    dirs.delete (path);
    listed (parent_of (path)).delete (name_of (path));
    return OK;
  },

  path_unlink_file (fd, ptr, len) {
    const path = resolve (fd, ptr, len);
    if (path === undefined) return EBADF;
    if (is_dir (path)) return EISDIR;
    if (!files.delete (path)) return ENOENT;
    listed (parent_of (path)).delete (name_of (path));
    return OK;
  },

  path_rename (fd, ptr, len, to_fd, to_ptr, to_len) {
    const from = resolve (fd, ptr, len);
    const to = resolve (to_fd, to_ptr, to_len);
    if (from === undefined || to === undefined) return EBADF;
    if (is_dir (from)) return EISDIR;
    const moving = files.get (from);
    if (moving === undefined) return ENOENT;
    if (!is_dir (parent_of (to)) || is_dir (to)) return ENOENT;
    files.set (to, contents (moving, from));
    files.delete (from);
    listed (parent_of (from)).delete (name_of (from));
    listed (parent_of (to)).add (name_of (to));
    return OK;
  },

  path_filestat_set_times () { return OK; },
  path_readlink () { return EINVAL; },
  path_symlink () { return ENOSYS; },
  path_link () { return ENOSYS; },

  page_report (len) {
    say (said (len));
    return OK;
  },

  page_spawn (tid, arg) {
    postMessage ({ kind: "spawn", tid, arg });
    return OK;
  },

  page_relaunch (len) {
    relaunch = said (len);
    return OK;
  },

  page_open_url (len) {
    postMessage ({ kind: "open_url", url: said (len) });
    return OK;
  },

  page_exit (code, len) {
    say (said (len));
    postMessage ({ kind: "exit", code, files: harvest (), relaunch });
    return OK;
  },

  proc_exit (code) {
    postMessage ({ kind: "exit", code: Number (code), files: harvest (), relaunch });
    return OK;
  },
};

const last = new Map ();

const writers = new Map ();
let serving = 0;

function note_role (out) {
  const tag = /^(?:\x1b\[[0-9;]*m)?\[[^\]]*\]\[[^\]]*\]\[[^\]]*\]\[([^\]]+)\]/.exec (out);
  if (tag === null) return;
  if (!writers.has (serving)) writers.set (serving, new Set ());
  writers.get (serving).add (tag[1]);
}

function idle_report () {
  const rows = [...last.entries ()].sort ((a, b) => a[0] - b[0])
    .map (([tid, seen]) =>
      `${tid}:${seen.name} ${Math.round (performance.now () - seen.at)}ms ago`);
  const wrote = [...writers.entries ()].sort ((a, b) => a[0] - b[0])
    .map (([tid, seen]) => `${tid}:${[...seen].join ("/")}`);
  say (`host: no syscall for ${IDLE_REPORT_MS}ms; `
       + `${Atomics.load (ctrl.i32, WAITING)} waiting on the channel, `
       + `lock ${Atomics.load (ctrl.i32, LOCK)}; last per thread ${rows.join (", ")}`
       + `; lines written by ${wrote.join (", ")}`);
}

function serve () {
  const { i32, args } = ctrl;
  for (;;) {
    while (Atomics.load (i32, STATE) !== REQUEST)
      if (Atomics.wait (i32, STATE, IDLE, IDLE_REPORT_MS) === "timed-out")
        idle_report ();
    const name = names[Atomics.load (i32, OP)];
    serving = Atomics.load (i32, TID);
    last.set (serving, { name, at: performance.now () });
    const call = calls[name];
    let code;
    if (call === undefined) {
      say (`host: ${name} is not implemented by this harness`);
      code = ENOSYS;
    } else {
      const passed = [];
      for (let i = 0; i < call.length; ++i)
        passed.push (name === "fd_seek" && i === 1 ? args[i]
                     : name === "fd_readdir" && i === 3 ? args[i]
                     : name === "fd_filestat_set_size" && i === 1 ? args[i]
                     : name === "proc_exit" ? args[i]
                     : Number (args[i]));
      try {
        code = call (...passed);
      } catch (e) {
        say (`host: ${name} failed: ${e.message}`);
        code = EIO;
      }
    }
    Atomics.store (i32, RET, code);
    Atomics.store (i32, STATE, RESPONSE);
    Atomics.notify (i32, STATE);
  }
}

onmessage = (e) => {
  ({ mem, names } = e.data);
  ctrl = control (e.data.ctrl, e.data.text);
  wanted = e.data.collect;
  postMessage ({ kind: "ready" });
  serve ();
};
