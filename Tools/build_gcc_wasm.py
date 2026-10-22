"""Build an experimental wasm32 GCC cross compiler from a trunk snapshot plus the
vendored WebAssembly backend patches (Tools/gcc-toolchain/wasm).

This is NOT the toolchain the engine is built with, and it does not touch it. The
backend is an out-of-tree patch series (see Tools/gcc-toolchain/wasm/README.md), so
this builds into its own prefix: driver, cc1, cc1plus, libgcc and libstdc++, against
a downloaded wasi-libc sysroot. Measuring what that compiler can and cannot do is the
point — see docs/wasm_pipeline_plan.md.

It used to stop at `make all-gcc`, with no target library at all, because there was no
libc for one to be built against and no linker to put the result together. Both of
those are gone: step 7 closed linking, and wasi-libc supplies the libc.

Probes that carry an expected result are assembled and executed, not just compiled:
llvm-mc turns the emitted assembly into a relocatable object, wasm-ld links it, and
node runs it. That is the only way to tell correct codegen from plausible-looking
codegen, and it is what every later claim about this backend gets measured against.

There is one run pipeline where there used to be two. The old `unlinked` one was
wat2wasm on the emitted module text, and it was worth having because it answered "is
this codegen right" independently of "can this output be linked" — rwat could place
neither a static nor a function pointer, so the linked pipeline was red for reasons
that had nothing to do with GCC. It cannot survive the move to llvm-mc, whose input is
assembly rather than a module, and it no longer has a question to answer: see
docs/wasm_pipeline_plan.md "Step 7". The cost is that a miscompile now surfaces only
through the link.

Unlike build_gcc_trunk.py this configures GCC directly instead of going through
makepkg. The vendored PKGBUILD exists to make GCC build *for* native Windows, and its
patch stack is mingw-target-specific; a cross whose host compiler is the already-working
MSYS2 gcc needs none of it.

    python Tools/build_gcc_wasm.py --snapshot 20260809
"""
import argparse
import collections
import hashlib
import json
import os
import re
import shlex
import shutil
from pathlib import Path
from typing import NamedTuple, Optional

import module_graph
from msys2 import DEFAULT_MSYS_ROOT, REPO_ROOT, bash, to_msys
# The two patterns a scenario run's console output is read with. Imported rather than
# written again: `parity_gate.py` is what decides whether two runs of a scenario agree,
# and a browser run that printed a line this harness recognised and that gate did not
# would be a pass criterion met against the wrong contract.
from parity_gate import hash_pattern, p50_pattern

# The snapshot the engine's own native toolchain is built from, which is what this has
# to be for an engine-source probe to mean anything: the series' merge base, 20260524,
# rejects `Hash.cppm' and fourteen modules cascade off it, and that is eleven weeks of
# frontend drift rather than a target gap (step 14). Matching the engine's compiler is
# also what keeps a red row readable -- a failure here is then the target's, not a
# difference between two trunks.
#
# Not the newest snapshot, deliberately. Newest would add nine more weeks of middle-end
# drift for the patch series to survive, and would make every red engine row ambiguous
# between a target gap and a trunk regression the engine has never seen.
DEFAULT_SNAPSHOT = "20260809"

# config.sub rejects LLVM's wasm32-unknown-unknown ("OS `unknown' not recognized") and
# canonicalises wasm32-none to this. wasm32-wasi also configures, but names a WASI libc
# that does not exist here; the backend's config.gcc stanza matches any wasm*-*-*.
TARGET = "wasm32-unknown-none"

SNAPSHOT_URL = "https://gcc.gnu.org/pub/gcc/snapshots/17-{snapshot}/gcc-17-{snapshot}.tar.xz"

# wasi-libc, as the target's C library. It is the only libc that exists for wasm32
# without writing one, and taking it is what makes a *hosted* libstdc++ possible --
# which is the whole point, because libstdc++'s `std` module is built only in hosted
# mode (libstdc++-v3/src/c++23/Makefile.am gates std.cc on GLIBCXX_HOSTED), and the
# engine's own code imports std.
#
# Its objects are ordinary wasm objects, so wasm-ld links them beside GCC's without
# anything knowing which compiler produced which -- a property the move to llvm-mc
# bought and the annotated dialect could never have had.
#
# Pinned, and the pin is load-bearing: wasi-libc's headers are only as C++-clean as
# the clang they were last tested against. 27.0's wasi/api.h has bare
# `_Static_assert (_Alignof (...))', which no C++ compiler accepts and which the first
# #include <cstdio> reaches; 32.0 does not. What no checked release avoids is
# `_Thread_local' on errno -- see WASI_TARGET.
#
# The cost, stated because it is load-bearing later: this makes the measurement target
# WASI, not a browser. A browser build needs either a WASI shim in JS or the engine's
# own host layer in place of one. That is a known-shaped job and the engine needs a
# host layer regardless, but nothing here has measured it.
WASI_SYSROOT_VERSION = "32.0"
WASI_SYSROOT_URL = ("https://github.com/WebAssembly/wasi-sdk/releases/download/"
                    "wasi-sdk-{major}/wasi-sysroot-{version}.tar.gz")

# Which of the tarball's targets is flattened. This decides what the measurement is OF
# and not merely where the files come from, so it belongs in the sysroot's cache key
# beside the version -- and is in it, for the reason stated on SYSROOT_EDITS below.
#
# wasm32-wasip1-threads is the threads variant, taken in step 19. It is what makes
# <mutex> declare anything: libstdc++ chooses its locking and atomicity implementation
# from whether a gthread model is present, and the non-threads variant has no pthreads
# for --enable-threads=posix to find. That is why six of step 18's eight rejected units
# were std::mutex not existing.
#
# It is not simply a longer file list. The variant declares errno thread-local for real,
# which is what makes native TLS in the backend a prerequisite rather than a nicety --
# see SYSROOT_EDITS.
WASI_TARGET = "wasm32-wasip1-threads"

# What declare_setjmp does to the flattened headers, named so the sysroot stamp can
# record it. The sysroot's cache key is every input that decides what was measured, and
# an edit to a header is one of those as much as the version is: without this in the
# key, an existing workdir goes on measuring the unedited headers and reports that as
# the result of the edit.
#
# `errno-plain' was the second entry and is gone, with plain_errno itself. Step 17 added
# it because a non-threads wasi-libc built by clang puts errno in ordinary data while
# its header claims `_Thread_local', so GCC asked for an emutls control object nobody
# defined. On the threads variant the header is telling the truth -- errno really is a
# TLS symbol in libc.a -- so rewriting the declaration would be step 17's bug pointed
# the other way: C++ would read a non-thread-local address that libc never writes, and
# every thread but the first would see another thread's errno. What replaces the rewrite
# is the backend actually implementing TLS.
SYSROOT_EDITS = "setjmp-declared"

# Tools the run stage needs, installed into the repo-local MSYS2 on demand. Deliberately
# not added to install_msys2.py's list: that list is read out of build-gcc-trunk.yml so
# CI and local builds share one package closure, and these are not GCC build dependencies.
# llvm supplies llvm-mc, the assembler the backend's ASM_SPEC now names; lld supplies
# wasm-ld; node runs the result. llvm-mc and wasm-ld have to come from one LLVM release,
# which they do here by both being MSYS2 packages: a relocation the assembler emits is
# only useful if the linker that consumes it understands the same numbering. wabt is
# here for wasm-objdump, which no probe runs but every investigation of one does, and
# which is how wasm_dialect_probe.py asserts its subjects' object facts.
RUN_TOOLS = ("mingw-w64-x86_64-wabt", "mingw-w64-x86_64-nodejs",
             "mingw-w64-x86_64-lld", "mingw-w64-x86_64-llvm")

# The triple handed to llvm-mc, which is not the one GCC is configured for: LLVM calls
# the OS-less wasm target `unknown', GCC's config.sub canonicalises it to `none'. The
# two name the same target; only the spelling differs, and ASM_SPEC carries this one.
ASM_TRIPLE = "wasm32-unknown-unknown"

# The instruction-set features the assembler has to be told to accept, and the reason
# this is a constant rather than a literal at the one call site: it has to match the
# `-mattr' in the backend's own ASM_SPEC, because the driver assembles libgcc and
# libstdc++ through that spec while this script assembles every probe by hand. Two
# spellings of the same list is a probe table measuring a different assembler from the
# one the engine's objects are built with -- the same measurement bug as the
# ENGINE_CXXFLAGS one, which step 18 paid for in ten misattributed red rows.
#
# `check_asm_features' enforces the match against wasm.h rather than trusting this
# comment, because a constant that merely claims to agree with a patch is how the
# first version of this got it wrong: the probes assembled with no -mattr at all and
# passed, since no probe's own translation unit emitted an atomic. The ones that
# would have failed are the next feature-gated family's.
#
# Both features, not one: see 0015. wasm-ld refuses thread-local storage unless the
# object declares atomics AND bulk-memory, so they are enabled as a pair.
ASM_FEATURES = "+atomics,+bulk-memory"

CXXFLAGS = "-std=c++26 -fno-exceptions -fno-rtti"

# What wasi-libc demands of any C++ translation unit that reaches far enough into it, and
# so of every probe that uses the C++ runtime. It is already in CXXFLAGS_FOR_TARGET for
# the library build; this is the same statement for the compiles that happen outside it:
# wasi-libc's signal.h is an #error without it, reached by <csignal>, and the matching
# -lwasi-emulated-signal is already on the hosted link line.
#
# `-D_Thread_local=__thread' is here beside it again, and on the threads variant it is
# load-bearing rather than the inert leftover step 17 left it as. `_Thread_local' is C11
# and not a C++ keyword, so without the define the `extern _Thread_local int errno' that
# <cerrno> reaches does not parse as thread-local at all -- and on this variant it has
# to, because libc.a really does define errno as a TLS symbol. Step 17's objection to
# the define was to mapping it onto a target with no TLS; the target has TLS now.
CXX_RUNTIME_DEFINES = "-D_WASI_EMULATED_SIGNAL -D_Thread_local=__thread"

# Every run probe exports this and returns an int, so the harness needs no per-probe
# configuration.
ENTRY = "entry"

# The linear memory the harness instantiates, in wasm pages. Stated here rather than in
# the harness text because the *link* has to agree with it: a shared memory must declare
# a maximum and wasm-ld takes --max-memory in bytes, so both sides read these.
#
# The maximum is a ceiling and not a reservation -- memory.grow is what allocates -- so
# there is one number here rather than a probe's and an engine's. 16 pages was 1MB, which
# was the whole of what a probe ever needed and is less than the engine's own module; a
# run of `scene_reset_cpu` has to fit the heap a world of rigid bodies allocates, so this
# is the first value chosen for the engine rather than for a probe. Every probe links and
# instantiates against the same ceiling and none of them grows into it.
#
# The INITIAL is a floor the module sets rather than a choice the harness makes: an
# imported memory carries the minimum the link derived from the engine's static data, its
# stack and __heap_base, and instantiation refuses a memory smaller than that. 2 pages
# served every probe and is 75x too small for the Sandbox link, which declares 152. This
# is that number with headroom, and both ways of being wrong are loud rather than silent
# -- too small and WebAssembly.instantiate names the declared initial it wanted, too
# large and a probe commits a few MB it never touches.
PAGE_SIZE = 65536
HARNESS_INITIAL_PAGES = 256
HARNESS_MAX_PAGES = 8192

# The wasm stack, which is linear memory rather than a host stack and is 64KB by default
# -- enough for every probe and not for a frame of this engine, whose update tier nests
# systems, coroutine frames and the solver's own recursion. Passed to wasm-ld only on the
# arm that links the engine: a probe measuring a construct has nothing to gain from a
# bigger stack, and changing what every probe links would make this a new input to rows
# that are already green.
ENGINE_STACK_SIZE = 8 * 1024 * 1024

# How long a scenario run is given before it is killed. `scene_reset_cpu` is 240 fixed
# steps of a headless world, which is seconds natively; this is generous against an
# interpreter's first run and still bounded, because the failure a first run can produce
# that no earlier stage could is to block forever with nothing on the console.
SCENARIO_TIMEOUT_SECONDS = 900

