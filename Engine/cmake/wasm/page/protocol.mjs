export const CTRL_BYTES = 256;

export const TEXT_BYTES = 8192;

export const REPORT = "page_report", EXIT = "page_exit", SPAWN = "page_spawn";
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
