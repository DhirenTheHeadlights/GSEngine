export const CTRL_BYTES = 256;

export const TEXT_BYTES = 8192;

export const SURFACE_SIZE = 0, SURFACE_VISIBLE = 1, SURFACE_CAPTURE = 2, SURFACE_LOCKED = 3,
             SURFACE_RESIZE_SEQ = 4, SURFACE_MARK = 5;
export const SURFACE_SLOTS = 6, SURFACE_OUT_SLOTS = 6;

export const INPUT_HEAD = 0, INPUT_TAIL = 1, INPUT_DROPPED = 2;
export const INPUT_HEADER_BYTES = 16, INPUT_RECORD_BYTES = 48, INPUT_CAPACITY = 1024;
export const INPUT_CODE_OFFSET = 25, INPUT_CODE_BYTES = 23;
export const INPUT_KEY_DOWN = 1, INPUT_KEY_UP = 2, INPUT_BUTTON_DOWN = 3, INPUT_BUTTON_UP = 4,
             INPUT_MOVE = 5, INPUT_WHEEL = 6, INPUT_TEXT = 7, INPUT_RAW_MOVE = 8;

export function bump (wake) {
  Atomics.add (wake, 0, 1);
  Atomics.notify (wake, 0);
}

export const REPORT ="page_report", EXIT = "page_exit", SPAWN = "page_spawn";
export const RELAUNCH = "page_relaunch", OPEN_URL = "page_open_url";
export const SYNTHETIC_CALLS = [REPORT, EXIT, SPAWN, RELAUNCH, OPEN_URL];

export const LOCK = 0, STATE = 1, RET = 2, OP = 3, TID = 4, WAITING = 5;
export const ARGS_OFFSET = 64, ARGS_COUNT = 12;

export const IDLE = 0, REQUEST = 1, RESPONSE = 2;

export const OK = 0, EBADF = 8, EEXIST = 20, EINVAL = 28, EIO = 29, EISDIR = 31,
             ENOENT = 44, ENOSYS = 52, ENOTDIR = 54, ENOTEMPTY = 55, ENOTCAPABLE = 76;

export const FILETYPE_DIRECTORY = 3, FILETYPE_REGULAR = 4;

export const OFLAG_CREAT = 1, OFLAG_DIRECTORY = 2, OFLAG_EXCL = 4, OFLAG_TRUNC = 8;
export const FDFLAG_APPEND = 1;

export const PREOPEN_FD = 3, FIRST_FD = 4;
export const PREOPEN_PATH = "/";

export const LOCAL_CALLS = new Set ([
  "args_get", "args_sizes_get", "environ_get", "environ_sizes_get",
  "clock_time_get", "clock_res_get", "random_get", "sched_yield", "poll_oneoff",
]);

export function control (buffer, text, tid = 0) {
  return {
    tid,
    i32: new Int32Array (buffer, 0, ARGS_OFFSET / 4),
    args: new BigInt64Array (buffer, ARGS_OFFSET, ARGS_COUNT),
    text: new Uint8Array (text),
  };
}

export const IDLE_REPORT_MS = 5000;

export function call (ctrl, names, name, args, said) {
  const op = names.indexOf (name);
  if (op < 0) return ENOSYS;
  const { i32, args: slots } = ctrl;
  if (Atomics.compareExchange (i32, LOCK, 0, 1) !== 0) {
    Atomics.add (i32, WAITING, 1);
    while (Atomics.compareExchange (i32, LOCK, 0, 1) !== 0)
      Atomics.wait (i32, LOCK, 1);
    Atomics.sub (i32, WAITING, 1);
  }
  if (said !== undefined) {
    const bytes = new TextEncoder ().encode (said).subarray (0, TEXT_BYTES);
    ctrl.text.set (bytes);
    args = [...args, bytes.length];
  }
  for (let i = 0; i < args.length; ++i) slots[i] = BigInt (args[i]);
  Atomics.store (i32, TID, ctrl.tid);
  Atomics.store (i32, OP, op);
  Atomics.store (i32, STATE, REQUEST);
  Atomics.notify (i32, STATE);
  while (Atomics.load (i32, STATE) !== RESPONSE)
    Atomics.wait (i32, STATE, REQUEST);
  const code = Atomics.load (i32, RET);
  Atomics.store (i32, STATE, IDLE);
  Atomics.store (i32, LOCK, 0);
  Atomics.notify (i32, LOCK);
  return code;
}