# The import object is derived from what the module declares rather than written out.
# A linked module normally imports nothing at all -- wasm-ld defines the memory, the
# stack pointer and the table itself -- but a function the backend could not resolve
# stays an import, and a hardcoded `env` would be a second statement of a fact the
# module already carries. Imported functions are supplied as the identity so that an
# unresolved callee shows up as a wrong result rather than as a failure to instantiate.
HARNESS = """
import { readFileSync, readdirSync, writeSync } from "node:fs";
import { WASI } from "node:wasi";
import { Worker, isMainThread, workerData } from "node:worker_threads";

// The one thing the module does NOT say about itself, so the one thing passed in. A
// shared memory and a plain one are not interchangeable -- instantiating against the
// wrong kind fails with a flag mismatch -- and `WebAssembly.Module.imports' reports a
// memory import's name and kind but never its sharedness. Guessing shared and
// retrying plain was the alternative, and it was rejected: it would turn a genuine
// instantiation failure into a second, misleading error from the retry, which is
// precisely the confusion the three-way exit taxonomy below exists to prevent.
const SHARED = process.argv[4] === "shared";

// A host directory to preopen, or nothing. WASI has no ambient filesystem: a module can
// only reach what the embedder hands it, so without this every `open' in the subject
// fails with a capability error rather than with a missing file. No probe passes it and
// their invocation is unchanged; the engine run does, because a scenario names a scene
// and the asset tree is on disk here. Spelled as one preopen at `/' rather than a map of
// several, so that what the subject could not reach is a path this harness can be
// matched against rather than a list it has to have guessed right.
const ROOT = process.argv[5];

// The subject's environment, as JSON, or nothing. WASI has no ambient environment either:
// `getenv' reads what the embedder passed and nothing else, so a module whose
// configuration arrives that way sees an empty environment unless this is supplied. No
// probe passes it and their invocation is unchanged; the engine run does, because
// `gse.config' resolves the executable's path and the writable roots from it rather than
// from a path this harness would otherwise have had to be told twice.
const ENV = JSON.parse (process.argv[6] ?? "{}");

// One constructor for both arms, because a spawned thread does the same syscalls: the
// engine loads assets on job threads, and a worker whose WASI had no preopen would fail
// an open the main thread can do -- a difference between the two paths that would read as
// a fact about the target. The environment travels with it for the same reason: a thread
// that resolved a different root than the thread that spawned it would be a divergence
// this harness invented.
function hosting (root, env) {
  return new WASI ({ version: "preview1", env,
                     ...(root ? { preopens: { "/": root } } : {}) });
}

// A shared memory must carry a maximum, and these are the numbers the link has to
// agree with, so they are substituted in from the one place that states them rather
// than written twice.
const INITIAL_PAGES = %(initial_pages)d, MAX_PAGES = %(max_pages)d;

function memory () {
  return SHARED
    ? new WebAssembly.Memory ({ initial: INITIAL_PAGES, maximum: MAX_PAGES, shared: true })
    : new WebAssembly.Memory ({ initial: INITIAL_PAGES });
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

// node's own WASI answers `fd_readdir' with ENOSYS, measured directly: `path_open' with
// O_DIRECTORY returns a descriptor, `fd_filestat_get' reports a directory and `fcntl'
// works on it, and then the one call that would enumerate it is not implemented. Nothing
// in the subject can route around that -- `opendir', `readdir' and every
// `std::filesystem::directory_iterator' above them are that syscall -- so a directory
// walk is the host's gap to close, and this is the host.
//
// The descriptors have to be tracked because node's WASI does not say which directory an
// fd denotes: `fd_prestat_dir_name' names the preopens, `path_open' derives every other
// descriptor from one of those, and both are wrapped here so the mapping is built from
// the calls the subject actually makes rather than guessed from a convention.
//
// Returned entries are the directory's own: `.' and `..' are omitted, which readdir's
// callers already skip, and `d_ino' is an index because a browser has no inode to report
// and nothing above this reads it. Truncation is the protocol's own -- a short write is
// how libc learns to come back with the next cookie -- so a buffer too small for an entry
// simply stops the fill.
function filesystem (memory, root, wasi) {
  const wrapped = { ...wasi.getImportObject ().wasi_snapshot_preview1 };
  const paths = new Map ();
  // Resolved per call and through a function, because which memory the module ended up
  // with is not known when this object is built: a link that imports one uses the
  // harness's, and a link that does not exports its own. Reading the wrong one is
  // silent -- it decodes whatever happens to be at the offset -- so it is worth not
  // deciding early.
  const text = (ptr, len) =>
    new TextDecoder ().decode (new Uint8Array (memory ().buffer, ptr, len));

  const join = (base, relative) => {
    const parts = (relative.startsWith ("/") ? relative : base + "/" + relative).split ("/");
    const out = [];
    for (const part of parts) {
      if (part === "" || part === ".") continue;
      if (part === "..") out.pop (); else out.push (part);
    }
    return "/" + out.join ("/");
  };

  const prestat = wrapped.fd_prestat_dir_name;
  wrapped.fd_prestat_dir_name = (fd, ptr, len) => {
    const code = prestat (fd, ptr, len);
    if (code === 0) paths.set (fd, join ("/", text (ptr, len)));
    return code;
  };

  const open = wrapped.path_open;
  wrapped.path_open = (fd, dirflags, ptr, len, ...rest) => {
    const relative = text (ptr, len);
    const code = open (fd, dirflags, ptr, len, ...rest);
    if (code === 0 && paths.has (fd)) {
      const opened = new DataView (memory ().buffer).getUint32 (rest[rest.length - 1], true);
      paths.set (opened, join (paths.get (fd), relative));
    }
    return code;
  };

  const close = wrapped.fd_close;
  wrapped.fd_close = (fd) => { paths.delete (fd); return close (fd); };

  // ENOSYS, EBADF and ENOTDIR are 52, 8 and 54 in preview1's own numbering, which is not
  // errno's: the module compares against what wasi-libc's errno.h says, so these are the
  // values that reach it as ENOSYS, EBADF and ENOTDIR.
  wrapped.fd_readdir = (fd, buf, len, cookie, used) => {
    const guest = paths.get (fd);
    if (guest === undefined) return 8;
    let entries;
    try {
      entries = readdirSync (join_host (root, guest), { withFileTypes: true });
    } catch (e) {
      return e.code === "ENOTDIR" ? 54 : 52;
    }
    const view = new DataView (memory ().buffer);
    const bytes = new Uint8Array (memory ().buffer);
    let at = 0;
    for (let i = Number (cookie); i < entries.length; ++i) {
      const name = new TextEncoder ().encode (entries[i].name);
      if (at + 24 + name.length > len) break;
      view.setBigUint64 (buf + at, BigInt (i + 1), true);
      view.setBigUint64 (buf + at + 8, BigInt (i + 1), true);
      view.setUint32 (buf + at + 16, name.length, true);
      view.setUint8 (buf + at + 20, entries[i].isDirectory () ? 3
                                    : entries[i].isSymbolicLink () ? 7 : 4);
      bytes.set (name, buf + at + 24);
      at += 24 + name.length;
    }
    view.setUint32 (used, at, true);
    return 0;
  };
  return { wasi_snapshot_preview1: wrapped };
}

// Node's path join, spelled here rather than imported, because the guest side of the
// mapping is always POSIX while the host side is whatever this machine uses.
function join_host (root, guest) {
  return root.replace (/[\\/]+$/, "") + guest;
}

// wasi-threads: the host supplies `wasi.thread-spawn', and all it owes the module is a
// fresh instance over the SAME memory with `wasi_thread_start (tid, arg)' called on it.
// The thread's stack and its descriptor are wasi-libc's own, allocated in linear memory
// by pthread_create and reached through `arg', which is why the host does not have to
// know anything about either.
//
// node:wasi is instantiated per thread rather than shared: it binds to one instance's
// exports. `unref' is what lets the process exit once the main thread is done -- a
// joined thread has already returned, and an unjoined one must not hold the run alive.
//
// `ids' is a shared counter rather than one per spawner, because a thread may spawn in
// turn and wasi-threads requires thread ids to be distinct: a per-spawner counter would
// hand a worker's child the same id the main thread had already used. It starts at 1 so
// the first allocated id is 2, leaving 1 for the initial thread.
function spawner (module, mem, ids, root, env) {
  return (arg) => {
    const tid = Atomics.add (ids, 0, 1) + 1;
    const worker = new Worker (new URL (import.meta.url),
                               { workerData: { module, mem, ids, tid, arg, root, env } });
    worker.unref ();
    trace_thread (`thread ${tid} spawned`);
    return tid;
  };
}

// `writeSync' on the descriptor and not `console.log', which is the whole reason thread
// lifetime was unobservable here. A worker's `console.log' is streamed to the parent and
// delivered by the parent's event loop, and an `error' event on the Worker needs that
// loop too -- but a subject that spins on its own job queue never returns to it, so both
// arrive never. The engine's own output was visible throughout only because WASI's
// `fd_write' is a synchronous write to this same descriptor. Unbuffered and interleaved
// with the subject's stdout on purpose: what is wanted is the order the two happened in.
function trace_thread (text) {
  writeSync (1, text + "\\n");
}

// A spawned thread is handed the module and the memory rather than only its arguments,
// so it can spawn in turn: a thread that creates a thread is ordinary in the engine's
// job system, and a worker that answered -1 here would have measured a restriction this
// harness invented rather than one the target has.
if (!isMainThread) {
  const { module, mem, ids, tid, arg, root, env } = workerData;
  const wasi = hosting (root, env);
  // The same import object as the main arm, derived the same way. Spelling it
  // `env: { memory: mem }' here instead worked only because nothing yet imports
  // anything else from env, and would have failed as the unresolved import of a
  // module this harness can otherwise run -- a divergence between two paths that
  // must agree, which is the shape worth not leaving behind.
  // Declared before the import object rather than beside its assignment: the
  // filesystem wrapper closes over it to find the module's memory, and a `let' read
  // before its declaration is a reference error rather than an undefined.
  let instance;
  const imports = {
    ...supply (module, mem),
    ...filesystem (() => instance?.exports?.memory ?? mem, root, wasi),
    wasi: { "thread-spawn": spawner (module, mem, ids, root, env) },
  };
  // Reported rather than left to an unhandled rejection in an unref'd worker, which
  // is where this surfaced as silence: the main thread then blocks in pthread_join
  // on a thread that never started.
  try {
    instance = await WebAssembly.instantiate (module, imports);
  } catch (e) {
    trace_thread (`thread ${tid} did not instantiate: ${e.message}`);
    process.exit (%(no_instance)d);
  }
  // node:wasi gates every syscall on having been started, and without this a spawned
  // thread threw `wasi.start() has not been called' out of its first one -- whichever
  // it happened to reach, which here was `clock_time_get' under `trace::end_async'
  // inside the first job the thread ran. It cannot be `start' or a plain `initialize'
  // over this instance: both call the module's entry, re-running libc setup and every
  // global constructor over a program already live in this shared memory. The thread's
  // entry is `wasi_thread_start' and libc does its per-thread setup there, so what is
  // owed here is the binding and not the start: `initialize' over an object carrying
  // only the memory marks this WASI live and runs no guest code, because it calls
  // `_initialize' only if the object it is handed has one.
  wasi.initialize ({ exports: { memory: instance.exports.memory ?? mem } });
  // A trap on a spawned thread used to be silence: this is an unref'd worker with no
  // `error' listener, so the rejection had nowhere to surface, and a thread that died
  // inside its first job was indistinguishable from one still working on it.
  try {
    instance.exports.wasi_thread_start (tid, arg);
  } catch (e) {
    trace_thread (`thread ${tid} trapped: ${e.message}\n${e.stack}`);
    process.exit (%(trap)d);
  }
  trace_thread (`thread ${tid} returned`);
  // Ends the worker, and it is not optional: this is top-level module code, so
  // without it a spawned thread falls through into the main-thread path below and
  // instantiates the module a second time as though it were the program.
  process.exit (0);
}

// A module linked against wasi-libc imports the WASI preview1 syscalls, which node
// hosts itself -- the identity functions `supply` hands out would let it instantiate
// and then misbehave in whatever way a stubbed write or clock produces. Which of the
// two it is comes from the module's own imports rather than from a flag, for the same
// reason `supply` is derived rather than written: the module already says.
let instance, wasi;
try {
  const module = new WebAssembly.Module (readFileSync (process.argv[2]));
  const mem = memory ();
  // Set, because a SharedArrayBuffer is zero-filled and the comment above `spawner' is
  // the contract: the initial thread holds tid 1, so the first one handed out has to be
  // 2. Left at zero it handed the first spawned thread the initial thread's own id, and
  // wasi-threads requires them distinct.
  const ids = new Int32Array (new SharedArrayBuffer (4));
  ids[0] = 1;
  wasi = WebAssembly.Module.imports (module).some (i => i.module.startsWith ("wasi_"))
         ? hosting (ROOT, ENV) : null;
  // `supply' first and the real implementations over it, namespace by namespace: it
  // derives a stub for every import including the WASI syscalls and `thread-spawn',
  // and an identity function in either of those places would instantiate and then
  // misbehave rather than fail. Spreading replaces a whole namespace object, so
  // `env' and the table survive from `supply' while wasi_snapshot_preview1 and
  // `wasi' come from node and from the spawner.
  instance = await WebAssembly.instantiate (module, {
    ...supply (module, mem),
    ...(wasi ? filesystem (() => instance?.exports?.memory ?? mem, ROOT, wasi) : {}),
    wasi: { "thread-spawn": spawner (module, mem, ids, ROOT, ENV) },
  });
} catch (e) {
  console.log(`did not instantiate: ${e.message}`);
  process.exit(%(no_instance)d);
}

const want = Number(process.argv[3]);
let got;
try {
  // Static constructors do not run on their own. The linker collects every object's
  // init-array entries into one function and leaves calling it to whatever embeds the
  // module, which here is this harness. The two calls are alternatives, not a
  // sequence: a reactor's _initialize calls __wasm_call_ctors itself after setting the
  // libc up, so calling both would run every constructor twice.
  if (wasi)
    wasi.initialize (instance);
  else
    instance.exports.__wasm_call_ctors?.();
  got = instance.exports.%(entry)s();
  // What a wasi COMMAND's `_start' would have done after main returned, and this is a
  // reactor: it runs the atexit handlers, which is where libc's stdio exit flushes the
  // buffers. Without it a subject that printed its result and returned 0 leaves the
  // output in linear memory -- and stdout here is a pipe, so it is fully buffered rather
  // than line buffered. Optional because the linker synthesises it only where some
  // object registered a destructor, the same reason the exports below are if-defined.
  instance.exports.__wasm_call_dtors?.();
} catch (e) {
  // The stack as well as the message, because a trap's message names the kind of fault
  // and never the site: `null function or function signature mismatch' is the same
  // sentence wherever in half a million lines of subject it happened. V8 renders wasm
  // frames as `wasm-function[N]', which `llvm-nm' over the objects maps back to a
  // symbol -- so this is what turns a trap from a fact into an address.
  console.log(`trapped: ${e.message}\n${e.stack}`);
  process.exit(%(trap)d);
}
console.log(got === want ? `ok ${got}` : `got ${got} want ${want}`);
process.exit(got === want ? 0 : 1);
"""

# A module can fail to instantiate, trap, or return the wrong number, and the three say
# different things about the backend — a validation failure is the shape an unresolved
# global takes, which is exactly what this harness exists to tell apart from bad codegen.
# Node exits 1 on an unhandled rejection too, so each case needs its own code to be
# distinguishable at all.
TRAP_EXIT = 2
NO_INSTANCE_EXIT = 3


def harness_text() -> str:
    """The node harness, substituted. Both pipelines write this same file.

    A function rather than a formatted constant because the substitution had been
    spelled out at each of the two call sites, and the page counts threads added would
    have been a third and fourth chance for the dialect probe's harness to differ from
    this one's -- which is the single thing the two scripts must not do, since a result
    here and a result there are reported as though they mean the same.
    """
    return (HARNESS % {"entry": ENTRY, "trap": TRAP_EXIT,
                       "no_instance": NO_INSTANCE_EXIT,
                       "initial_pages": HARNESS_INITIAL_PAGES,
                       "max_pages": HARNESS_MAX_PAGES}).lstrip()


# COMPILE stops at the compiler, which is all the frontend questions need. LINKED goes
# all the way to a number out of node.
COMPILE, LINKED = "compile", "linked"


class Blocked(NamedTuple):
    """A known-red (probe, mode), and the exact status it is known to be red with.

    The status is as load-bearing as the reason: a blocked probe that fails some *other*
    way is a new regression wearing a known gap's clothes, and excusing it would turn the
    probe from a tripwire into a blindfold. A blocked probe that passes is reported louder
    still.
    """
    mode: str
    status: str
    reason: str


class Probe(NamedTuple):
    """A C++ translation unit and how far it is expected to get.

    `expect` is the integer `entry` must return once the module runs; probes that leave
    it None are compile-only. `second` is a second translation unit linked with the
    first, for the one question a single TU cannot ask.

    `sources` names real repo files, relative to the repo root, compiled in the order
    given and ahead of `body`. It is how the engine's own code becomes a probe: a
    module interface unit has to be compiled before anything that imports it, and that
    order is a fact about the engine rather than about this script, so it is written
    out here rather than derived. A probe may have `sources` and a `body` both -- the
    sources bring a module up and the body is the translation unit that uses it.

    `hosted` says the probe needs the target's C runtime rather than only the
    compiler: the sysroot's headers on the way in, and wasi-libc on the link line.
    `cxx_runtime` adds libstdc++ and libsupc++ to that. They are separate, and off by
    default, for the reason the probe table exists at all -- a probe that does not need
    a libc should not be able to fail because of one, and a probe that needs only a
    libc should not be able to fail because libstdc++ is missing.

    `std_module` says the `std` module has to be compiled before this probe can import
    it. GCC builds no module implicitly and libstdc++ installs `std.cc` as a *source*
    file, so `import std;` without this is an error about a missing `.gcm` rather than
    anything about the target.
    """
    name: str
    flags: str
    body: str
    expect: Optional[int] = None
    blocked: tuple = ()
    second: str = ""
    sources: tuple = ()
    # A module whose derived closure, or whose partitions, are this probe's sources.
    # Resolved when the probe runs rather than when this table is built, because the
    # graph is read out of a configured native build directory and nothing else in this
    # script needs one -- see `graph`.
    closure_of: str = ""
    partitions_of: str = ""
    hosted: bool = False
    cxx_runtime: bool = False
    std_module: bool = False
    # `threads` says the probe needs the target's threads: the pthread archive on the
    # link line, a shared linear memory, and a harness that services wasi.thread-spawn.
    # It is one field rather than three because the three are not independently useful
    # -- wasm-ld refuses thread-local storage unless the atomics and bulk-memory
    # features are present, and refuses a shared memory without a maximum, so a probe
    # asking for any part of this needs all of it. Measured in step 19a, where a
    # hand-written subject with TLS and no features was rejected outright.
    threads: bool = False


# The engine's own modules, derived. These used to be four hand-written lists, with a
# comment arguing that the order "is a fact about the engine rather than about this
# script, so it is written out here rather than derived" -- true, and the reason they
# went: `module_graph.py` derives the same fact from the files and from CMake's own
# dependency scan, and a sweep over all 425 interface units cannot have a hand-written
# list behind it. The lists were kept as the oracle for exactly one run: all three
# closures below came out equal as sets to what was recorded, and the derived order
# topologically valid, before they were deleted.
#
# `gse.meta` is self-contained on `std` and `gse.assert`'s interface is too, so the
# closures nest: gse.math's 36 units are gse.meta's 16 plus gse.assert's 1 plus its own
# 19. That is not asserted here -- it is what the graph says, and a change to the
# engine's imports is allowed to change it.
#
# Lazily, and that matters: the graph is read out of a configured native build
# directory, which a clean checkout does not have. Scanning at import time made that
# directory a prerequisite of the whole script -- `--help` and all seventeen steps of
# building the cross compiler, none of which have anything to do with the engine's
# modules. Only the measurement needs it, so only the measurement asks for it.
_GRAPH = None


def graph() -> module_graph.Graph:
    """The engine's module graph, scanned once per run and only if something asks."""
    global _GRAPH
    if _GRAPH is None:
        _GRAPH = module_graph.scan()
    return _GRAPH

# What the engine is built with, minus the two things this target cannot honour:
# -mavx2, which is why `SIMD.cppm` now has a portable arm, and exceptions, which are
# off because the std module and libstdc++ here were built that way and because a
# throw aborts on this target regardless.
#
# The two -W options are `Engine/cmake/GSEEngine.cmake`'s, and leaving them out was a
# measurement bug the first sweep found: `-Wchanges-meaning` is an error by default, the
# engine turns it off in both spellings, and the engine's own code relies on that -- a
# member named for the type it holds (`contact_point::separation`, `request::target`,
# `buffer::handle()`) is the shape it fires on. Without them ten units of gse.graphics,
# gse.physics, gse.gpu_backend and gse.runtime failed here while compiling natively, which
# is this script's own include-path hazard wearing different clothes: a red that looks
# like the target and is really a flag the engine passes and the harness did not. Any flag
# the engine relies on belongs here or the sweep measures a compiler the engine never
# uses.
ENGINE_CXXFLAGS = (CXXFLAGS + " -fmodules -freflection -fcontracts"
                   " -Wno-changes-meaning -Wno-error=changes-meaning")

# The engine's own include directory, `Engine/CMakeLists.txt`'s one
# target_include_directories entry, and the same hazard the paragraph above records. No
# compile here needed it until now: a wrapper under Source/External was the only kind of
# unit that #includes a vendored header, and every one of them was short-circuited into a
# host-bound row before the compiler saw it. `gse.moodycamel` is the first that is
# compiled, and without this flag its row would read FAILED on a header the engine finds
# -- this harness's include-path hazard again, wearing the target's clothes.
#
# Handed out by `target_cxxflags` and not appended beside ENGINE_CXXFLAGS at a call site,
# which is the whole point: the probe table and the sweep both read their include flags
# from that one function, and an engine probe whose closure reaches moodycamel --
# `closure_of="gse.time"` is the obvious next one -- has to be compiled with what the
# sweep used or the two measure different compilers.
ENGINE_INCLUDE_DIR = REPO_ROOT / "Engine" / "External"

# What `--link-engine` links when its command line names nothing, and the one judgment in
# this file about the browser port rather than about the compiler.
#
# `gse` is the umbrella the Sandbox imports and `gse.scenario` is the runner that names a
# scenario, so between them they are the whole engine a headless scenario needs. They are
# NOT that scenario: `scene_reset_cpu` is declared in `sandbox:scenarios`, under Sandbox/,
# which neither this script nor module_graph.py walks -- reaching it means enumerating a
# second tree with its own scenes and systems. Every `.cpp`-level gap this port has been
# unable to see is in the engine, so the engine is what the first link measures, and the
# Sandbox is a later one rather than a thing quietly folded in here.
LINK_SUBJECT = ("gse", "gse.scenario")

# And what `--run-scenario` links, which is the paragraph above answered: the Sandbox is
# a second tree with its own scenes and systems, `module_graph.py`'s roots now reach it,
# and these are the modules that have to be among the link's roots for its program unit to
# compile. `Main.cpp` imports eight modules and three of them -- `gse.config`, `gse.test`,
# `gse.tests` -- are reached by nothing in the engine's own closure, so a subject that
# named only `gse` and `sandbox` would compile that file against whatever BMI the shared
# module cache still held. That is the artifact `link_closure`'s docstring records, and on
# this arm it arrives as a rejection of the one unit the whole run exists to call.
# `gse.server` is on the list for the reason `link_roots` names PORTED_HOST_ROOTS
# unconditionally: `.cpp` files contribute no edges to the module graph, and the only
# importer of that module in the tree is `Sandbox/Source/NetSetup.cpp`. No fixpoint over
# interface imports can therefore reach it, so without naming it here its BMI is never
# built and the unit defining `server_setup` -- which `Main.cpp` references whether or not
# a headless scenario takes the dedicated-server branch -- is rejected.
SCENARIO_SUBJECT = ("gse", "gse.scenario", "gse.config", "gse.system_manifest",
                    "gse.test", "gse.tests", "gse.server", "sandbox")

# The program, named here because no module names it. `Main.cpp` carries no module
# declaration, so it is in `undeclared_impls` -- the list this arm already prints as a
# fact about every link -- and a walk keyed on that declaration cannot place it. One
# constant is the honest form: the alternative is a rule for guessing which module a
# nameless `.cpp` belongs to, which would be a second enumerator disagreeing with CMake
# about what a build contains.
PROGRAM_UNIT = "Sandbox/Sandbox/Source/Main.cpp"

# The same program as the native build produces it, which is what the run reports to
# `gse.config` as its executable. Not the `.wasm` the link writes: that file is beside the
# compiler, outside the preopen, and its stem would name the engine's log file and its
# user-state directories something no native run has ever used. See
# `scenario_environment`.
NATIVE_PROGRAM = "Sandbox.exe"

# The harness entry, and all it does is reach the program's own `main`.
#
# Assembling the bench configuration here instead was the alternative and is the one thing
# this arm must not do. `Main.cpp`'s `apply_scenario` is what turns a scenario name into a
# scene, a frame count, a warmup count and the settings the annotation pins -- that
# configuration IS the world whose hash the parity criteria compare -- so a second copy of
# it drifting by one setting would produce a hash with nothing to say and no way to tell.
# Calling `main` is the whole point rather than a shortcut around writing more.
#
# `__main_argc_argv` and not `main`, which the first run of this arm measured: GCC's wasm
# backend renames a `main(int, char**)` definition to the wasi-libc entry name, the same
# convention clang follows, so `main` is a symbol no object in the link defines. Declaring
# it is also what makes this call well-formed -- a program may not use `main` itself -- and
# it is the entry point `crt1.o`'s `__main_void` would have called in the command form.
#
# `--no-engine-gpu` is the one flag this arm adds, and it is a configuration the engine
# already has rather than a wasm special case: `engine_config::gpu` is a first-class field
# and `Runtime/Engine.cpp` drops `gpu::context::data` through the same `resolve_activation`
# that already drops the window and the audio root for a headless scenario. A wasm build
# compiles no GPU backend, so `create_device` has nothing to return; without this the run
# reaches `gpu::context::init` and dereferences the null it was honestly handed.
#
# Spelled by `gse::parse_args`'s reflective derivation -- `config::engine` is the group,
# `engine_config::gpu` the bool, `_` becomes `-` -- the same derivation that produces the
# `--engine-bench-scenario` beside it. It is NOT one of the flags `apply_scenario` owns, so
# a scenario run accepts it; the owned set is window/render/solver/encode/settings/assets.
# The extra arguments are read from the environment rather than substituted in beside
# the two above, and that is the difference between a diagnostic flag costing nothing
# and costing a relink. The entry is a generated translation unit, so every argument
# baked into it is an input to the *link* -- the ~25 minutes this arm takes -- and the
# last several measurements in this port each spent that to add one flag to a module
# that was otherwise identical. `GSE_RUN_ARGS` is read through the same WASI
# environment `scenario_environment` already fills and the harness already forwards to
# the main and worker arms, so there is nothing new between the argument and `main`.
#
# Split on spaces, which is what a `getenv` string can carry without this file growing
# a quoting convention the shell would then have to agree with. Every flag the engine
# derives is one token or two, and a value with a space in it is a run this cannot
# express -- loudly, since the split produces a flag `parse_args` then rejects by name,
# rather than quietly.
#
# ARG_LIMIT bounds the array because `main`'s argv has to be one, and a fixed small
# ceiling is honest where a heap allocation before libc's own argv exists would not be.
SCENARIO_ENTRY = """
#include <string.h>
#include <stdlib.h>

extern "C" int __main_argc_argv(int argc, char** argv);

extern "C" int entry() {
    static const int arg_limit = 64;
    static char* args[arg_limit] = {
        const_cast<char*>("sandbox"),
        const_cast<char*>("--no-engine-gpu"),
        const_cast<char*>("--engine-bench-scenario"),
        const_cast<char*>("%(scenario)s"),
    };
    int argc = 4;

    static char extra[4096];
    if (const char* from_env = getenv("GSE_RUN_ARGS"); from_env != nullptr) {
        strncpy(extra, from_env, sizeof(extra) - 1);
        for (char* token = strtok(extra, " "); token != nullptr && argc < arg_limit;
             token = strtok(nullptr, " ")) {
            args[argc++] = token;
        }
    }

    return __main_argc_argv(argc, args);
}
"""