export const RING_LOCK = 0, RING_HEAD = 1, RING_TAIL = 2;
export const RING_HEADER_BYTES = 64;
export const GPU_RING_WORDS = 1 << 24, GPU_EVENT_WORDS = 1 << 18;

export function make_ring (words) {
  return new SharedArrayBuffer (RING_HEADER_BYTES + words * 4);
}

export function ring (buffer) {
  const words = new Uint32Array (buffer, RING_HEADER_BYTES);
  return { header: new Int32Array (buffer, 0, RING_HEADER_BYTES / 4), words, mask: words.length - 1 };
}

function ring_free (r) {
  const used = (Atomics.load (r.header, RING_HEAD) - Atomics.load (r.header, RING_TAIL)) & r.mask;
  return r.mask - used;
}

function ring_put (r, at, record) {
  const first = Math.min (record.length, r.words.length - at);
  r.words.set (record.subarray (0, first), at);
  if (first < record.length) r.words.set (record.subarray (first), 0);
  return (at + record.length) & r.mask;
}

function ring_put_bytes (r, at, bytes) {
  const whole = bytes.length >> 2;
  let next = ring_put (r, at, new Uint32Array (bytes.buffer.slice (bytes.byteOffset, bytes.byteOffset + whole * 4)));
  if (whole * 4 < bytes.length) {
    const last = new Uint8Array (4);
    last.set (bytes.subarray (whole * 4));
    next = ring_put (r, next, new Uint32Array (last.buffer));
  }
  return next;
}

function ring_copy_in (r, record, tail) {
  let head = ring_put (r, Atomics.load (r.header, RING_HEAD), record);
  if (tail !== undefined) head = ring_put_bytes (r, head, tail);
  Atomics.store (r.header, RING_HEAD, head);
  Atomics.notify (r.header, RING_HEAD);
}

export function ring_write (r, record, tail) {
  const length = record.length + (tail === undefined ? 0 : (tail.length + 3) >> 2);
  if (length * 2 > r.words.length)
    throw new Error (`gpu wire: a ${length}-word record does not fit the ring`);
  while (Atomics.compareExchange (r.header, RING_LOCK, 0, 1) !== 0)
    Atomics.wait (r.header, RING_LOCK, 1, 50);
  while (ring_free (r) < length)
    Atomics.wait (r.header, RING_TAIL, Atomics.load (r.header, RING_TAIL), 50);
  ring_copy_in (r, record, tail);
  Atomics.store (r.header, RING_LOCK, 0);
  Atomics.notify (r.header, RING_LOCK);
}

export function ring_try_write (r, record) {
  if (ring_free (r) < record.length) return false;
  ring_copy_in (r, record);
  return true;
}

export function ring_record (r, at) {
  const length = r.words[(at + 1) & r.mask];
  const out = new Uint32Array (length);
  const first = Math.min (length, r.words.length - at);
  out.set (r.words.subarray (at, at + first));
  if (first < length) out.set (r.words.subarray (0, length - first), first);
  return out;
}

export function ring_drain (r, into, max) {
  while (Atomics.compareExchange (r.header, RING_LOCK, 0, 1) !== 0)
    Atomics.wait (r.header, RING_LOCK, 1, 50);
  let tail = Atomics.load (r.header, RING_TAIL);
  const head = Atomics.load (r.header, RING_HEAD);
  let written = 0;
  while (tail !== head) {
    const length = r.words[(tail + 1) & r.mask];
    if (written + length > max) break;
    into.set (ring_record (r, tail), written);
    written += length;
    tail = (tail + length) & r.mask;
  }
  Atomics.store (r.header, RING_TAIL, tail);
  Atomics.notify (r.header, RING_TAIL);
  Atomics.store (r.header, RING_LOCK, 0);
  Atomics.notify (r.header, RING_LOCK);
  return written;
}