PROBES = (
    Probe("baseline", CXXFLAGS, """
struct point { int x, y; };
constexpr int add(int a, int b) { return a + b; }
static point p{add(1, 2), 4};
int sum() { return p.x + p.y; }
"""),
    # Core-language reflection only: there is no <meta> to include on a freestanding target.
    Probe("reflection", CXXFLAGS + " -freflection", """
struct point { int x, y; };
constexpr auto r = ^^point;
using alias = typename [:r:];
constexpr int sum() { alias p{1, 2}; return p.x + p.y; }
static_assert (sum () == 3);
int entry() { return sum (); }
"""),
    Probe("contracts", CXXFLAGS + " -fcontracts", """
int halve(int n) pre (n % 2 == 0) { return n / 2; }
int call() { return halve(4); }
"""),
    # Arithmetic through a loop and a branch: the one probe that proves the br_table
    # dispatch loop the backend emits for every CFG actually computes the right answer.
    Probe("arith", CXXFLAGS, """
extern "C" int entry ()
{
  int acc = 0;
  for (int i = 1; i <= 10; ++i)
    acc += (i % 3 == 0) ? i * 2 : i;
  return acc;
}
""", expect=73),
    # Varargs: nvptx's recipe, an implicit pointer to the outgoing-args block in linear
    # memory. Nothing here needs a relocation, so it runs without a linker.
    Probe("varargs", CXXFLAGS, """
#include <stdarg.h>

extern "C" int total (int count, ...)
{
  va_list ap;
  va_start (ap, count);
  int acc = 0;
  for (int i = 0; i < count; ++i)
    acc += va_arg (ap, int);
  va_end (ap);
  return acc;
}

extern "C" int entry () { return total (4, 10, 20, 30, 40); }
""", expect=100),
    # An internal-linkage callee, which is what almost all real code looks like and what
    # nothing else here covers: baseline, contracts and reflection carry no `expect`, so
    # they are never taken through the linked pipeline. It is also the second probe of a
    # cross-function direct call, so a lost `(@reloc)` on `call` shows up here too.
    Probe("local_linkage", CXXFLAGS, """
static int scale (int n) { return n * 3; }

extern "C" int entry () { return scale (14) + 7; }
""", expect=49),
    # Two call targets, so a function pointer that resolved to the wrong index cannot
    # pass by landing on the right function anyway. This is one of the two rows the move
    # to llvm-mc exists to turn green: a function address in data is a TABLE_INDEX
    # relocation, which the annotated dialect had no way to ask for.
    Probe("funcptr", CXXFLAGS, """
extern "C" int twice (int n) { return n * 2; }
extern "C" int thrice (int n) { return n * 3; }

extern "C" int entry ()
{
  int (*fns[2]) (int) = { twice, thrice };
  int acc = 0;
  for (int i = 0; i < 2; ++i)
    acc += fns[i] (7);
  return acc;
}
""", expect=35),
    # Two statics, the other row the dialect move exists to turn green: each is a data
    # symbol in its own section with a MEMORY_ADDR relocation naming it, where the
    # annotated dialect emitted both at (i32.const 0) and had no data symbol to place
    # them by, so they overlapped and `entry` returned 2002.
    Probe("static", CXXFLAGS, """
int lo = 40;
int hi = 2;

extern "C" int entry () { return lo + hi * 1000; }
""", expect=2040),
    # A varargs callee this unit only declares, which is the case where a signature
    # derived from the type and a call site derived from the registers can disagree
    # without anything noticing: the hidden trailing pointer to the outgoing-args
    # block is a parameter the argument list does not mention. `varargs` cannot catch
    # it, because there the definition is in the same TU and its signature wins.
    Probe("extern_varargs", CXXFLAGS, """
extern "C" int sum_all (int count, ...);

extern "C" int entry () { return sum_all (3, 11, 22, 33); }
""", expect=66, second="""
#include <stdarg.h>

extern "C" int sum_all (int count, ...)
{
  va_list ap;
  va_start (ap, count);
  int acc = 0;
  for (int i = 0; i < count; ++i)
    acc += va_arg (ap, int);
  va_end (ap);
  return acc;
}
"""),
    # A result too large to come back in the return local, which is the other half of
    # the same question: the caller passes a hidden leading pointer and the callee
    # writes through it, so the function declares no return local and must not emit a
    # `return` that reads one. Nothing else here returns anything but an int.
    Probe("sret", CXXFLAGS, """
struct big { int a[8]; };

extern "C" big make (int seed)
{
  big out;
  for (int i = 0; i < 8; ++i)
    out.a[i] = seed + i;
  return out;
}

extern "C" int entry ()
{
  big b = make (5);
  int acc = 0;
  for (int i = 0; i < 8; ++i)
    acc += b.a[i];
  return acc;
}
""", expect=68),
    # A static constructor, which nothing measured until now and which upstream marked
    # with an annotation no assembler read, so constructors silently did not run. The
    # initialiser has to be something the frontend cannot fold into the object's
    # initial bytes, or the probe passes whether or not a constructor ever ran — hence
    # the volatile source for the value.
    #
    # Hosted libstdc++ is what makes this stop being a footnote: ios_base::Init and the
    # locale tables are static constructors, so a `std` module without this does not
    # work and does not say so.
    Probe("init_array", CXXFLAGS, """
static volatile int seed = 7;

struct counter
{
  int value;
  counter () : value (seed * 6) {}
};

static counter c;

extern "C" int entry () { return c.value; }
""", expect=42),
    # A libcall: a call the compiler writes and the probe does not, resolved out of
    # libgcc rather than out of either translation unit. It is the only probe that can
    # say whether libgcc was built at all, and the only one that would notice it
    # disappearing again.
    #
    # Finding one took measuring rather than guessing, because wasm has far more
    # instructions than the usual soft-float target: i64 divide, modulo, shifts and
    # every float conversion are single opcodes, `__int128` is rejected outright on a
    # 32-bit target, and `long double` is `double` here, so none of the shapes that
    # would call libgcc anywhere else do so on wasm. `__builtin_powi` does — it is
    # `__powidf2`, which no ISA has.
    #
    # It therefore also measures something no other probe can: a libgcc signature is
    # derived from a GCC-internal builtin declaration rather than from a C++ one the
    # probe wrote, and the emitted `.functype` is `(i32, f64) -> (f64)` while libgcc
    # defines `__powidf2 (double, int)`. If that ordering is the emitter's and not just
    # a spelling, this probe returns the wrong number.
    Probe("libcall", CXXFLAGS, """
static volatile double base = 3.0;

extern "C" int entry () { return (int) __builtin_powi (base, 5); }
""", expect=243),
    # __multi3, the one symbol that stood between a clang-built sysroot and any
    # hosted C++ link: wasi-libc's intscan.c calls it, and libstdc++'s locale setup
    # reaches intscan. This compiler never emits a call to it -- TImode is vetoed, so
    # there is no __int128 to multiply -- so the routine exists purely so that objects
    # built by the compiler the rest of the ecosystem uses can be linked against ours.
    #
    # It computes rather than links, and that is the point of the row. A symbol libgcc
    # does not define is a link error anyone notices; a hand-written 64x64 -> 128
    # multiply that drops the carry out of its two middle partial products links
    # perfectly and is wrong only on the inputs nobody picks. So the operands are
    # volatile and chosen to carry through every stage, and both result words are
    # checked: 100 for the low word, 23 for the high, 123 for a routine that is right.
    #
    # The ABI this declares is clang's, read off intscan.c.obj's type section rather
    # than assumed -- (i32, i64, i64, i64, i64) -> nil, a result pointer first and each
    # 128-bit argument as two i64s low word first. This probe cannot check that reading,
    # since it shares it; `include_vector` linking without wasm-ld's `function signature
    # mismatch' is what checks it, and that is the row to watch if this one stays green
    # while the hosted ones do not.
    Probe("multi3", CXXFLAGS, """
extern "C" void __multi3 (unsigned long long *result,
                          unsigned long long a_lo, unsigned long long a_hi,
                          unsigned long long b_lo, unsigned long long b_hi);

static volatile unsigned long long a_lo = 0x1234567890ABCDEFull;
static volatile unsigned long long a_hi = 0x0F0F0F0F0F0F0F0Full;
static volatile unsigned long long b_lo = 0xFEDCBA9876543210ull;
static volatile unsigned long long b_hi = 0x00000000DEADBEEFull;

extern "C" int entry ()
{
  unsigned long long product[2] = {0, 0};
  __multi3 (product, a_lo, a_hi, b_lo, b_hi);

  return (product[0] == 0x236D88FE55618CF0ull ? 100 : 0)
         + (product[1] == 0x5EA231548A055A53ull ? 23 : 0);
}
""", expect=123),
    # Two translation units each defining an internal `scale`, which is the hazard the
    # annotated dialect carried and nothing measured: rwat emitted every symbol global
    # and strong whatever its linkage, so these two collided. Local binding is now a
    # `.globl` withheld, and this is the only probe that can tell whether it was.
    Probe("two_tu", CXXFLAGS, """
extern "C" int part_b ();

static int scale (int n) { return n * 3; }

extern "C" int entry () { return scale (14) + 7 + part_b (); }
""", expect=249, second="""
static int scale (int n) { return n * 100; }

extern "C" int part_b () { return scale (2); }
"""),
    # Two translation units each defining the same one-only decls, which is what a
    # header looks like in real code and what the target had no way to express: without
    # MAKE_DECL_ONE_ONLY an inline function and a vtable are deduplicated only by the
    # plain-weak fallback, with no group tying a vtable to the rest of its class.
    #
    # It is a tripwire rather than a measurement of the dedup itself, and the comment
    # says so because the two are easy to confuse: the linker resolves two weak
    # definitions to one either way, so no return value can tell the group from the
    # fallback. What it does catch is both ways the group emission goes wrong, and both
    # were hit by hand getting here -- a malformed group directive, which llvm-mc
    # rejects, and a following function swallowed into the group because plain `.text'
    # does not end one, which surfaces as `undefined symbol' for a symbol the object
    # plainly defines. The structural side is asserted in Tools/wasm_dialect_probe.py,
    # where a subject can name what must appear in the object.
    #
    # -fno-rtti is in CXXFLAGS, so the vtable here needs no typeinfo and the probe stays
    # freestanding; a virtual destructor is deliberately absent, since the deleting
    # variant would reach for an operator delete this probe has no libc for.
    Probe("comdat_tu", CXXFLAGS, """
struct shape
{
  virtual int sides () const { return 3; }
};

inline int doubled (int n) { return n * 2; }

extern "C" int part_b ();

extern "C" int entry ()
{
  shape s;
  return doubled (s.sides ()) + part_b ();
}
""", expect=606, second="""
struct shape
{
  virtual int sides () const { return 3; }
};

inline int doubled (int n) { return n * 2; }

extern "C" int part_b ()
{
  shape s;
  return doubled (s.sides ()) * 100;
}
"""),
    # The first probe to need a target libc at all, and the first to run under a real
    # WASI host rather than against stub imports: malloc, a string function and stdio,
    # which between them are most of what libstdc++'s own configure tests link.
    Probe("libc", CXXFLAGS, """
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" int entry ()
{
  char *s = (char *) malloc (32);
  strcpy (s, "libc");
  printf ("probe sees %s\\n", s);
  int n = (int) strlen (s);
  free (s);
  return n * 10;
}
""", expect=40, hosted=True),
    # A container, which is the shortest statement of "hosted libstdc++ works": it
    # needs the headers, the library, the operator new in libsupc++ and the allocator
    # underneath that, so a gap in any of those four shows up here.
    Probe("include_vector", CXXFLAGS, """
#include <vector>

extern "C" int entry ()
{
  std::vector<int> v;
  for (int i = 1; i <= 10; ++i)
    v.push_back (i * i);
  int acc = 0;
  for (int n : v)
    acc += n;
  return acc;
}
""", expect=385, hosted=True, cxx_runtime=True),
    # errno, which has to be the *same* integer on both sides of the language boundary.
    # It computes rather than links on purpose: a missing __emutls_v.errno is a link
    # error anyone notices, while an errno that is private to the C++ side links
    # perfectly and then reports success for every failing libc call. So the probe
    # never writes the value it reads -- it clears errno, hands an out-of-range literal
    # to libc's own integer parser, and asks whether what libc set is what C++ sees.
    #
    # It was step 17's row for the sysroot edit that is now gone, and it keeps its job
    # exactly: on the threads variant errno is a real TLS symbol, so this is the probe
    # that says GCC's TLS addressing reaches the same storage libc writes. Red here
    # means the backend's TLS is wrong, and it would otherwise be invisible -- a
    # per-thread copy C++ alone can see links perfectly and reports success for every
    # failing libc call.
    Probe("errno_shared", CXXFLAGS, """
#include <cerrno>
#include <climits>
#include <cstdlib>

extern "C" int entry ()
{
  errno = 0;
  const long parsed = std::strtol ("99999999999999999999", nullptr, 10);
  return (errno == ERANGE && parsed == LONG_MAX) ? 7 : 0;
}
""", expect=7, hosted=True, cxx_runtime=True),
    # Step 19's three, in the order they narrow a failure: the compiler's own TLS, then
    # libstdc++ declaring a mutex at all, then a thread actually running.
    #
    # A thread_local across two translation units, which is the question one TU cannot
    # ask. `0015' emits a .tdata definition in one object and a `@TLSREL' reference in
    # the other, and only the link decides whether those agree -- a single TU could be
    # self-consistent and wrong. It is also the row that would catch the TLS block being
    # laid out per-object instead of once.
    Probe("thread_local_shared", CXXFLAGS, """
extern int bump ();

extern "C" int entry ()
{
  return bump () + bump () + bump ();
}
""", expect=42, second="""
thread_local int counter = 12;

int bump ()
{
  counter += 1;
  return counter;
}
"""),
    # std::mutex existing, which is what six of step 18's eight rejected engine units
    # needed and the reason this step was taken. It locks and unlocks rather than only
    # declaring one, so the row also covers pthread_mutex_lock being reachable at the
    # link -- `<mutex>` compiling and the libc call resolving are different facts.
    Probe("std_mutex", CXXFLAGS, """
#include <mutex>

static std::mutex m;
static int guarded = 7;

extern "C" int entry ()
{
  {
    std::lock_guard<std::mutex> lock (m);
    guarded += 35;
  }
  std::unique_lock<std::mutex> second (m);
  return second.owns_lock () ? guarded : 0;
}
""", expect=42, hosted=True, cxx_runtime=True, threads=True),
    # A thread that actually runs, joined, with its result read back. The point of
    # taking real pthreads rather than a single-threaded gthread model was that this
    # row is reachable at all, so it is the row that says the trade paid off: a
    # declarations-only model would compile the two above and never this one.
    Probe("std_thread", CXXFLAGS, """
#include <thread>

static int produced = 0;

extern "C" int entry ()
{
  std::thread worker ([] { produced = 42; });
  worker.join ();
  return produced;
}
""", expect=42, hosted=True, cxx_runtime=True, threads=True),
    # A compare-exchange that FAILS: the exchange does not happen, the expected value is
    # updated to what was found, and memory is left alone. Nothing else covered the
    # failing path -- every other atomic row here succeeds.
    #
    # **It is the regression test for ASM_FEATURES**, and that is its load-bearing job:
    # it is the only probe whose own translation unit emits an atomic instruction, so it
    # is the only one that fails when the probe pipeline assembles without `-mattr'
    # ("instruction requires: atomics", measured both ways). Every other thread probe
    # gets its atomics from libstdc++ objects the driver assembled through ASM_SPEC,
    # which is exactly how the pipeline went a whole step passing the flag nowhere and
    # staying green.
    #
    # **It does NOT discriminate the two spellings of `atomic_cmpxchg's memory set**, and
    # that was measured rather than assumed, because the comment it replaces claimed it
    # did. Built against `(set mem desired)' -- the false form, which says memory
    # unconditionally became the desired value -- this returns 7 just the same, with a
    # real `i32.load' in the output, at -O2, for a plain load as well as an atomic one.
    # The reason is the sibling unspec_volatile in the same insn: it makes the whole
    # pattern side-effecting, so no pass uses the second set's claim. The claim is still
    # false and still fixed; what is not available is a row that would catch it coming
    # back. -O2 is kept because the fold would need it if anything ever did look.
    Probe("cmpxchg_fail", CXXFLAGS + " -O2", """
static int cell = 7;

extern "C" int entry ()
{
  int expected = 99;
  if (__atomic_compare_exchange_n (&cell, &expected, 1234, false,
				   __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
    return 0;
  if (expected != 7)
    return 1;
  return __atomic_load_n (&cell, __ATOMIC_SEQ_CST);
}
""", expect=7, hosted=True, cxx_runtime=False),
    # A throw caught in the same function, which is the narrowest thing that asks for
    # an unwinder. It carries its own flags because CXXFLAGS turns exceptions and RTTI
    # off for every other probe, and a probe compiled with -fno-exceptions would
    # measure the absence of the question.
    #
    # Worth its own row even though include_vector reaches the same libraries: that one
    # pulls libstdc++'s throwing objects in whether or not the probe throws, so it
    # cannot distinguish "libstdc++ contains throwing code" from "a throw does not
    # work". This one is the second half, and it is the row that separates the
    # terminating unwinder of 0009 from a real one.
    Probe("throw_catch", "-std=c++26", """
#include <stdexcept>

extern "C" int entry ()
{
  try
    {
      throw std::runtime_error ("probe");
    }
  catch (const std::runtime_error &)
    {
      return 7;
    }
  return 0;
}
""", expect=7, hosted=True, cxx_runtime=True,
       blocked=(Blocked(LINKED, "trapped",
                        "0009's terminating unwinder, doing what it says: this links "
                        "and then traps in the abort () inside _Unwind_RaiseException. "
                        "Confirmed to be that abort and not something else by making "
                        "the stub return rather than trap, at which point the probe "
                        "runs and returns 7. So `trapped' is this row's honest status "
                        "until the WebAssembly exception-handling proposal reaches the "
                        "back end -- TARGET_EXCEPT_UNWIND_INFO is UI_NONE, which "
                        "suppresses EH region generation, so nothing downstream has "
                        "ever been asked for a landing pad. The instructions "
                        "themselves are reachable: `eh_instructions' in "
                        "Tools/wasm_dialect_probe.py assembles, links and runs a "
                        "try_table catching a thrown tag. A throw terminating is what "
                        "this target does, and this is the row that says so."),)),
    # A named module with a global initializer, and a second TU that imports it. The
    # narrowest subject for the one thing `import std` cannot ask: an `import std;` TU
    # emits no `_ZGIW` call at all, so until the engine rows arrived nothing in this
    # pipeline had ever assembled a *consumer* of a named module.
    #
    # The initialiser is dynamic on purpose, for two reasons at once. It is what makes
    # the module's initializer non-empty, so `module_determine_import_inits` marks it
    # active and the consumer emits the call this probe is about; and, as in
    # `init_array` above, a value the frontend cannot fold is what separates "the call
    # was emitted" from "the call ran". Freestanding: nothing here needs a libc, and
    # the harness calls `__wasm_call_ctors` itself for a probe that is not a reactor.
    Probe("module_init", CXXFLAGS + " -fmodules", """
export module mi;

static volatile int seed = 9;

export int value = seed * 5;
""", expect=45, second="""
import mi;

extern "C" int entry () { return value; }
"""),
    # A function pointer whose target is instantiated and addressed in one TU and called
    # in another, which is what `funcptr` cannot ask: there both targets are defined in
    # the same unit, so the definition's own signature decides the call site's type and
    # the two cannot disagree. Here the indirect call is in the module interface and the
    # address-take is in its consumer, which is the engine's shape exactly --
    # `Ecs/SystemManifest.cppm` forms `&invoke_annotated_fn<...>` and `Ecs/Scheduler.cpp`
    # calls through it.
    #
    # Two specialisations rather than one, for `funcptr`'s reason: a pointer that
    # resolved to the wrong table index cannot pass by landing on the right function.
    Probe("funcptr_tu", CXXFLAGS + " -fmodules", """
export module fp;

export template <int N> int step (int n) { return n * N; }

export int call_through (int (*fn) (int), int n) { return fn (n); }
""", expect=1421, second="""
import fp;

extern "C" int entry ()
{
  return call_through (&step<2>, 7) * 100 + call_through (&step<3>, 7);
}
"""),
    # `import std`, which is the first thing the engine's own code needs: every file
    # under Engine/Source reaches the standard library that way, and Import/Math.cppm
    # -- the gse.math probe the plan has been pointing at since step 3 -- imports
    # nothing else. Hence this being the last row rather than a nicety: it is the one
    # that says whether the engine-side port can start.
    Probe("import_std", CXXFLAGS + " -fmodules", """
import std;

extern "C" int entry ()
{
  std::vector<int> v {3, 1, 2};
  std::ranges::sort (v);
  return v[0] * 100 + v[1] * 10 + v[2];
}
""", expect=123, hosted=True, cxx_runtime=True, std_module=True),
    # A coroutine, which nothing above measures at all and which is how every system in
    # the engine's scheduler runs. The subject is the narrowest one that reaches the
    # resume this target gets wrong: a task whose `final_suspend` transfers symmetrically
    # to `std::noop_coroutine ()`, which is the arm a task with no continuation takes, and
    # whose resume is a `call_indirect` through a pointer in a frame libstdc++ writes
    # rather than the compiler. Hosted because a coroutine frame is an `operator new`.
    #
    # The returned value is computed from a volatile so the frontend cannot fold it: a
    # constant here would pass whether or not the coroutine ever resumed.
    Probe("coroutine_resume", CXXFLAGS + " -fmodules", """
import std;

struct task
{
  struct promise_type;
  using handle = std::coroutine_handle<promise_type>;

  struct final_awaiter
  {
    bool await_ready () const noexcept { return false; }

    std::coroutine_handle<> await_suspend (handle) const noexcept
    { return std::noop_coroutine (); }

    void await_resume () const noexcept {}
  };

  struct promise_type
  {
    int value = 0;

    task get_return_object () { return task {handle::from_promise (*this)}; }
    std::suspend_always initial_suspend () const noexcept { return {}; }
    final_awaiter final_suspend () const noexcept { return {}; }
    void return_value (int v) { value = v; }
    void unhandled_exception () {}
  };

  handle h;
};

static volatile int seed = 6;

static task make () { co_return seed * 7; }

extern "C" int entry ()
{
  task t = make ();
  t.h.resume ();
  const int out = t.h.promise ().value;
  t.h.destroy ();
  return out;
}
""", expect=42, hosted=True, cxx_runtime=True, std_module=True),
    # The engine's own code, which everything above exists to make possible. Five rows
    # rather than one, each the deepest slice that still answers a single question, so
    # that a red names the module it is in rather than "the engine does not compile".
    #
    # `gse.meta` first because it is the engine's reflection-heaviest code: fourteen
    # partitions of P2996 over annotations, fields, enums and dispatch, against a
    # target where `reflection' above only measures the core language. This is the row
    # that says the engine's reflection runs on wasm32, and it is green.
    Probe("engine_meta_parts", ENGINE_CXXFLAGS, "",
          hosted=True, cxx_runtime=True, std_module=True, partitions_of="gse.meta"),
    Probe("engine_assert", ENGINE_CXXFLAGS, "",
          hosted=True, cxx_runtime=True, std_module=True, closure_of="gse.assert"),
    # The whole of gse.meta, `Hash.cppm` included -- the file the previous snapshot
    # would not take.
    Probe("engine_meta", ENGINE_CXXFLAGS, "",
          hosted=True, cxx_runtime=True, std_module=True, closure_of="gse.meta"),
    # The row this step exists for: nineteen partitions of unit-typed math, reflection
    # and contracts, compiled for a 32-bit target for the first time. `Units.cppm`
    # carries its own static_asserts over dimensioned arithmetic, so a green here is
    # already more than "it parsed".
    Probe("engine_math", ENGINE_CXXFLAGS, "", hosted=True, cxx_runtime=True,
          std_module=True, closure_of="gse.math"),
    # And the one that computes. 350 from a unit conversion, 5 from a vec3 magnitude,
    # 6 from a 4x4 float multiply -- the last of which is the only path through
    # `simd::mul_mat4`, whose portable arm has no x86 original to fall back on. A
    # compile-only row could not have caught 0012's constant miscompile, and this
    # family of code is where a repeat of it would land.
    Probe("engine_math_run", ENGINE_CXXFLAGS, """
import std;

import gse.math;

extern "C" int entry ()
{
  const gse::length distance = gse::meters (3.f) + gse::centimeters (50.f);
  const gse::vec3<gse::length> leg = gse::meters (3.f, 4.f, 0.f);
  const gse::mat4f product = gse::mat4f (2.f) * gse::mat4f (3.f);

  return static_cast<int> (distance.as<gse::meters> () * 100.f)
         + static_cast<int> (gse::magnitude (leg).as<gse::meters> ())
         + static_cast<int> (product[0][0]);
}
""", expect=361, hosted=True, cxx_runtime=True, std_module=True,
         closure_of="gse.math",
         # gse.assert's out-of-line symbols, which `Quantity.cppm` reaches through the
         # assert templates. Terminating rather than reporting, the same bargain 0009's
         # unwinder makes: what this row measures is the math, and a real reporter is a
         # host port.
         #
         # `handle_contract_violation` is the fourth and is not an assert symbol at all:
         # `-fcontracts` makes every contract_assert in the engine's math call it, and
         # `Import/Assert.cpp` is where the engine defines it. It is stubbed here for
         # the same reason as the other three and discovered the same way -- as an
         # undefined symbol at link, once 0013 let this row reach the linker.
         second="""
module gse.assert;

import std;

auto gse::install_fatal_reporter (fatal_reporter) -> void {}

auto gse::fatal_exit (int code) noexcept -> void { std::_Exit (code); }

auto gse::assert_fail (std::source_location, std::string_view) noexcept -> void
{
  std::abort ();
}

extern "C++" auto handle_contract_violation (const std::contracts::contract_violation &)
  -> void
{
  std::abort ();
}
"""),
)

def modes_of(probe: Probe) -> tuple:
    """The pipelines a probe is measured through, which follow from `expect` alone."""
    return (COMPILE,) if probe.expect is None else (LINKED,)


# A `Blocked` naming a mode its probe is never measured through excuses nothing and reads
# as coverage, which is the blindfold the type exists to prevent. Catching it here rather
# than letting it pass as an absent row is what keeps the next person who adds a known-red
# probe from writing one by accident.
for _probe in PROBES:
    for _blocked in _probe.blocked:
        if _blocked.mode not in modes_of(_probe):
            raise SystemExit(f"probe {_probe.name}: not measured in mode {_blocked.mode}")


def configure_args(prefix: Path, msys_root: Path, sysroot: Path) -> list:
    """The cross configuration: a compiler, libgcc and libstdc++, against wasi-libc.

    Every --disable below names a target library that has no wasm32 port to build.
    Dropping any of them makes configure descend into a library that cannot be built
    yet. --disable-libstdcxx is NOT among them any more, which is the point of this
    step: libstdc++ cannot be configured --without-headers, because its feature tests
    compile and link against a libc, so the sysroot has to come first.

    There is deliberately no --with-native-system-header-dir, even though the sysroot's
    headers are not where GCC looks by default: on an MSYS2 host a bare `/include'
    reaches the *compile* as C:/...msys64/include, because the argument converter reads
    it as a POSIX path in a -D. Patch 0006 states it in the two places that have to
    agree instead -- config.gcc for the Makefile and wasm.h for the preprocessor --
    which is how config/mingw handles the same bug.

    --with-as/--with-ld are what let a *target library* build at all, as opposed to a
    probe. A probe stops at `-S` and is handed to llvm-mc by this script, so it never
    needs the driver to find an assembler; libgcc compiles to an object through the
    driver, which looks for `as` in the -B directory and finds the build tree's
    exec-tool shim. That shim refuses with "$original is unset" unless top-level
    configure was told where the target's assembler is, and no amount of PATH helps,
    because the shim is consulted before PATH. Naming them here is also what keeps the
    driver's own notion of the toolchain and this script's agreeing: one llvm-mc and
    one wasm-ld, named once.

    AR_FOR_TARGET and RANLIB_FOR_TARGET are the same problem one step further on: every
    object of libgcc compiles and then `wasm32-unknown-none-ar: command not found' ends
    the build, because there is no binutils for this target and nothing supplies a
    default. llvm-ar writes and indexes a wasm archive, and comes from the same LLVM
    release as the other two.
    """
    llvm_bin = msys_root / "mingw64" / "bin"
    return [
        f"--target={TARGET}",
        f"--prefix={to_msys(prefix)}",
        f"--with-as={to_msys(llvm_bin / 'llvm-mc.exe')}",
        f"--with-ld={to_msys(llvm_bin / 'wasm-ld.exe')}",
        f"AR_FOR_TARGET={to_msys(llvm_bin / 'llvm-ar.exe')}",
        f"RANLIB_FOR_TARGET={to_msys(llvm_bin / 'llvm-ranlib.exe')}",
        f"NM_FOR_TARGET={to_msys(llvm_bin / 'llvm-nm.exe')}",
        "--enable-languages=c,c++",
        "--disable-bootstrap",
        "--enable-checking=release",
        f"--with-sysroot={to_msys(sysroot)}",
        # wasi-libc's signal.h is an #error unless this is defined, and libstdc++'s
        # <csignal> includes it unconditionally. -g -O2 is restated because naming
        # these variables replaces their default rather than adding to it, and the
        # value is quoted here rather than at the join: these arguments go onto a
        # shell command line, and the only ones with a space in them are these two.
        #
        # -D_Thread_local=__thread is load-bearing on the threads variant rather than
        # the inert leftover step 17 left it as: see CXX_RUNTIME_DEFINES, which states
        # the same thing for the compiles that happen outside the library build.
        "CFLAGS_FOR_TARGET='-g -O2 -D_WASI_EMULATED_SIGNAL'",
        "CXXFLAGS_FOR_TARGET='-g -O2 -D_WASI_EMULATED_SIGNAL "
        "-D_Thread_local=__thread'",
        # The threads variant of the sysroot has pthreads, and this is what makes
        # libstdc++ use them: the gthread model decides its atomicity and locking
        # implementation, and with no model it declares no std::mutex at all -- six of
        # step 18's eight rejected engine units. libgcc/config.host's `wasm*-*-*' arm
        # names no thread_file, so `posix' here is what reaches gthr-posix.h, which
        # includes the sysroot's own pthread.h.
        #
        # This argument is the build directory's cache key, so changing it discards the
        # directory and rebuilds all-gcc, libgcc and libstdc++. That is why step 19 did
        # every cheap measurement before touching this line.
        "--enable-threads=posix",
        "--disable-shared",
        # libgcov wraps fork and the six exec functions, and compiles those
        # wrappers whenever a libc is present at all -- wasi-libc has no process
        # primitives, so they are undeclared and libgcc does not build. There is no
        # way to drop them from a t- fragment: libgcc/Makefile.in includes
        # tmake_file at :382 and assigns LIBGCOV_INTERFACE at :923, so the
        # assignment wins. Coverage instrumentation for a browser build is a
        # question for much later anyway.
        "--disable-gcov",
        "--disable-libssp",
        "--disable-libatomic",
        "--disable-libgomp",
        "--disable-libquadmath",
        "--disable-libvtv",
        "--disable-multilib",
        "--disable-nls",
        "--disable-werror",
        # --with-gxx-libcxx-include-dir used to be here, to make the `-stdlib=libc++`
        # that wasm.h forced into CC1PLUS_SPEC a recognized option. Patch 0006 removes
        # that spec instead: a compiler that builds libstdc++ should not be telling the
        # driver to look for libc++, and with the spec gone the option it needed is not
        # needed either.
        "--with-gmp=/mingw64",
        "--with-mpfr=/mingw64",
        "--with-mpc=/mingw64",
        "--with-isl=/mingw64",
    ]


def fetch_source(msys_root: Path, workdir: Path, snapshot: str, cache: Path) -> Path:
    src = workdir / f"gcc-17-{snapshot}"
    if (src / "gcc" / "config.gcc").exists():
        print(f"Using existing source tree {src}")
        return src
    tarball = cache / f"gcc-17-{snapshot}.tar.xz"
    if not tarball.exists():
        cache.mkdir(parents=True, exist_ok=True)
        url = SNAPSHOT_URL.format(snapshot=snapshot)
        print(f"Downloading {url}")
        bash(msys_root, f"curl -fL -o {to_msys(tarball)} {url}")
    workdir.mkdir(parents=True, exist_ok=True)
    print(f"Extracting {tarball}")
    bash(msys_root, f"cd {to_msys(workdir)} && tar -xf {to_msys(tarball)}")
    if not (src / "gcc" / "config.gcc").exists():
        raise SystemExit(f"extraction did not produce {src}")
    return src


def stage_sysroot(msys_root: Path, workdir: Path, cache: Path) -> Path:
    """Download and extract the multi-target wasi-sysroot tarball, once.

    Separate from the flattening below because two callers want the *unflattened* tree:
    fetch_sysroot, which copies one target out of it, and wasm_dialect_probe, which
    links a hand-written subject straight against the threads target's archives. Those
    archives are the point of that subject -- it asks whether real pthreads link and
    run -- so it has to reach the originals rather than a flattened copy of whichever
    target this script happens to be configured for.
    """
    staged = workdir / f"wasi-sysroot-{WASI_SYSROOT_VERSION}"
    if (staged / "lib" / WASI_TARGET).is_dir():
        return staged
    tarball = cache / f"wasi-sysroot-{WASI_SYSROOT_VERSION}.tar.gz"
    if not tarball.exists():
        cache.mkdir(parents=True, exist_ok=True)
        url = WASI_SYSROOT_URL.format(major=WASI_SYSROOT_VERSION.split(".")[0],
                                      version=WASI_SYSROOT_VERSION)
        print(f"Downloading {url}")
        bash(msys_root, f"curl -fL -o {to_msys(tarball)} {url}")
    workdir.mkdir(parents=True, exist_ok=True)
    print(f"Extracting {tarball}")
    bash(msys_root, f"rm -rf {to_msys(staged)} && cd {to_msys(workdir)} "
                    f"&& tar -xzf {to_msys(tarball)}")
    if not (staged / "lib" / WASI_TARGET).is_dir():
        raise SystemExit(f"extraction did not produce {staged}/lib/{WASI_TARGET}")
    return staged


# The `-mattr=' value inside the backend's ASM_SPEC, which is the authority on what
# the driver tells the assembler. Required to be there and to be exactly one.
ASM_SPEC_MATTR = re.compile(r'"-triple=[^"]*-mattr=([^ "]+)')


def check_asm_features(src: Path) -> None:
    """Refuse to measure when ASM_FEATURES and the backend's ASM_SPEC disagree.

    The two have to say the same thing: the driver assembles libgcc and libstdc++
    through ASM_SPEC, and this script assembles every probe itself. Where they differ,
    the probe table reports on an assembler no engine object was ever built with, and
    the failure is a red row whose cause is the harness -- which is exactly the shape
    of step 18c's ENGINE_CXXFLAGS bug and of this check's own origin: the probe
    pipeline passed no -mattr at all for a whole step, and nothing caught it because no
    probe's own translation unit happened to emit an atomic.

    Checked against the patched source rather than the built compiler because that is
    where the string lives, and because a mismatch should stop the run before an hour
    of building rather than after it.
    """
    header = src / "gcc" / "config" / "wasm" / "wasm.h"
    found = ASM_SPEC_MATTR.findall(header.read_text(encoding="utf-8"))
    if found == [ASM_FEATURES]:
        return
    raise SystemExit(
        f"{header}: ASM_SPEC names -mattr={found!r} while this script assembles probes "
        f"with -mattr={ASM_FEATURES!r}. They have to agree, or the probe table measures "
        "an assembler no engine object is built with; see ASM_FEATURES.")


def sysroot_stamp(sysroot: Path) -> tuple:
    """A flattened sysroot's stamp file and the inputs it has to carry.

    Shared with --sweep-only, which refuses a sysroot these do not match rather than
    re-flattening one: a second spelling of the inputs string would let that arm
    measure against a sysroot `fetch_sysroot` would have discarded.
    """
    return (sysroot / ".wasi-sysroot-version",
            f"{WASI_SYSROOT_VERSION}\n{WASI_TARGET}\n{SYSROOT_EDITS}")


def fetch_sysroot(msys_root: Path, workdir: Path, cache: Path) -> Path:
    """Lay the wasi-libc sysroot out the way GCC expects to find one.

    The release tarball is multi-target: headers under include/wasm32-wasip1 and
    libraries under lib/wasm32-wasip1, with wasip2, wasip3 and a threads variant
    beside them. GCC looks for one set of headers and one library directory, so the
    chosen target is flattened into <sysroot>/include and <sysroot>/lib/wasm32-wasi.

    That last directory name is not a tidier spelling of the same thing: wasm.h's
    STANDARD_STARTFILE_PREFIX_1 is "/lib/wasm32-wasi/", so it is where the driver
    looks for crt1.o, and renaming it would mean patching the backend instead.
    """
    sysroot = workdir / "sysroot"
    # Keyed on the pinned version rather than on the sysroot being there at all.
    # WASI_SYSROOT_VERSION is load-bearing -- which of wasi-libc's headers are
    # C++-clean depends on it -- so bumping it against an existing workdir would
    # otherwise go on measuring the previous one, and report that as the new pin.
    stamp, inputs = sysroot_stamp(sysroot)
    if not stale(stamp, inputs):
        print(f"Using existing sysroot {sysroot} ({WASI_SYSROOT_VERSION})")
        return sysroot
    staged = stage_sysroot(msys_root, workdir, cache)
    print(f"Flattening {WASI_TARGET}")
    bash(msys_root, f"rm -rf {to_msys(sysroot)} "
                    f"&& mkdir -p {to_msys(sysroot)}/lib "
                    f"&& cp -r {to_msys(staged)}/include/{WASI_TARGET} {to_msys(sysroot)}/include "
                    f"&& cp -r {to_msys(staged)}/lib/{WASI_TARGET} {to_msys(sysroot)}/lib/wasm32-wasi")
    if not (sysroot / "include" / "stdio.h").exists():
        raise SystemExit(f"flattening did not produce {sysroot}/include/stdio.h")
    declare_setjmp(sysroot)
    record(stamp, inputs)
    return sysroot


# The guard that has to go, as it stands in wasi-libc 32.0's setjmp.h.
SETJMP_GUARD = re.compile(
    r"#if !defined\(__wasm_exception_handling__\)\n#error Setjmp/longjmp support[^\n]*\n"
    r"#endif\n")


def declare_setjmp(sysroot: Path) -> None:
    """Let the flattened setjmp.h declare setjmp, without defining it.

    wasi-libc refuses to declare setjmp at all unless the wasm exception-handling
    proposal is on: the header is a bare #error. That blocks far more than setjmp.
    libstdc++ precompiles <csetjmp> into stdc++.h.gch, and src/c++23/std.cc.in is
    `#include <bits/stdc++.h>', so the #error stops libstdc++ building and stops the
    `std' module existing -- over a function nothing in either one calls.

    Removing the guard leaves `setjmp' declared and undefined, which is the literal
    truth for a wasi-libc built without the proposal: there is no setjmp.o in this
    sysroot, so code that really calls it fails at link, naming the symbol. Nothing
    here defines __wasm_exception_handling__, because that would be a claim about a
    capability the backend does not have -- it would get this header past the
    preprocessor and then produce a module that silently does not unwind.

    The pattern is required rather than optional: a release that writes the guard
    differently must fail here and be looked at, not quietly keep an #error that the
    caller then reads as the edit having been applied.
    """
    header = sysroot / "include" / "setjmp.h"
    text = header.read_text(encoding="utf-8")
    edited, n = SETJMP_GUARD.subn("", text)
    if n != 1:
        raise SystemExit(
            f"{header}: the exception-handling guard this edit removes is not there in "
            f"the shape it is expected in. wasi-libc {WASI_SYSROOT_VERSION} may have "
            "changed it; see declare_setjmp.")
    with open(header, "w", encoding="utf-8", newline="\n") as f:
        f.write(edited)
    print(f"Patched {header}: setjmp declared, still undefined")


def stale(stamp: Path, inputs: str) -> bool:
    """Whether an artifact on disk was produced from inputs other than the current ones.

    Everything expensive here is cached by existing: the source tree, the sysroot, the
    build directory. Existence cannot say *which* inputs produced it, and each of those
    has an input that is both load-bearing and easy to change -- the pinned sysroot
    version, a re-cut local patch, a new configure argument. Changing one leaves an
    artifact that looks finished and measures something else, silently, which is the
    worst failure this script has available to it.

    So each artifact records what it was built from and compares. A missing stamp beside
    an existing artifact counts as stale too: that is exactly the case that cannot be
    told apart from "built correctly, before anything recorded this", and treating it as
    stale costs one rebuild rather than a wrong measurement.
    """
    return not stamp.exists() or stamp.read_text(encoding="utf-8") != inputs


def record(stamp: Path, inputs: str) -> None:
    """Note what an artifact was just built from, for `stale` to read next time."""
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text(inputs, encoding="utf-8")


# The vendored series, in application order, and the testsuite patch that is not part
# of building a compiler. Module-level rather than a local of apply_patches because
# --sweep-only verifies the same series without applying it, and a second copy of this
# list is a list that can disagree with the one that did the applying.
PATCH_NAMES = ("0001-wasm-New-backend.patch", "0002-local-Windows-host-portability.patch",
               "0005-local-emit-llvm-mc-s-assembly-dialect.patch",
               "0006-local-sysroot-and-libstdcxx-host.patch",
               "0007-local-one-only-and-aliases.patch",
               "0008-local-import-signatures.patch",
               "0009-local-terminating-unwinder.patch",
               "0010-local-weak-references.patch",
               "0011-local-multi3.patch",
               "0012-local-host-wide-int-operands.patch",
               "0013-local-module-initializer-decl.patch",
               "0014-local-personality-accessors.patch",
               "0015-local-native-tls.patch",
               "0016-local-atomics.patch",
               "0017-local-gthr-no-weak-refs.patch",
               "0018-local-called-function-decls-and-muloti4.patch",
               "0019-local-noop-coroutine-frame-signature.patch")
TESTS_PATCH = "0003-wasm-Adjust-tests.patch"


def patch_names(with_tests: bool) -> list:
    """The series a run of this script expects to be applied to its source tree."""
    return list(PATCH_NAMES) + ([TESTS_PATCH] if with_tests else [])


def patch_stamp(src: Path, patches: Path, name: str) -> tuple:
    """A patch, its stamp file, and the digest that stamp has to carry.

    One place rather than two: the digest is what `apply_patches` records and what
    --sweep-only checks, and if those two ever computed it differently the check would
    pass on a tree the applier would have rejected.
    """
    patch = patches / name
    if not patch.exists():
        raise SystemExit(f"missing vendored patch {patch}")
    return (patch, src / f".applied-{name}",
            hashlib.sha256(patch.read_bytes()).hexdigest())


def apply_patches(msys_root: Path, src: Path, patches: Path, with_tests: bool) -> None:
    for name in patch_names(with_tests):
        # Keyed on the patch's bytes, not just its name: `0005` and `0006` are ours and
        # get re-cut, and a stamp that only records "applied" would leave the tree
        # carrying the previous revision while every later step reports on it. Unlike
        # the sysroot and the build directory this cannot be repaired by rebuilding --
        # re-applying an edited patch to an already-patched tree fails in a way that
        # reads like a rejected hunk -- so it refuses and names the fix instead.
        patch, stamp, digest = patch_stamp(src, patches, name)
        if not stale(stamp, digest):
            print(f"{name} already applied")
            continue
        if stamp.exists():
            raise SystemExit(
                f"{name} has changed since it was applied to {src}.\n"
                f"Delete that source tree and re-run; it will be re-extracted and the "
                f"current patches applied to it.")
        # --batch: without it patch asks "Reversed (or previously applied) patch
        # detected! Assume -R?" and, with no tty, answers no and *skips the file* while
        # still failing — which reads like a rejected hunk rather than a re-run.
        bash(msys_root, f"cd {to_msys(src)} && patch -p1 --batch --no-backup-if-mismatch -i {to_msys(patch)}")
        record(stamp, digest)


class Toolchain(NamedTuple):
    """Where the things a probe needs live.

    Grouped because they are all paths and are handed to one function: a positional
    call of `(msys_root, gcc_dir, libgcc_dir)` conceals which is which and permits a
    valid-looking swap, while the field names make the one call site read.
    """
    msys_root: Path
    gcc_dir: Path
    libgcc_dir: Path
    sysroot: Path
    libstdcxx_dir: Path
    libstdcxx_include_dir: Path
    libsupcxx_include_dir: Path
    std_module_sources: tuple

    def near(self, path: Path) -> str:
        """PATH as the run tools can open it: relative to where they are invoked.

        xg++, llvm-mc and wasm-ld are mingw binaries and cannot open the /c/... paths
        an MSYS2 shell hands them, so nothing on a command line here may be absolute.
        """
        return os.path.relpath(path, self.gcc_dir).replace("\\", "/")


def target_cxxflags(tools: Toolchain, hosted: bool, cxx_runtime: bool) -> str:
    """The include flags a compile for this target needs, in one place.

    One place rather than two, because a second copy of them is the likeliest way to
    turn a green row red for a reason that is not the target -- a compile that cannot
    see <vector> fails the same way whatever state libstdc++ is in. Both the probe table
    and the module sweep call this.

    A freestanding compile keeps -nostdinc, which is what makes it a measurement of the
    compiler rather than of the sysroot; a hosted one is asking about the sysroot, so it
    is simply allowed to see it. No --sysroot is passed: the configured one is what the
    target libraries were built against, and naming a second here would let the two
    disagree without saying so.

    libstdc++'s headers are named explicitly, because nothing here runs `make install`
    and the driver's default C++ include directory is under the *prefix*. Four
    directories, each carrying headers the others do not, and every one of them found by
    a probe failing without it: the target-specific one carries c++config.h, the staged
    one carries most headers, `backward' carries <strstream>, which `std.cc' includes,
    and libsupc++ carries the ones belonging to the ABI runtime rather than to the
    library -- <compare> among them, reached by the first <vector>. An installed
    toolchain has all four on the default path, which is what makes their absence here
    look like a target problem instead of a missing -I. -nostdinc++ ahead of them keeps
    an installed copy, if one ever appears under the prefix, from shadowing the one just
    built.

    The engine's own include directory is last and is NOT conditional, which is the one
    departure from the paragraph above. It belongs here for the reason this function
    exists: `gse.moodycamel` is the first unit the compiler is ever offered that
    `#include`s a vendored header, and a sweep that can see it while the five engine
    probes cannot is flag divergence between the two paths -- 18c's bug, which step 18
    paid for in ten misattributed red rows. It cannot be gated on `hosted` or
    `cxx_runtime`, because three of those probes are freestanding, and it does not weaken
    what `-nostdinc` measures: this is a directory under Engine, not the target's sysroot,
    and a probe that includes nothing from it is compiled exactly as before.
    """
    flags = [] if hosted else ["-nostdinc"]
    if cxx_runtime:
        include_dir = tools.libstdcxx_include_dir
        flags += ["-nostdinc++",
                  f"-I{tools.near(include_dir / TARGET)}",
                  f"-I{tools.near(include_dir)}",
                  f"-I{tools.near(include_dir / 'backward')}",
                  f"-I{tools.near(tools.libsupcxx_include_dir)}",
                  CXX_RUNTIME_DEFINES]
    flags.append(f"-I{tools.near(ENGINE_INCLUDE_DIR)}")
    return " ".join(flags)


def compile_unit(tools: Toolchain, flags: str, stem: str, source: str) -> bool:
    """Compile one translation unit to assembly in the probe directory.

    Assembly rather than an object: llvm-mc is the assembler, and stopping at -S is what
    separates "the compiler accepted this" from every later stage. The directory is
    fixed because the module cache is keyed on the working directory -- a unit compiled
    anywhere else writes a .gcm the next unit cannot find.
    """
    return bash(tools.msys_root,
                f"cd {to_msys(tools.gcc_dir)} && "
                f"./xg++ -B. -S {flags} -o {stem}.s {source}",
                check=False).returncode == 0


def build_std_module(tools: Toolchain, flags: str) -> Optional[str]:
    """Compile libstdc++'s `std` module interface, or name how it failed.

    The `std` module has to exist as a .gcm before anything can import it, and libstdc++
    ships std.cc as a source file for exactly this reason. It is compiled in the probe
    directory rather than once up front because the cache is keyed on the working
    directory. Its own object is discarded: everything the module contributes at link
    time is already in libstdc++.a, and -S leaves nothing to link anyway.

    The interface is concatenated here from the two source-tree `.in' files, which is
    exactly what src/c++23/Makefile's own `std.cc' rule does, and NOT read from the
    generated std.cc in the build tree. That one is rewritten when the module fails to
    compile: `std.lo's recipe answers a failure with `echo > std.cc.tmp && mv
    std.cc.tmp std.cc' so that the library still builds without a module initialiser.
    Reading it therefore measured the build's recovery rather than the target, and which
    of the two a run saw depended on whether `stamp-modules-bits' copied the file before
    or after the emptying -- so one re-run of make turned this row's red from `std module
    failed', with the seven names on it, into a bare `compile failed' about a .gcm that
    was never written.
    """
    parts = " ".join(tools.near(part) for part in tools.std_module_sources)
    if bash(tools.msys_root,
            f"cd {to_msys(tools.gcc_dir)} && cat {parts} > std-module.cc",
            check=False).returncode != 0:
        return "std module sources missing"
    if not compile_unit(tools, flags, "std-module", "std-module.cc"):
        return "std module failed"
    return None


def probe_status(tools: Toolchain, probe: Probe, mode: str) -> str:
    """Take one probe through the pipeline, and name the stage it reached.

    Stages are reported separately because a run probe can fail at any of half a dozen
    points, and "FAILED" alone sends the next reader back to the shell to find out
    which one.
    """
    stem = f"probe-{probe.name}"
    # (output stem, source as the compiler can open it), in compile order. Engine
    # sources are compiled where they sit rather than copied here: a copy would be a
    # second statement of the file, and the one failure mode this probe exists to find
    # -- a construct the engine really uses meeting a 32-bit target -- is exactly the
    # one a stale copy would hide. Their output stems are indexed because two engine
    # modules may own files of the same name.
    sources = probe.sources
    if probe.closure_of:
        sources = graph().closure(probe.closure_of)
    elif probe.partitions_of:
        sources = graph().paths(graph().partitions_of(probe.partitions_of))
    units = [(f"{stem}-{i:02d}", tools.near(REPO_ROOT / source))
             for i, source in enumerate(sources)]
    written = [(stem, probe.body)]
    if probe.second:
        written.append((f"{stem}-b", probe.second))
    for unit, body in written:
        if not body:
            continue
        with open(tools.gcc_dir / f"{unit}.cc", "w", encoding="utf-8", newline="\n") as f:
            f.write(body.lstrip())
        units.append((unit, f"{unit}.cc"))
    msys_root = tools.msys_root
    cd = f"cd {to_msys(tools.gcc_dir)} && "
    flags = f"{target_cxxflags(tools, probe.hosted, probe.cxx_runtime)} {probe.flags}"
    if probe.std_module:
        failure = build_std_module(tools, flags)
        if failure:
            return failure
    for unit, source in units:
        if not compile_unit(tools, flags, unit, source):
            return f"compile failed: {Path(source).name}"
    if mode == COMPILE:
        return "compiled"

    for unit, _ in units:
        if bash(msys_root, cd + f"llvm-mc -triple={ASM_TRIPLE} "
                                f"-mattr={ASM_FEATURES} -filetype=obj "
                                f"{unit}.s -o {unit}.o", check=False).returncode != 0:
            return "assemble failed"

    binary = f"{stem}.wasm"
    objects = [f"{unit}.o" for unit, _ in units]
    if bash(msys_root, cd + wasm_ld(tools, objects, binary, probe.hosted,
                                    probe.cxx_runtime, probe.threads),
            check=False).returncode != 0:
        return "link failed"
    # --no-warnings: node:wasi is an experimental module and says so on stderr every
    # time the harness is loaded, whether or not the probe uses it. Nothing else here
    # warns, so there is nothing else to lose by silencing it.
    # The harness cannot derive sharedness from the module -- Module.imports reports a
    # memory import's kind but not its flags -- so a threads probe has to say so, or it
    # is handed a plain memory and fails with "memory import has no maximum limit".
    mode = " shared" if probe.threads else ""
    code = bash(msys_root,
                cd + f"node --no-warnings harness.mjs {binary} {probe.expect}{mode}",
                check=False).returncode
    if code == NO_INSTANCE_EXIT:
        return "did not instantiate"
    if code == TRAP_EXIT:
        return "trapped"
    if code != 0:
        return "wrong result"
    return "ran"


def wasm_ld(tools: Toolchain, objects: list, binary: str, hosted: bool,
            cxx_runtime: bool, threads: bool, entry: str = ENTRY,
            stack_size: int = 0) -> str:
    """The one wasm-ld command line, assembled in one place.

    One place for the reason the order itself has: it is load-bearing and invisible, so a
    second copy of it is how a measurement becomes an unresolved symbol. `probe_status`
    composed this inline until the engine link needed the same line over a different unit
    set -- which is exactly the shape that got the archive order wrong in the first place,
    appending them from separate `if` blocks.
    """
    # Section collection is left on, which the rwat pipeline could not do: GCC's
    # `__stack_pointer` was an import with no symbol, so wasm-ld used to drop it and
    # every module died on `global.get 0`. Declared with `.globaltype` it has a real
    # symbol, and leaving the default in place is what keeps measuring that.
    #
    # In dependency order, because wasm-ld pulls archive members lazily and scans left
    # to right, so a library must appear before everything it needs. Each archive below
    # depends only on ones further down -- libstdc++ on libsupc++, both on libgcc (and
    # on libc for malloc), libgcc on libc for memcpy and abort. Appending them from
    # separate `if` blocks got this wrong in the other direction and would not have
    # shown up until libstdc++ built, as a `link failed` on a probe whose known red is
    # a compile error.
    #
    # crt1-reactor rather than crt1: the command form defines _start and expects to be
    # the whole program, while a reactor exposes _initialize and leaves the module
    # callable afterwards -- which is what every probe here is, a module whose `entry`
    # the harness calls. It is also what lets one harness run the hosted and
    # freestanding probes alike.
    #
    # -lwasi-emulated-signal is the other half of -D_WASI_EMULATED_SIGNAL: the define
    # gets <csignal> past wasi-libc's #error, and this supplies the stubs it declares.
    #
    # libgcc is on every link line rather than only the probe that needs it. A libcall
    # is emitted by the compiler, not written by the probe, so which probes need it is
    # not a fact the probe list can state -- `-L` plus `-lgcc` costs nothing where it
    # is unused and is the difference between a measurement and an unresolved symbol
    # where it is not.
    wasi_lib = tools.near(tools.sysroot / "lib" / "wasm32-wasi")
    link = []
    if hosted:
        link += [f"{wasi_lib}/crt1-reactor.o"]
    link += objects
    if cxx_runtime:
        # -lstdc++ alone, and deliberately not -lsupc++ beside it: libtool links
        # libsupc++ into libstdc++ as a convenience library, so all 65 of its members
        # -- eh_throw, eh_personality, tinfo, pure, the lot -- are already among
        # libstdc++.a's 199. Naming the second archive as well presents every one of
        # them to the linker twice, and wasm-ld rejects that rather than taking the
        # first: `duplicate symbol: std::type_info::~type_info()' and a dozen
        # `symbol type mismatch' lines, none of which are about the target.
        #
        # This was invisible for as long as libstdc++.a did not exist -- the probe
        # stopped at `unable to find library' before it could double-count anything --
        # so the archive appearing is what turned a wrong link line into a red probe.
        link += [f"-L{tools.near(tools.libstdcxx_dir)}", "-lstdc++"]
    link += [f"-L{tools.near(tools.libgcc_dir)}", "-lgcc"]
    if hosted:
        link += [f"-L{wasi_lib}", "-lc", "-lwasi-emulated-signal"]
    # No -lpthread: on this sysroot libpthread.a is an empty 8-byte archive and every
    # pthread_* API is in libc.a, which is also why 0017 turns the weak references off.
    # What a shared memory costs at the link is below, each flag measured in 19a rather
    # than copied from a tutorial: --shared-memory needs a maximum, and --export-memory
    # is needed because node:wasi binds to `instance.exports.memory' and refuses a
    # module that only imports one.
    if threads:
        link += ["--shared-memory", "--import-memory", "--export-memory",
                 f"--max-memory={HARNESS_MAX_PAGES * PAGE_SIZE}"]
    # --export-if-defined rather than --export for the two entry points: the linker
    # synthesises __wasm_call_ctors only where some object contributed an init-array
    # entry and _initialize only for a reactor, and demanding either unconditionally
    # would turn every probe that does not have one into a link failure.
    #
    # `entry` is the same thought one level up, and the engine link is why it is an
    # argument: a probe's subject defines `entry` because the probe table wrote it, and
    # the engine's does not define anything of the sort. --export of a name no object
    # provides is a link failure in itself, which on that arm would report the absence of
    # a harness entry point as though it were one of the engine's missing symbols -- the
    # single thing that arm exists to enumerate.
    # --error-limit=0 because a truncated list is not an inventory. wasm-ld stops at 20
    # errors by default and says so, which the engine link's first run hit: fifteen
    # undefined symbols reported out of an unknown number, with "too many errors
    # emitted" as the only hint that the measurement was a sample. No probe is affected
    # -- it changes how much is printed when a link fails, never whether it does.
    #
    # TWO DASHES, and the second run of that arm paid for the one: wasm-ld parses
    # `-error-limit=0` as `-e rror-limit=0`, because single-dash `-e` is the entry
    # symbol. The link then failed with `entry symbol not defined: rror-limit=0` and
    # reported zero undefined symbols, which is what a link that never resolved anything
    # looks like from the outside.
    # Default off, like `entry`, and for the same reason: a probe's link is a row that
    # is already green, so a flag it has nothing to gain from is a new input to an old
    # measurement. See ENGINE_STACK_SIZE for why the engine's arm needs one at all.
    if stack_size:
        link += ["-z", f"stack-size={stack_size}"]
    exports = ["--error-limit=0",
               "--export-if-defined=__wasm_call_ctors",
               "--export-if-defined=_initialize",
               "--export-if-defined=__wasm_call_dtors"]
    if entry:
        exports.insert(0, f"--export={entry}")
    return (f"wasm-ld {' '.join(link)} -o {binary} --no-entry "
            f"{' '.join(exports)}")


# What one interface unit's compile came to. `cause` names the unit the verdict
# originates from, which for `ok` is the unit itself: a row that says "blocked" without
# naming what blocked it sends the reader back to the shell, which is the failure mode
# the probe table's staged statuses already exist to avoid.
class UnitStatus(NamedTuple):
    kind: str
    cause: str


# A unit that was never offered to the compiler because it IS a wrapper over a platform
# library, and the verdict that propagates from one. Everything else host-bound is
# host-bound by propagation rather than by being named anywhere -- see module_graph.py's
# HOST_DIR.
HOST = "host"
BLOCKED = "blocked"


def sweep(tools: Toolchain) -> None:
    """Compile every engine interface unit for wasm32, once, in dependency order.

    One pass, not one pass per module. A module's closure is most of its dependencies'
    closures, so compiling each module separately would compile gse.core sixty times;
    in one topologically ordered pass over all 425 units each file is offered to the
    compiler exactly once and every module's row is derived from the results. The
    module cache makes this not merely cheaper but *necessary*: a .gcm has to exist
    before a dependent can import it, and the order is what guarantees it does.

    The pass does not stop at the first failure, which is the whole point. Step 14
    measured one error cascading into fourteen and recorded the rule: a count of what
    compiles needs a sweep that continues, because a probe that stops reports the first
    error as though it were the only one.
    """
    flags = f"{target_cxxflags(tools, hosted=True, cxx_runtime=True)} {ENGINE_CXXFLAGS}"
    # Once, ahead of the pass, rather than per module: every engine unit imports std.
    failure = build_std_module(tools, flags)
    if failure:
        raise SystemExit(f"the sweep cannot start: {failure}")

    order = graph().sweep_order()
    status = {}
    for index, logical in enumerate(order, start=1):
        path = graph().path_of[logical]
        if graph().is_host_root(logical):
            status[logical] = UnitStatus(HOST, logical)
            continue
        # A unit importing a module no file in the engine declares -- the Vulkan SDK's
        # own `vulkan` is the only one in this tree -- is host-bound for the same reason
        # a wrapper is, and for the same reason must not be offered to the compiler: it
        # would fail on the missing module and report the SDK's absence as a wasm32
        # finding, with FAILED outranking host-bound in the row.
        external = graph().external_requires(logical)
        if external:
            status[logical] = UnitStatus(HOST, external[0])
            continue
        # Every dependency that is not `ok`, with host-boundness preferred over a
        # rejection -- not the first one in `requires` order, which is arbitrary. A unit
        # importing both a rejected module and a host-bound one is unreachable for the
        # host reason whatever the rejection does, so calling it `blocked on <unit>`
        # would promise that fixing that unit frees it, and would inflate the blocked
        # count with modules nobody can compile anyway.
        #
        # Host-boundness is then inherited as itself, however many modules sit between
        # this unit and the wrapper, while a rejection upstream is inherited as
        # `blocked` carrying the unit that was actually rejected. That distinction is
        # the whole value of the bottom list: `failed` has to mean the compiler was
        # handed this file and said no, because a list mixing those with the files
        # behind them is a list of 15 target gaps where there are six.
        upstream = [status[req] for req in graph().requires.get(logical, ())
                    if req in status and status[req].kind != "ok"]
        if upstream:
            blame = next((u for u in upstream if u.kind == HOST), upstream[0])
            inherited = HOST if blame.kind == HOST else BLOCKED
            status[logical] = UnitStatus(inherited, blame.cause)
            continue
        if compile_unit(tools, flags, f"sweep-{index:04d}", tools.near(REPO_ROOT / path)):
            status[logical] = UnitStatus("ok", logical)
        else:
            status[logical] = UnitStatus("failed", logical)
        print(f"  [{index:3}/{len(order)}] {status[logical].kind:7} {path}")

    print("\n== modules ==")
    tally = collections.Counter()
    for module in graph().modules():
        units = [status[logical] for logical in graph().units_of(module)]
        failed = [u for u in units if u.kind == "failed"]
        # Precedence is deliberate: a unit of this module that the compiler rejected is
        # the target's problem and outranks every inherited verdict, because it is the
        # only one that is news. Host-bound outranks blocked for the same reason in the
        # other direction -- "behind gse.win32" is a conclusion, "behind a module that
        # is behind gse.win32" is the same conclusion said twice.
        if failed:
            # Rejected over total rather than a bare FAILED: gse.graphics is three units
            # out of 114, and the verdict without the denominator reads as a module that
            # did not compile at all.
            verdict = f"FAILED {len(failed)}/{len(units)} units"
            detail = graph().path_of[failed[0].cause]
        elif any(u.kind == HOST for u in units):
            # The derived chain where there is one, and the naming cause where there is
            # not: a module held up only by an import the engine does not declare has no
            # chain to a wrapper under Source/External, and `host-bound` with nothing
            # beside it is the row a reader cannot act on.
            chain = graph().host_chain(module)
            host = next(u for u in units if u.kind == HOST)
            verdict = "host-bound"
            detail = " -> ".join(chain) if len(chain) > 1 else f"via {host.cause}"
        elif any(u.kind == BLOCKED for u in units):
            blocked = next(u for u in units if u.kind == BLOCKED)
            verdict = f"blocked {sum(1 for u in units if u.kind == BLOCKED)}/{len(units)}"
            detail = f"on {blocked.cause}"
        else:
            verdict = "translates"
            detail = f"{len(units)} units"
        tally[verdict.split()[0]] += 1
        print(f"  {module:28} {verdict:22} {detail}")

    # The number this sweep is actually for. A host-bound module was never offered to
    # the compiler, because a module it imports has no BMI -- so the host-bound count is
    # the target's arithmetic rather than a measure of the engine.
    #
    # `sole` is the column to read, and in this engine it is zero for every root: no
    # wrapper is the only thing in front of any module, so "port this one and N become
    # measurable" answers zero every time and the groups below are the actionable form.
    print("\n== host frontier ==")
    print(f"  {'root':28} {'sole':>5}  {'reached':>7}")
    for root, reached, sole in graph().frontier():
        print(f"  {root:28} {len(sole):5}  {len(reached):7}")

    print("\n== what has to be ported together ==")
    for roots, modules in graph().gated_groups():
        print(f"  {len(modules):3} modules behind {', '.join(sorted(roots))}")

    print("\n== units ==")
    unit_tally = collections.Counter(u.kind for u in status.values())
    print("  " + "  ".join(f"{kind}={count}" for kind, count in sorted(unit_tally.items())))
    print("  modules: " + "  ".join(f"{kind}={count}" for kind, count in sorted(tally.items())))
    rejected = sorted(graph().path_of[logical] for logical, u in status.items()
                      if u.kind == "failed")
    if rejected:
        print("\n== units the compiler rejected ==")
        for path in rejected:
            print(f"  {path}")


def link_engine(tools: Toolchain, modules: tuple, scenario: str = "",
                run_args: str = "") -> None:
    """Link MODULES and everything they reach for wasm32, and report what is missing.

    The other question the sweep cannot ask. A sweep stops at `-S` over interface units,
    so every fact about the engine's implementation units -- 110 `.cpp` files, where the
    host actually lives -- has been invisible to this port: `Os/Win32/Window.cpp` is the
    only definition of gse.os's window partition and no configuration but a Windows one
    has it, which a translation measurement cannot see and a link cannot miss.

    THE DELIVERABLE IS THE INVENTORY, not the binary. A module with no undefined symbols
    is the better outcome and is not the expected one; a complete list of what the
    compiler rejected and what the linker could not resolve is the artifact, so this
    continues past every failure the way `sweep` does rather than returning at the first
    the way `probe_status` does. Those are the two existing drivers and the difference
    between them is exactly this: a probe answers one question and a measurement answers
    all of them at once.

    No harness run, and `entry=""` at the link, when SCENARIO is empty: the engine defines
    no such symbol, there is nothing to call it with, and a module that merely links is all
    that arm can measure.

    SCENARIO is the second arm, and it is this one plus a caller. The program unit and a
    generated `entry` that calls its `main` are compiled after everything else, the link
    gets a stack the engine can run a frame on, and the module is handed to the same node
    harness every probe uses -- so a run is this inventory with two rows on the end rather
    than a second driver that could disagree with it about what was offered.
    """
    print(f"module graph from {graph().build}")
    verify_graph("their definitions would be reported as undefined symbols, or their "
                 "importers as portability gaps, with nothing to tell either from a "
                 "real one")
    flags = f"{target_cxxflags(tools, hosted=True, cxx_runtime=True)} {ENGINE_CXXFLAGS}"
    failure = build_std_module(tools, flags)
    if failure:
        raise SystemExit(f"the link cannot start: {failure}")

    # Interfaces first and all of them, then the implementation units: a plain
    # implementation unit of gse.x needs gse.x's BMI, and nothing guarantees every
    # module in the closure has one until the closure is done.
    # link_closure and not closure: an impl unit imports partitions no interface does,
    # and an interface absent from this list is linked against whatever a previous run
    # left in the shared module cache. See that function -- the first run of this arm
    # paid for the distinction in sixteen CRC-mismatch rejections.
    interfaces, seen = [], set()
    for path in (p for module in modules for p in graph().link_closure(module)):
        if path not in seen:
            seen.add(path)
            interfaces.append(path)
    impls, seen = [], set()
    for path in (p for module in modules for p in graph().impls_of(module)):
        if path not in seen:
            seen.add(path)
            impls.append(path)

    print(f"\n== link {', '.join(modules)}: {len(interfaces)} interface units, "
          f"{len(impls)} implementation units"
          + (f", {PROGRAM_UNIT}, the generated entry" if scenario else "")
          + " ==")
    # The program unit after every repo file, because it imports modules and nothing
    # imports it; the generated entry after even that. The entry is written here rather
    # than kept in the tree because it is a property of this measurement -- which
    # scenario, called how -- and a file under Sandbox/ would be a second place the
    # Sandbox's entry point lives.
    program = [PROGRAM_UNIT] if scenario else []
    if scenario:
        with open(tools.gcc_dir / "link-entry.cc", "w",
                  encoding="utf-8", newline="\n") as f:
            f.write((SCENARIO_ENTRY % {"scenario": scenario}).lstrip())
    offered = [(path, tools.near(REPO_ROOT / path))
               for path in interfaces + impls + program]
    if scenario:
        offered.append(("link-entry.cc", "link-entry.cc"))

    objects, rejected = [], []
    for index, (path, source) in enumerate(offered, start=1):
        stem = f"link-{index:04d}"
        if compile_unit(tools, flags, stem, source):
            objects.append(f"{stem}.o")
            kind = "ok"
        else:
            rejected.append(path)
            kind = "failed"
        print(f"  [{index:3}/{len(offered)}] {kind:7} {path}")

    cd = f"cd {to_msys(tools.gcc_dir)} && "
    assembled, unassembled = [], []
    for obj in objects:
        if bash(tools.msys_root, cd + f"llvm-mc -triple={ASM_TRIPLE} "
                                      f"-mattr={ASM_FEATURES} -filetype=obj "
                                      f"{obj[:-2]}.s -o {obj}", check=False).returncode == 0:
            assembled.append(obj)
        else:
            unassembled.append(obj)

    # Captured rather than streamed, which every other command here is: the linker's
    # stderr IS the measurement on this arm, and a list of a thousand undefined symbols
    # read off a scrolled console is not an inventory. Streamed too, by tee, so a run
    # that dies before the parse still leaves the evidence in the log.
    # `set -o pipefail` is load-bearing and not a habit: without it the status of a
    # pipeline is tee's, which succeeds whatever the linker did, and the verdict below
    # would read a clean parse of an unexamined failure as a link that worked.
    log = "link-engine.log"
    binary = "scenario.wasm" if scenario else "engine.wasm"
    linked = bash(tools.msys_root,
                  cd + "set -o pipefail && "
                  + wasm_ld(tools, assembled, binary, hosted=True,
                            cxx_runtime=True, threads=True,
                            entry=ENTRY if scenario else "",
                            stack_size=ENGINE_STACK_SIZE if scenario else 0)
                  + f" 2>&1 | tee {log}",
                  check=False).returncode
    lines = (tools.gcc_dir / log).read_text(encoding="utf-8", errors="replace").splitlines()
    undefined = sorted({line.split("undefined symbol:", 1)[1].strip()
                        for line in lines if "undefined symbol:" in line})
    # Every other error, counted separately and not folded in. An inventory that
    # presented a duplicate symbol or a feature mismatch as a missing definition would
    # send the next task to write a body that already exists.
    other = [line for line in lines
             if "error:" in line and "undefined symbol:" not in line]

    print("\n== link inventory ==")
    print(f"  units offered   {len(offered)}")
    print(f"  compiled        {len(objects)}")
    print(f"  rejected        {len(rejected)}")
    print(f"  assembled       {len(assembled)}")
    print(f"  not assembled   {len(unassembled)}")
    print(f"  undefined       {len(undefined)}")
    print(f"  other errors    {len(other)}")
    print(f"  wasm-ld exit    {linked}")
    # In the inventory and not only in `module_graph.py --impls`, because a `.cpp` that
    # declares no module is one no module's link enumerates, and this list is what a
    # reader checks a missing definition against. `Diag/AllocTrackGlobal.cpp` replaces
    # global operator new; its absence is a fact about this link, so it belongs beside
    # the other facts rather than in a second tool nobody runs next to this one.
    if graph().undeclared_impls:
        print(f"\n== {len(graph().undeclared_impls)} .cpp files declare no module, so "
              f"no module's link enumerates them ==")
        for path in graph().undeclared_impls:
            print(f"  {path}")
    if rejected:
        print("\n== units the compiler rejected ==")
        for path in rejected:
            print(f"  {path}")
    if undefined:
        print("\n== undefined symbols ==")
        for symbol in undefined:
            print(f"  {symbol}")
    if other:
        print("\n== other linker errors ==")
        for line in other:
            print(f"  {line}")
    # The linker's own status and not an empty inventory, for the reason the pipefail
    # above exists: "nothing matched my two patterns" is a statement about the patterns.
    if linked == 0:
        print(f"\n{', '.join(modules)} linked: {tools.gcc_dir / binary}"
              + (f", with {len(rejected)} units missing from it" if rejected else ""))
    if scenario:
        run_scenario(tools, binary, scenario, linked == 0 and not rejected, run_args)


def scenario_environment(run_args: str = "") -> dict:
    """What `gse.config` resolves the engine's paths from on this target.

    Every name here is one `resolve()` already consults -- four of them predate this arm
    entirely -- so the browser run is configured through the seam the engine has rather
    than through a second one invented for it. The web `executable_path()` reads the
    fifth, because a module has no argv[0] and nothing else can tell it where it is.

    The executable reported is the NATIVE binary's path, found by its manifest rather than
    by a configuration name spelled here. Nothing opens it: `resolve()` walks its parent
    for `gse.manifest` and takes its stem, so naming the real binary is what keeps the
    stem, the log name and the build root identical to the native run instead of a second
    spelling of them. That manifest is `mode = dev`, which puts the baked resources at
    `<build>/Engine/Resources` -- the same bytes the native run reads, and the only reason
    a hash comparison between the two means anything.

    GSE_ROOT_DIR is why the manifest's own `root` can be left alone: it is written by the
    native build as a Windows absolute path, which is not reachable through a preopen, and
    editing a generated file to say otherwise would be a fact about this run stored where
    the next native build overwrites it.

    The writable roots are real directories under the preopen, not stubs. `gse.save` and
    `layout_store` write there, and an unsupported report would stop the run short of the
    hash it exists to produce.

    GSE_RUN_ARGS is the one name here the engine itself never reads: the generated entry
    does, and it is carried in the environment for the reason the others are -- it is
    already forwarded to the main and the worker arms, so a run's argv arrives by the
    same route as its paths rather than through a second channel.
    """
    manifests = sorted((REPO_ROOT / "out" / "build").glob("*/gse.manifest"),
                       key=lambda p: p.stat().st_mtime, reverse=True)
    if not manifests:
        raise SystemExit("no gse.manifest under out/build: the wasm run resolves its "
                         "engine paths from the native build tree, so build natively "
                         "first")
    def guest(path: Path) -> str:
        return "/" + path.relative_to(REPO_ROOT).as_posix()

    scratch = REPO_ROOT / ".gcc-wasm-run"
    for name in ("user", "state", "projects"):
        (scratch / name).mkdir(parents=True, exist_ok=True)
    return {
        "GSE_RUN_ARGS": run_args,
        "GSE_EXECUTABLE_PATH": guest(manifests[0].parent / NATIVE_PROGRAM),
        "GSE_ROOT_DIR": "/",
        "GSE_PROJECT_DIR": "/Sandbox",
        "GSE_USER_DIR": guest(scratch / "user"),
        "GSE_STATE_DIR": guest(scratch / "state"),
        "GSE_PROJECTS_DIR": guest(scratch / "projects"),
    }


def run_scenario(tools: Toolchain, binary: str, scenario: str, complete: bool,
                 run_args: str = "") -> None:
    """Run the linked module under node, and report what its console carried.

    COMPLETE says whether the link was whole. A module missing a unit still runs, and
    the run is still worth having -- it is how a trap site gets named -- but its output
    is not evidence about the scenario, so the distinction is printed rather than
    inferred from a hash's absence.

    The two patterns are `parity_gate.py`'s own, imported rather than restated: the hash
    line is the contract that gate greps for, and a run reported as passing against a
    pattern only this file knows would be a criterion met against nothing.

    A timeout and not a wait, because the one thing a first run of this engine can do
    that neither the compiler nor the linker can is not finish: a job pool whose threads
    the harness never spawned blocks in pthread_join with no output and no exit. The log
    is tee'd for the same reason the link's is -- a run killed at the limit still has to
    leave its console behind.
    """
    log = "run-scenario.log"
    environment = scenario_environment(run_args)
    # The repo root is preopened rather than the asset directory alone: the scenario
    # names a scene and nothing here knows yet which paths the engine resolves on this
    # target, so the measurement wanted is which path it asks for, not whether a guess
    # at one directory was right.
    code = bash(tools.msys_root,
                f"cd {to_msys(tools.gcc_dir)} && set -o pipefail && "
                f"timeout -k 5 {SCENARIO_TIMEOUT_SECONDS} "
                f"node --no-warnings harness.mjs {binary} 0 shared "
                f"{to_msys(REPO_ROOT)} {shlex.quote(json.dumps(environment))} "
                f"2>&1 | tee {log}",
                check=False).returncode
    console = (tools.gcc_dir / log).read_text(encoding="utf-8", errors="replace")
    hashes = hash_pattern.findall(console)
    p50s = p50_pattern.findall(console)

    print("\n== scenario run ==")
    print(f"  scenario        {scenario}")
    for name, value in environment.items():
        print(f"  {name:<20} {value}")
    print(f"  link complete   {complete}")
    print(f"  node exit       {code}" + {
        0: " (ran, entry returned 0)",
        TRAP_EXIT: " (trapped)",
        NO_INSTANCE_EXIT: " (did not instantiate)",
        124: f" (no exit within {SCENARIO_TIMEOUT_SECONDS}s)",
    }.get(code, " (ran, entry returned non-zero or node failed)"))
    print(f"  console lines   {len(console.splitlines())}")
    print(f"  world-state hash {hashes[-1] if hashes else '(none printed)'}")
    print(f"  p50             {p50s[-1] + ' ms' if p50s else '(none printed)'}")
    print(f"  console         {tools.gcc_dir / log}")


def build_stamp(build: Path, src: Path, prefix: Path, msys_root: Path,
                sysroot: Path) -> tuple:
    """A build directory's stamp file and the inputs it has to carry.

    What it was produced from is the configure arguments *and the source tree*. The tree
    is the one input a snapshot bump changes: there is a tree per snapshot and one build
    dir, configure bakes an srcdir into its Makefiles, and nothing in configure_args
    names it -- so a new --snapshot against an existing build dir would keep Makefiles
    pointing at the previous tree and go on building it. That is a run which looks like
    the new snapshot and measures the old one, the failure `stale' exists to prevent,
    and it cannot be told from a successful bump by anything the build prints.

    Shared with --sweep-only for the same reason the two above are: that arm's whole
    claim to be measuring this toolchain is that these inputs still match, so it has to
    compare the string main would have written, not a second rendering of it.
    """
    return (build / ".configure-args",
            "\n".join([f"srcdir {src.name}"]
                      + configure_args(prefix, msys_root, sysroot)))


def toolchain_for(msys_root: Path, src: Path, build: Path, sysroot: Path) -> Toolchain:
    """Where this build directory's compiler, target libraries and headers ended up.

    Every path is derived from the build dir or the source tree and none of them depend
    on the build having just happened, which is what lets --sweep-only construct the
    same toolchain the full run does without re-walking it.
    """
    return Toolchain(msys_root=msys_root, gcc_dir=build / "gcc",
                     libgcc_dir=build / TARGET / "libgcc",
                     sysroot=sysroot,
                     libstdcxx_dir=build / TARGET / "libstdc++-v3" / "src" / ".libs",
                     libstdcxx_include_dir=build / TARGET / "libstdc++-v3" / "include",
                     libsupcxx_include_dir=src / "libstdc++-v3" / "libsupc++",
                     std_module_sources=(
                         src / "libstdc++-v3" / "src" / "c++23" / "std.cc.in",
                         src / "libstdc++-v3" / "src" / "c++23" / "std-clib.cc.in"))


def verify_graph(measurement: str) -> None:
    """Refuse to measure anything if the module graph does not match the tree.

    Refused rather than reported, because neither of these is a measurement. A unit the
    dependency scan has never seen has no edges and would sweep as a leaf, passing for
    the wrong reason; a `.cppm` with no module declaration is not in the graph at all,
    and every edge into it comes back as an unresolved import or a host-bound verdict --
    a loud symptom with its cause erased. Both are step 15's stale-build-dir hazard
    pointing the other way: a run that looks like one input and measures another.

    `unscanned', not `stale': this lived inside main, where a local of that name shadowed
    the `stale' function for the whole of it and the reconfigure branch calls it. The
    UnboundLocalError waited for the first run that changed a configure argument. The
    name stays because the hazard follows the code.

    ONE FUNCTION, TWO ARMS, and the second one is why it is a function. These refusals
    were `run_sweep`'s, so `--link-engine` did not inherit them, and they bind a link with
    more force than a sweep rather than less: a `.cppm` the scan has not seen is absent
    from `link_roots`, so it is never compiled, and every impl unit importing it is
    compiled against whatever BMI the shared module cache still holds. That is the
    `import 'std' has CRC mismatch` artifact this task has already paid for once -- and on
    the arm whose whole output is a list of missing definitions, it is indistinguishable
    from a real gap, which is the one way this measurement could send a later task to
    write a body that already exists.

    `measurement` names the arm in the refusal, because what the staleness would do to
    the numbers is the half of the message a reader acts on and it differs per arm.
    """
    unscanned, undeclared = graph().stale_interfaces, graph().undeclared
    if unscanned:
        raise SystemExit(f"{len(unscanned)} interface units are absent from that "
                         f"dependency scan, starting with {unscanned[0]}; "
                         f"reconfigure the native build so the graph matches "
                         f"the tree")
    if undeclared:
        raise SystemExit(f"{len(undeclared)} .cppm files declare no module and are "
                         f"not in the graph, starting with {undeclared[0]}; "
                         f"{measurement}")


def run_sweep(tools: Toolchain) -> None:
    """The sweep, behind the refusals that keep it from measuring the wrong tree."""
    print("\n== sweep ==")
    print(f"module graph from {graph().build}")
    verify_graph("the sweep would report everything importing them as host-bound")
    sweep(tools)


def write_harness(gcc_dir: Path) -> None:
    """The node harness every `run`-mode probe and the sweep's own links are driven by."""
    with open(gcc_dir / "harness.mjs", "w", encoding="utf-8", newline="\n") as f:
        f.write(harness_text())


def verify_workdir(args, msys_root: Path, workdir: Path) -> Toolchain:
    """The toolchain already in --workdir, behind every refusal that keeps it honest.

    `--sweep` exists to answer "does this configuration still work", so it walks fetch,
    patch, configure and three makes before it measures anything. That walk is ~50
    minutes against an already-finished build directory, and six consecutive attempts at
    one measurement died inside it rather than inside the sweep. Nothing in `Toolchain`
    needs the walk to have happened -- every path in it is a location, not a product of
    this run -- so this derives the same paths and hands them back.

    What such an arm cannot do is notice that the build directory was produced from other
    inputs, which is the one way it could print a number from the wrong compiler. So every
    input the full run would have re-established is checked here instead, read-only, and a
    mismatch is a refusal naming the full run as the fix. The checks cost milliseconds;
    being wrong about which compiler a measurement came from has already cost this port
    more than the walk has.

    One function and not a paragraph per arm, because these five refusals ARE the claim
    that the measurement is of the pinned compiler, and the second arm to make that claim
    -- `--link-engine` -- would otherwise make it by copying them. A copy that drifted
    would be an arm that reports confidently on a toolchain nothing describes.

    Neither arm runs the probes. They are the regression rows and belong to `--sweep`; a
    run of either reports no probe verdicts and must not be read as saying they are green.
    """
    src = workdir / f"gcc-17-{args.snapshot}"
    build = workdir / "build"
    sysroot = workdir / "sysroot"
    # Names the full run and not the flag this arm was given: both --sweep-only and
    # --link-engine reach here, and advising "re-run without --sweep-only" to someone who
    # passed --link-engine is a fix for a command they did not type.
    full_run = ("build it with `python Tools/build_gcc_wasm.py --sweep`, which walks "
                "fetch, patch, configure and the makes this arm skips")

    # The source tree, because Toolchain reads libsupc++'s headers and std.cc.in out of
    # it -- the sweep's very first act is to build the `std' module from those.
    if not (src / "gcc" / "config.gcc").exists():
        raise SystemExit(f"no patched source tree at {src}; {full_run}")

    # The compiler and both target libraries. cc1plus beside xg++ because the driver is
    # a stub without it, and the archives because the sweep links nothing but would
    # report a half-built libstdc++ as a wasm32 finding about the engine.
    for needed in (build / "gcc" / "xg++.exe", build / "gcc" / "cc1plus.exe",
                   build / TARGET / "libgcc" / "libgcc.a",
                   build / TARGET / "libstdc++-v3" / "src" / ".libs" / "libstdc++.a"):
        if not needed.exists():
            raise SystemExit(f"{needed} is missing, so this workdir holds no finished "
                             f"toolchain; {full_run}")

    stamp, inputs = build_stamp(build, src, args.prefix.resolve(), msys_root, sysroot)
    if stale(stamp, inputs):
        raise SystemExit(f"{build} was configured from other inputs than this script "
                         f"now asks for, so a sweep of it would report on a compiler "
                         f"nothing here describes; {full_run}")

    # A refusal rather than a call to fetch_sysroot: its matching path is a print, but
    # its stale path silently downloads and re-flattens a sysroot, which is the kind of
    # long walk this arm exists to not do by surprise.
    sysroot_marker, sysroot_inputs = sysroot_stamp(sysroot)
    if stale(sysroot_marker, sysroot_inputs):
        raise SystemExit(f"{sysroot} was flattened from other inputs than "
                         f"WASI_SYSROOT_VERSION {WASI_SYSROOT_VERSION} / {WASI_TARGET}; "
                         f"{full_run}")

    # The series, verified and not applied. Read-only -- a digest per patch file, no
    # shell -- and it is the one input of the four that rebuilding cannot repair: a
    # re-cut patch leaves a tree that looks patched and carries the previous revision.
    for name in patch_names(args.with_tests):
        _, patch_marker, digest = patch_stamp(src, args.patches.resolve(), name)
        if stale(patch_marker, digest):
            raise SystemExit(f"{name} is not applied to {src} in its current form; "
                             f"{full_run}")

    # Kept rather than skipped: one regex over the backend's wasm.h, and it is exactly
    # the "measured through an assembler no object was built with" failure that a run
    # skipping the build has no other guard against.
    check_asm_features(src)

    tools = toolchain_for(msys_root, src, build, sysroot)
    write_harness(tools.gcc_dir)
    return tools


def sweep_only(args, msys_root: Path, workdir: Path) -> None:
    """Sweep that toolchain, without rebuilding any part of it."""
    tools = verify_workdir(args, msys_root, workdir)
    print(f"Sweeping the toolchain in {tools.gcc_dir.parent} "
          f"(built elsewhere; probes not run)")
    run_sweep(tools)
    print(f"\nCompiler: {tools.gcc_dir / 'xg++.exe'}")


def main() -> None:
    p = argparse.ArgumentParser(description="Build an experimental wasm32 GCC cross compiler")
    p.add_argument("--snapshot", default=DEFAULT_SNAPSHOT,
                   help=f"GCC 17 weekly snapshot date (default {DEFAULT_SNAPSHOT}, "
                        f"what the engine's own toolchain is built from)")
    p.add_argument("--msys-root", type=Path, default=DEFAULT_MSYS_ROOT)
    p.add_argument("--patches", type=Path, default=REPO_ROOT / "Tools" / "gcc-toolchain" / "wasm")
    p.add_argument("--src-cache", type=Path, default=REPO_ROOT / ".gcc-src-cache")
    p.add_argument("--workdir", type=Path, default=REPO_ROOT / ".gcc-wasm-build")
    p.add_argument("--prefix", type=Path, default=REPO_ROOT / ".gcc-wasm")
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--with-tests", action="store_true",
                   help="Also apply the testsuite patch (not needed to build a compiler)")
    p.add_argument("--reconfigure", action="store_true", help="Discard the build dir and configure again")
    p.add_argument("--sweep", action="store_true",
                   help="After the probes, compile every engine interface unit for "
                        "wasm32 and report which modules translate")
    p.add_argument("--link-engine", nargs="*", metavar="MODULE", default=None,
                   help="Compile and link the given engine modules' closures, impl "
                        "units included, against the toolchain already in --workdir, and "
                        "report every unit the compiler rejected and every symbol the "
                        "linker could not resolve. Defaults to "
                        f"{' '.join(LINK_SUBJECT)}; does not run the probes or the sweep")
    p.add_argument("--run-args", metavar="ARGS", default="",
                   help="Extra command-line arguments for the linked module, passed "
                        "through the run's WASI environment as GSE_RUN_ARGS and "
                        "appended to the argv its entry hands the Sandbox's main. "
                        "Space separated. Costs no relink, which is the point: the "
                        "entry reads them at run time rather than carrying them as "
                        "link inputs")
    p.add_argument("--run-scenario", metavar="NAME", default="",
                   help="Link the Sandbox's own closure too -- its program unit and a "
                        "generated entry that calls its main -- and run the named "
                        "scenario under node, reporting its console. Implies "
                        f"--link-engine over {' '.join(SCENARIO_SUBJECT)}")
    p.add_argument("--sweep-only", action="store_true",
                   help="Sweep the toolchain already built in --workdir, skipping "
                        "fetch, patch, configure and the makes. Verifies that build "
                        "directory was produced from the current inputs and refuses if "
                        "not; does not run the probes")
    args = p.parse_args()

    msys_root = args.msys_root.resolve()
    workdir = args.workdir.resolve()
    # One arm and not two: a scenario run IS the link plus a caller, and a second entry
    # point into `link_engine` is how the two would come to offer different unit lists
    # while reporting the same inventory headings. So --run-scenario selects the subject
    # and the modes, and --link-engine's own list still wins if it was given one.
    if args.run_scenario and args.link_engine is None:
        args.link_engine = []
    # Refused rather than ignored, because the arms that do not run a module have no
    # argv to extend: a command line asking for both would otherwise link, report an
    # inventory, and silently drop the flag whose whole purpose is to be observed.
    if args.run_args and not args.run_scenario:
        raise SystemExit("--run-args extends the argv of a module this run executes, so "
                         "it needs --run-scenario; a link or a sweep runs nothing")
    if args.link_engine is not None:
        if args.reconfigure:
            raise SystemExit("--link-engine and --reconfigure contradict each other: "
                             "one links what is in the build directory, the other "
                             "discards it")
        # Refused and not silently won, because the two arms report different things and
        # a run that printed one while its command line asked for both would be read as
        # the other. The same refusal --sweep-only already makes of --reconfigure.
        if args.sweep or args.sweep_only:
            raise SystemExit("--link-engine measures a link and --sweep measures "
                             "translation; ask for one per run, since each reports "
                             "numbers the other does not produce")
        tools = verify_workdir(args, msys_root, workdir)
        default = SCENARIO_SUBJECT if args.run_scenario else LINK_SUBJECT
        link_engine(tools, tuple(args.link_engine) or default,
                    scenario=args.run_scenario, run_args=args.run_args)
        print(f"\nCompiler: {tools.gcc_dir / 'xg++.exe'}")
        return
    if args.sweep_only:
        # Refused rather than ignored: --reconfigure asks for the build directory to be
        # discarded, which is the one thing this arm cannot do and still have anything
        # to sweep, and silently dropping it would be a run that did the opposite of
        # what its command line said.
        if args.reconfigure:
            raise SystemExit("--sweep-only and --reconfigure contradict each other: "
                             "one sweeps an existing build directory, the other "
                             "discards it")
        sweep_only(args, msys_root, workdir)
        return

    src = fetch_source(msys_root, workdir, args.snapshot, args.src_cache.resolve())
    apply_patches(msys_root, src, args.patches.resolve(), args.with_tests)
    check_asm_features(src)
    sysroot = fetch_sysroot(msys_root, workdir, args.src_cache.resolve())

    # Before configure, not after: configure now names llvm-mc and wasm-ld as the
    # target's assembler and linker, and it probes the assembler it was given.
    print("\n== run tools ==")
    bash(msys_root, f"pacman -S --noconfirm --needed {' '.join(RUN_TOOLS)}", msystem="MSYS")

    build = workdir / "build"
    wanted = configure_args(args.prefix.resolve(), msys_root, sysroot)
    stamp, inputs = build_stamp(build, src, args.prefix.resolve(), msys_root, sysroot)
    # Reconfigure when the arguments change, not only when asked for. A build dir left
    # by an older revision of this script keeps Makefiles that predate whatever
    # configure_args has gained since, and make cannot notice: that is how `--with-as`
    # would be silently missing from an existing tree, and the failure it produces
    # ("there is no 'as' for the target") names neither the cause nor the fix.
    if build.exists() and (args.reconfigure or stale(stamp, inputs)):
        print("\n== discarding a build dir configured from other inputs ==")
        shutil.rmtree(build)
    if not (build / "Makefile").exists():
        build.mkdir(parents=True, exist_ok=True)
        print("\n== configure ==")
        # srcdir must be RELATIVE. Configured through an absolute MSYS path, srcdir
        # becomes /c/Users/..., and gengtype.exe — a mingw binary, not an MSYS one —
        # cannot open those, so s-gtype dies on a header that is plainly there. The
        # vendored PKGBUILD sidesteps this the same way, with ../${_sourcedir}/configure.
        rel_src = os.path.relpath(src, build).replace("\\", "/")
        bash(msys_root, f"cd {to_msys(build)} && {rel_src}/configure {' '.join(wanted)}")
        record(stamp, inputs)

    print("\n== make all-gcc ==")
    bash(msys_root, f"cd {to_msys(build)} && make -O -j{args.jobs} all-gcc")

    # libgcc is the first target library this build has ever descended into, and it
    # has to exist before a probe can call one: a `long long` divide is a call to
    # __divdi3, and without libgcc that is a link error. It is also what the libstdc++
    # work sits on, so its failures belong here rather than there.
    print("\n== make all-target-libgcc ==")
    bash(msys_root, f"cd {to_msys(build)} && make -O -j{args.jobs} all-target-libgcc")

    # The step this whole configuration exists for. Its exit code is read off the
    # probes rather than acted on here: libstdc++ builds in dependency order, so a late
    # failure still leaves a usable library behind, and "which parts of it work" is a
    # question only the probes can answer. Ending the run on make's result would throw
    # that answer away before it was asked.
    print("\n== make all-target-libstdc++-v3 ==")
    if bash(msys_root, f"cd {to_msys(build)} && make -O -j{args.jobs} all-target-libstdc++-v3",
            check=False).returncode != 0:
        print("libstdc++ did not build cleanly; the hosted probes report how far it got")

    tools = toolchain_for(msys_root, src, build, sysroot)
    gcc_dir = tools.gcc_dir
    # Unconditionally, where this used to ask whether any probe carries an `expect`.
    # It always does, and a guard that cannot be false reads as a case someone
    # considered rather than one that cannot arise.
    write_harness(gcc_dir)

    print("\n== probes ==")
    results = [(probe, mode, probe_status(tools, probe, mode))
               for probe in PROBES for mode in modes_of(probe)]

    print("\n== result ==")
    failed = []
    for probe, mode, status in results:
        blocked = next((b for b in probe.blocked if b.mode == mode), None)
        # What a pass is named in this mode, which is the mode's own last stage: COMPILE
        # stops at the compiler and so passes as `compiled', never as `ran'. Reading
        # "ran" as the only pass reported a *blocked compile-mode row that had started
        # passing* as "FAILED: compiled, not the ... it is blocked on" -- which is how
        # the snapshot bump's two green engine rows first presented, a fixed gap wearing
        # a regression's clothes. The loudest case the table has was the broken one.
        passing = "compiled" if mode == COMPILE else "ran"
        if blocked and status == passing:
            note = f"NOW PASSES - drop its `blocked` entry ({blocked.reason})"
        elif blocked and status == blocked.status:
            note = f"expected red: {status} - {blocked.reason}"
        elif blocked:
            note = f"FAILED: {status}, not the {blocked.status} it is blocked on"
            failed.append(f"{probe.name}/{mode}")
        elif status == passing:
            note = status.upper()
        else:
            note = f"FAILED: {status}"
            failed.append(f"{probe.name}/{mode}")
        print(f"  {probe.name:18} {mode:9} {note}")

    # After the probes rather than instead of them: the probes are the regression rows,
    # and a sweep is only readable once they are green. They share the directory and so
    # the module cache, which is incidental and not a saving -- the sweep offers every
    # unit to the compiler itself, in its own dependency order, and so never consumes a
    # .gcm a probe built under different flags.
    if args.sweep:
        run_sweep(tools)

    print(f"\nCompiler: {gcc_dir / 'xg++.exe'}")
    if f"{PROBES[0].name}/{COMPILE}" in failed:
        raise SystemExit("the baseline C++ probe failed; the compiler is not usable yet")
    if failed:
        raise SystemExit(f"probes failed: {', '.join(failed)}")


if __name__ == "__main__":
    main()
