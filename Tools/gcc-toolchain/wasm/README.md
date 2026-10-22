# Vendored GCC WebAssembly backend patches

The wasm backend is **not in GCC trunk**. It is an out-of-tree series under review, so
building a `wasm32` compiler means applying it to a trunk snapshot. These are the patches
`Tools/build_gcc_wasm.py` applies. Nothing here affects the toolchain the engine is built
with — see `docs/wasm_pipeline_plan.md` for why this exists.

## Provenance

| | |
|---|---|
| Series | `[PATCH v2 0/5] WIP: wasm: New backend`, gcc-patches, 2026-05-28 |
| Author | "feedable" `<feedabl3@gmail.com>` |
| Cover letter | https://gcc.gnu.org/pipermail/gcc-patches/2026-May/718469.html |
| Upstream PR | https://forge.sourceware.org/gcc/gcc-TEST/pulls/160 (open) |
| Diff source | https://forge.sourceware.org/gcc/gcc-TEST/pulls/160.diff |
| Head revision | `ac20dcd5f8c5ae858f9b2d9cdf4140c0738e5e27` (branch `gcc-wasm-2`) |
| Merge base | trunk `33ac889aa27d9917e6e7d07c282015b7c48e0440` = `r17-901-g33ac889aa27d99`, 2026-05-23 |
| Size | 84 files, 3,107 insertions, 34 deletions |
| Base snapshot | `17-20260809` — **not** the merge base; see below |

Earlier revisions, for reference: `[RFC PATCH 0/3]` 2026-05-05
([715824](https://gcc.gnu.org/pipermail/gcc-patches/2026-May/715824.html)) and
`[PATCH v1 0/4]` ([717771](https://gcc.gnu.org/pipermail/gcc-patches/2026-May/717771.html)).
**Use v2**: the RFC needed a separately-posted x86 prerequisite patch, which v2 folds in
as its `gcc/config/i386/i386.cc` hunk.

The PR is `mergeable: false` against *current* trunk.

## The base snapshot, and why it is not the merge base

`build_gcc_wasm.py` pins **`17-20260809`**, which is what the engine's own native toolchain
is built from, and not `17-20260524`, the first snapshot at or after the series' merge
base. The merge base is where the series applies most cleanly and it is the wrong pin:
engine code the engine compiles every day does not compile on it. `Hash.cppm:44` splices a
member into a runtime expression inside a `template for`, which `20260524` rejects and
`20260809` accepts — eleven weeks of frontend drift — and fourteen gse.math modules cascade
off that one error. See `docs/wasm_pipeline_plan.md` "Step 15".

Two things make that affordable, and both are properties of the series rather than luck:

- **Most of our patches cannot reject on a snapshot bump.** `0005`–`0012` edit files
  `0001` *creates*, and `0014` a file `0009` does, so those bytes are the same on any
  snapshot. The drift-exposed surface
  is **16 hunks in 14 files**, counted off the patches rather than estimated: `0001`
  contributes 11 (in `gcc/config.gcc` twice, `gcc/config/i386/i386.cc`, `df-scan.cc`,
  `df-problems.cc`, `expr.cc`, `function.cc`, `target.def`, `doc/tm.texi{,.in}` and
  `libgcc/config.host`), `0006` three (a second `gcc/config.gcc` hunk,
  `libstdc++-v3/crossconfig.m4`, generated `libstdc++-v3/configure`), `0008` one
  (`src/c++23/print.cc`) and `0013` one (`gcc/cp/module.cc`). Fourteen rather than
  fifteen files because `gcc/config.gcc` is in two of them. All additions with full
  context, which is why every one has so far landed on an offset rather than rejecting.
- **A reject is repaired by adding a patch, never by editing one.** `apply_patches` keys
  its stamp on a patch's bytes and refuses an edited one; see "Updating".

**Not the newest snapshot**, deliberately: it would add nine more weeks of drift, and it
would make every red engine row ambiguous between a target gap and a trunk regression the
engine has never seen.

## The patches

`0001` and `0003` are upstream's bytes, split from the single forge diff on one line —
whether a file is under `gcc/testsuite/`. Everything else is ours.
Keeping them separate is what makes a re-sync with upstream mechanical, and makes our delta
reviewable and offerable back:

- **`0001-wasm-New-backend.patch`** (23 files, upstream) — the backend itself. 12 new
  files under `gcc/config/wasm/`, a new `libgcc/config/wasm/t-wasm`, the `config.gcc` and
  `libgcc/config.host` target stanzas, and the shared middle-end changes: `gcc/df-scan.cc`,
  `gcc/df-problems.cc`, `gcc/expr.cc`, `gcc/function.cc`, `gcc/target.def`,
  `gcc/doc/tm.texi{,.in}`, `gcc/config/i386/i386.cc`. This is all `make all-gcc` needs.
- **`0002-local-Windows-host-portability.patch`** (2 files, ours) — see below. Applied
  always; without it the backend does not compile on a Windows host.
- **`0003-wasm-Adjust-tests.patch`** (61 files, upstream) — `dg-skip`/`dg-require`
  adjustments to existing tests. Not applied by default (`--with-tests` opts in).
  Testsuite files churn constantly upstream, so these are the hunks most likely to reject
  on a newer snapshot, and letting them fail the whole apply would block a build that does
  not need them.
- **`0005-local-emit-llvm-mc-s-assembly-dialect.patch`** (5 files, ours) — see below.
  Applied always. It replaces the annotated wat the backend emits with LLVM's assembly
  dialect, which is what closes the linking blocker, and emits the `.init_array` sections
  that make static constructors run.
- **`0006-local-sysroot-and-libstdcxx-host.patch`** (4 files, ours) — see below. Applied
  always. It adds the `crossconfig.m4` arm without which libstdc++ refuses the target,
  points the target's system header directory at where wasi-libc actually puts it, and
  drops the `-stdlib=libc++` the backend forced on every C++ compile.
- **`0007-local-one-only-and-aliases.patch`** (2 files, ours) — see below. Applied always.
  It gives the target link-once semantics and symbol aliases, which is what libstdc++ asks
  of a C++ target and what the series had neither of.
- **`0009-local-terminating-unwinder.patch`** (2 files, ours) — see below. Applied always.
  It gives libgcc the four `_Unwind_*` symbols a hosted libstdc++ refers to from any
  link at all, each of them `abort ()`, which is what lets a program that does not throw
  run at all.
- **`0010-local-weak-references.patch`** (1 file, ours) — see below. Applied always. It
  emits `.weak` for a symbol declared weak that nothing defines, which is what
  `__attribute__((weak))` on an extern means and what libstdc++ assumes of any target
  setting `__GXX_WEAK__`.
- **`0011-local-multi3.patch`** (2 files, ours) — see below. Applied always. It gives
  libgcc a hand-written `__multi3`, which this compiler never calls and every
  clang-built wasm library expects, and which was the one symbol standing between a
  clang-built sysroot and any hosted C++ link at all.
- **`0012-local-host-wide-int-operands.patch`** (1 file, ours) — see below. Applied
  always. One `fprintf` format: an operand's `CONST_INT` was printed with `"%ld"`, which
  on an LLP64 host reads half of a `HOST_WIDE_INT`, so every 64-bit constant too large
  for 32 bits assembled as a different number.
- **`0013-local-module-initializer-decl.patch`** (1 file, ours) — see below. Applied
  always. The only one of ours in the C++ front end — `0006` and `0008` already reach
  outside the backend, into GCC's and libstdc++'s build and library files. The front end
  synthesizes the decl for an imported module's global initializer without
  `DECL_EXTERNAL`, so the call it then emits never gets its callee declared.
- **`0014-local-personality-accessors.patch`** (1 file, ours) — see below. Applied always.
  It adds the three `_Unwind_Get*` accessors `0009` deliberately left out, each of them
  `abort ()`, because `eh_personality.o` turns out to be pulled into a hosted link after
  all and refers to all three.
- **`0015-local-native-tls.patch`** (3 files, ours) — see below. Applied always. It
  gives the target native thread-local storage instead of emulated TLS, which is what
  lets GCC-compiled code see a thread-local that a clang-built library defines —
  wasi-libc's `errno` being the one that matters. It also turns the atomics and
  bulk-memory features on, in `ASM_SPEC` and in a `target_features` section, because
  `wasm-ld` refuses thread-local storage without them.
- **`0016-local-atomics.patch`** (2 files, ours) — see below. Applied always. It adds the
  atomic memory instructions, without which every `__atomic` builtin expanded into a
  libatomic libcall that has no wasm32 port. Inseparable from `0015` in practice, and it
  is what `--enable-threads=posix` costs: libstdc++ drops
  `__gnu_cxx::__atomic_add_single` for those builtins the moment a gthread model exists.
- **`0017-local-gthr-no-weak-refs.patch`** (1 file, ours) — see below. Applied always. It
  turns the `__gthrw` weak references off for this target, because wasi-libc keeps every
  pthread_* API in libc — its `libpthread.a` is an empty archive — and because a weakref
  alias is referenced without GCC declaring the target external, so its calls reach the
  assembler with no `.functype`. **This alone fixes the libgcc build.** A separate patch
  for wasi-libc's missing `pthread_cancel` was written first and then deleted as dead:
  both of its hunks sit inside regions this one switches off. Its header records that,
  for whoever re-enables `GTHREAD_USE_WEAK`.
- **`0018-local-called-function-decls-and-muloti4.patch`** (3 files, ours) — see below.
  Applied always. Two unrelated link blockers that wanted the same rebuild. It declares a
  function named in the instruction stream, which is the third instance of the class
  `0013` and `0017` each met once — a call reaching the assembler with no `.functype` —
  and the one that stood between this series and a linked engine module. It also gives
  libgcc a hand-written `__muloti4`, on exactly `0011`'s terms: wasi-libc's
  `clock_nanosleep.c` calls it, so it is undefined in any link whose program can sleep.
- **`0019-local-noop-coroutine-frame-signature.patch`** (1 file, ours) — see below.
  Applied always. The second of ours outside the backend and the first in libstdc++'s own
  headers. `std::noop_coroutine()`'s static frame declares its resume and destroy slots
  `void (*)()`, while generated code calls them the way it calls any coroutine's —
  `frame->__r (frame)`. wasm type-checks `call_indirect` against the callee's declared
  signature at run time, so there the extra argument is not ignored and every coroutine
  that reaches final suspend with no continuation traps. The slots are pointers either
  way, so the frame's layout is unchanged and nothing moves on any other target.
- **`0004-local-emit-rwat-s-annotation-dialect.patch`** (2 files, ours) — **superseded by
  `0005` and no longer applied.** Kept as the record of the path not taken. The two are
  mutually exclusive: both rewrite the same templates.

## External tools: what assembles and runs the output

The backend emits assembly, and `0005` decides whose. Upstream emits WebAssembly **text**
annotated for a relocatable assembler it does not ship with; with `0005` it emits LLVM's
assembly dialect, for `llvm-mc`.

`wasm.h` used to declare `ASM_SPEC "-r --enable-annotations"`, which are
[wat2wasm](https://webassembly.github.io/wabt/doc/wat2wasm.1.html)'s flags — except that
current wabt (1.0.42) has no `--enable-annotations` option at all, and no version of it
knows `(@sym …)` or `(@reloc …)`. Those are
[**rwat**](https://github.com/Spxg/rwat)'s vocabulary ("reloc wat": parse annotated wat
into a relocatable wasm object). So the spec named one tool while the output targeted
another — and the one it targeted could not place a static or form a function pointer:
rwat implements no `MEMORY_ADDR` and no `TABLE_INDEX` relocation and has no data or global
symbol kind at all. The six-row divergence table that used to live here is in this file's
git history, and the measurements behind it are steps 4 and 5 of
`docs/wasm_pipeline_plan.md`. Nothing in the pipeline reads it any more.

`build_gcc_wasm.py` now assembles with `llvm-mc` and links with `wasm-ld`, and every probe
— including `static`, `funcptr` and a two-TU internal-symbol collision — links and runs.
Three workarounds the rwat path needed are gone with it: the `(@rwat)` module-header shim,
the plain `(export …)` appended because the backend expressed the export only as an
annotation, and `--no-gc-sections`, which existed only because GCC's imported
`__stack_pointer` carried no symbol. Declared with `.globaltype` it has one, so section
collection is back on by default.

To read back what a probe produced:

```sh
cd .gcc-wasm-build/build/gcc
wasm-objdump -x probe-static.o | grep -E 'MEMORY_ADDR|D <'  # data symbols and their relocs
wasm-objdump -x probe-two_tu.o | grep binding=              # local binding, a withheld .globl
wasm-objdump -x probe-init_array.o | grep -A2 'init func'   # a constructor the linker will call
wasm-objdump -x probe-libc.wasm | grep wasi_snapshot        # the WASI imports node then hosts
```

What the dialect does and does not carry is measured separately by
`Tools/wasm_dialect_probe.py`, on hand-written subjects that need no GCC build. Eight of
its thirteen settle questions a port had to answer first: a `loop` can carry a result type
and a branch can name its target by depth (`blocktype`), a function can be imported from
`env` with `.import_module`/`.import_name` (`env_import`), a `.init_array.<prio>` section
becomes an init function the linker collects (`init_array`), a section can carry a comdat
group the linker deduplicates (`comdat`), `.set` can alias a function and a data symbol
(`alias`), a `.weak` symbol nothing defines survives the link unresolved
(`weak_undef`, which is what `0010` emits), a tag can be defined and a `try_table` can
catch what a `throw` raises — behind `-mattr=+exception-handling`, which is itself the
finding (`eh_instructions`, the question a future EH step would otherwise guess at) — and
a signature **cannot** be declared after the call that needs it
(`late_functype`, the one deliberately red subject). That last one is why
`wasm_asm_file_start` buffers the body at all: every `.functype` the unit needs has to be
flushed into the module header ahead of it.

`comdat` carries a second fact in its assertion rather than in prose, because it is the
one that bites: **plain `.text` does not end a comdat group.** A function emitted after a
one-only one joins its group, and the linker discards it along with the duplicate copy —
reported as `undefined symbol` for a symbol the object plainly defines. Hence every
symbol, one-only or not, getting a section of its own.

## The target libc and the target libraries

The build no longer stops at the compiler. **`libgcc` is built for `wasm32`** against a
**wasi-libc** sysroot `build_gcc_wasm.py` downloads and flattens (pinned by
`WASI_SYSROOT_VERSION`), and **`libstdc++.a` archives** — 199 members, all of `libsupc++`
and every `src/` object, the last two of which `0008` closed. See
`docs/wasm_pipeline_plan.md` "Step 10" for what those two were.
Things worth knowing, each of which cost time:

- **A target library needs the whole toolchain named.** A probe stops at `-S` and is
  handed to `llvm-mc` by the script, so the driver never looks for an assembler. A target
  library compiles to an object *through the driver*, which finds the build tree's
  `gcc/as` exec-tool shim before it consults PATH — and that shim refuses with
  `$original is unset` unless top-level configure was told which assembler the target has.
  `--with-as` and `--with-ld` are that; `AR_FOR_TARGET`, `RANLIB_FOR_TARGET` and
  `NM_FOR_TARGET` are the same problem one step on, since there is no binutils for this
  target and `libgcc.a` otherwise dies on `wasm32-unknown-none-ar: command not found`.
  Between them they are the whole reason `all-target-libgcc` had never been run.
- **`--disable-gcov` is not optional.** libgcov wraps `fork` and the six `exec`
  functions, and compiles those wrappers whenever a libc is present at all; wasi-libc has
  no process primitives. It cannot be dropped from a `t-` fragment either —
  `libgcc/Makefile.in` includes `tmake_file` at `:382` and assigns `LIBGCOV_INTERFACE` at
  `:923`, so the assignment wins.
- **Every cached artifact records what it was built from**, because presence on disk
  cannot say which inputs produced it and all three of the expensive ones have an
  input that is load-bearing and easy to change: the source tree (the bytes of our
  patches), the sysroot (`WASI_SYSROOT_VERSION`) and the build directory (the
  configure arguments). Changing one without the check leaves something that looks
  finished and measures the previous input. The sysroot and build directory rebuild
  themselves; a re-cut patch **refuses** and tells you to delete the source tree,
  because re-applying an edited patch to an already-patched tree fails in a way that
  reads like a rejected hunk.
- **wasi-libc's headers are not C++-clean**, and which header breaks depends on the
  release — they are only as clean as the clang they were last tested against. 27.0 has
  bare `_Static_assert (_Alignof (…))` in `wasi/api.h`; 32.0 and 34.0 have
  `extern _Thread_local int errno`. Hence the pin, plus `-D_WASI_EMULATED_SIGNAL` in
  `CXXFLAGS_FOR_TARGET`.
- **`errno` is declared in the flattened headers the way libc defines it**, which is the
  second sysroot edit and in the sysroot's cache key beside the first. `__errno.h` says
  `extern _Thread_local int errno` and `libc.a(errno.c.obj)` answers `00000000 D errno` —
  ordinary data, because a non-threads wasi-libc built by clang has no TLS in it either.
  Mapping the keyword to `__thread`, which is what `CXXFLAGS_FOR_TARGET` used to do, took
  that mismatch and made GCC ask for an `__emutls_v.errno` control object from the
  definition side; nothing defined it, and that was the last blocker on a *running* engine
  module. `plain_errno` rewrites the declaration instead. The `-D` is still in
  `CXXFLAGS_FOR_TARGET` and is now inert — it is part of the build directory's cache key,
  so removing it costs an `all-gcc` rebuild to change nothing measured, and the next
  reconfigure is where it goes.
- **The flattened `setjmp.h` is edited, and that edit is in the sysroot's cache key.**
  wasi-libc will not *declare* `setjmp` without the wasm exception-handling proposal — the
  header is a bare `#error` — and libstdc++ precompiles `<csetjmp>` into `stdc++.h.gch`
  while `src/c++23/std.cc.in` is `#include <bits/stdc++.h>`. So one function nothing in
  libstdc++ calls stopped both the library and the `std` module. `declare_setjmp` removes
  the guard, leaving `setjmp` declared and undefined, which is the literal truth for this
  sysroot: there is no `setjmp.o` in it, so code that really calls one fails at link naming
  the symbol. `__wasm_exception_handling__` is deliberately **not** defined — that would be
  a claim about a capability the backend does not have, and it would buy a module that
  silently does not unwind. The guard's exact shape is required, not optional: a release
  that words it differently fails there and gets looked at, instead of quietly keeping an
  `#error` the caller reads as the edit having worked.
- **The sysroot is flattened on purpose.** The release tarball is multi-target
  (`include/wasm32-wasip1`, `lib/wasm32-wasip1`, plus wasip2, wasip3 and a threads
  variant); GCC wants one include directory and one library directory. Headers land in
  `<sysroot>/include`, named by `0006`'s `NATIVE_SYSTEM_HEADER_DIR` and **not** by a
  configure argument (see below for why that does not work here), and libraries in
  `<sysroot>/lib/wasm32-wasi`, which is **not** a tidier spelling of the same thing —
  `wasm.h`'s `STANDARD_STARTFILE_PREFIX_1` is `/lib/wasm32-wasi/`, so it is where the
  driver looks for `crt1.o`.
- **The hosted link line is in dependency order, and that order is load-bearing.**
  `wasm-ld` pulls archive members lazily and scans left to right, so each archive has
  to appear before everything it needs: libstdc++, libsupc++, libgcc, libc. Getting it
  backwards costs nothing until a library is actually there to be pulled from, and then
  surfaces as a `link failed` on a probe whose known red is a compile error — a
  tripwire firing on the wrong cause. `build_gcc_wasm.py` assembles the whole line in
  one place for that reason rather than appending to it per-feature.
- **Hosted probes link `crt1-reactor.o`, not `crt1.o`.** The command form defines `_start`
  and expects to be the whole program; a reactor exposes `_initialize` and stays callable,
  which is what a probe whose `entry` the harness calls needs. The harness runs such a
  module under node's built-in `node:wasi` and calls `wasi.initialize`, which runs
  `__wasm_call_ctors` itself — so the two constructor paths in the harness are
  alternatives, not a sequence.

## Updating

Re-download the diff, re-split it on the same rule, and update the table above — in
particular the head revision and the base snapshot the series was rebased onto:

```sh
curl -fsSL -o wasm.diff https://forge.sourceware.org/gcc/gcc-TEST/pulls/160.diff
```

**`forge.sourceware.org` is behind Anubis now too** — measured 2026-10-16, on both
`/gcc/gcc-TEST/pulls/160` and `/160.diff`, each of which answers with an access-denied page
rather than the content. So is `inbox.sourceware.org` and so is `gcc.gnu.org/wiki`, as
before. `gcc.gnu.org/pipermail/` and `gcc.gnu.org/pub/gcc/snapshots/` still answer
normally. Provenance here still points at the forge because that is where the series
*is*, not because it can be fetched: re-downloading the diff now means a browser, and
until someone does, the vendored `0001` is the series.

To re-cut one of **our** patches after editing the backend in place, diff against a
snapshot of the tree as it was with only the patches *before* it applied. `0005` is
against `0001`+`0002`, so `baseline/wasm` is taken straight after those two; `0006` is
against `0005` as well, so it has its own snapshot, `baseline-0005/wasm`; `0007` is
against all four, so it has `baseline-0006/wasm`.

That settles each patch's **a**-side. The **b**-side is the harder half and is why several
of these have no command below: a patch may only be cut to the current tree if no later
patch touches the same file, and most of this series' files have since acquired one.

```sh
cd .gcc-wasm-build
# 0005 has no command here either. Its a-side is baseline/wasm throughout, and of its four
# files only wasm-cg.cc may end at the tree: wasm.md is edited after it by 0016, wasm.cc by
# 0015, and wasm-asm.cc by 0007, 0008, 0010, 0012, 0015 and 0018. See below.

# 0007 has no command here any more. Its a-side is baseline-0006/wasm for both files, but
# neither b-side may end at the tree: wasm-asm.cc is edited after it by 0008, 0010, 0012,
# 0015 and 0018, and wasm.h by 0015. Both boundaries are unsnapshotted; see below.

diff -u --label a/gcc/config/wasm/wasm-asm.cc --label b/gcc/config/wasm/wasm-asm.cc \
  baseline-0008/wasm/wasm-asm.cc baseline-0011/wasm/wasm-asm.cc \
  > ../Tools/gcc-toolchain/wasm/0010-local-weak-references.patch

{ diff -u --label a/libgcc/config/wasm/multi3.c --label b/libgcc/config/wasm/multi3.c \
    /dev/null gcc-17-20260809/libgcc/config/wasm/multi3.c
  diff -u --label a/libgcc/config/wasm/t-wasm --label b/libgcc/config/wasm/t-wasm \
    baseline-0010/libgcc-wasm/t-wasm baseline-0017/libgcc-wasm/t-wasm
} > ../Tools/gcc-toolchain/wasm/0011-local-multi3.patch

# 0012 and 0015 both end mid-chain and have no snapshot at their boundary; see below.

{ diff -u --label a/gcc/config/wasm/wasm-asm.cc --label b/gcc/config/wasm/wasm-asm.cc \
    baseline-0017/wasm/wasm-asm.cc gcc-17-20260809/gcc/config/wasm/wasm-asm.cc
  diff -u --label a/libgcc/config/wasm/muloti4.c --label b/libgcc/config/wasm/muloti4.c \
    /dev/null gcc-17-20260809/libgcc/config/wasm/muloti4.c
  diff -u --label a/libgcc/config/wasm/t-wasm --label b/libgcc/config/wasm/t-wasm \
    baseline-0017/libgcc-wasm/t-wasm gcc-17-20260809/libgcc/config/wasm/t-wasm
} > ../Tools/gcc-toolchain/wasm/0018-local-called-function-decls-and-muloti4.patch
```

`0013` and `0019` are the easy case in the other direction: **no other patch of ours
touches `gcc/cp/module.cc` or `libstdc++-v3/include/std/coroutine`**, so each one's a-side
is simply that file as the snapshot ships it. Re-cut either against a fresh extract of the
tarball — neither needs a baseline, because a
baseline directory exists only for a file two of our patches edit. (There is no
`baseline-0012/`, and unlike when this was written there now *should* be: `0015` and
`0018` both edit `wasm-asm.cc` after `0012`, so `0012`'s b-side is a boundary nothing has
snapshotted. See the gap described below.)

`0014` extends `0009`'s `unwind-wasm.c`, so that file is now in two of our patches and has
a snapshot like any other: `baseline-0013/libgcc-wasm/unwind-wasm.c` is the file as `0009`
left it, and `0014`'s side is that → tree.

```sh
diff -u --label a/libgcc/config/wasm/unwind-wasm.c --label b/libgcc/config/wasm/unwind-wasm.c \
  baseline-0013/libgcc-wasm/unwind-wasm.c gcc-17-20260809/libgcc/config/wasm/unwind-wasm.c \
  > ../Tools/gcc-toolchain/wasm/0014-local-personality-accessors.patch
```

It touches `libgcc/config/wasm/t-wasm` not at all, which is the reason it extends that
file rather than adding a sibling: `unwind-wasm.c` is already built.

`0009` is the one patch here with nothing to cut against: its `unwind-wasm.c` is a new
file, so its a-side is `/dev/null`, and the libgcc `t-wasm` it edits is touched by no
other patch of ours, so `baseline-0008/libgcc-wasm/` holds `0001`'s copy of it. That
directory is **not** a snapshot of the tree — `baseline-0008/wasm/` is, taken straight
after `0008` — and the two live together only because they are the same patch
generation.

**A file touched by two of our patches must never be cut against the current tree**, or the
earlier patch swallows the later one's hunks. Three files are in that position —
`wasm.h`, `wasm-asm.cc` and `libgcc/config/wasm/t-wasm`:

| File | `0005` side | `0006` side | `0007` side | `0008`/`0010` side | later |
|---|---|---|---|---|---|
| `wasm.h` | `baseline/wasm` → `baseline-0005/wasm` | `baseline-0005` → `baseline-0006` | `baseline-0006` → no snapshot | (untouched) | `0015`: no snapshot → tree |
| `wasm-asm.cc` | `baseline/wasm` → `baseline-0006/wasm` | (untouched) | `baseline-0006` → no snapshot | `baseline-0008` → `baseline-0011/wasm` | `0012`, `0015`: no snapshot; `0018`: `baseline-0017/wasm` → tree |

`wasm-asm.cc` is in **seven** of our patches now — `0005`, `0007`, `0008`, `0010`, `0012`,
`0015`, `0018` — so no side of it may end at the tree except `0018`'s. Three boundaries
are snapshotted: `0005` ends at `baseline-0006/wasm`, `0010` at `baseline-0011/wasm`, and
`0018` is cut against `baseline-0017/wasm`. **Four are not.** `0007`, `0008`, `0012` and
`0015` were each cut while it was the last patch to touch the file, so each ends at the
tree *as it then stood*, and nothing snapshotted the boundary when the next one landed.
Re-cutting any of them by a command ending at the tree today silently swallows every later
patch's hunks — `0007`'s would absorb five patches' worth, `0012`'s two. That is why none
of the four has a command above: reconstruct the boundary first, by applying the earlier
patches to a fresh `baseline/wasm` copy. `baseline-0017` does cap the chain: it is the
file as `0017` left it, which for this file is the file as `0015` left it, so it is
`0015`'s b-side as well as `0018`'s a-side.

`wasm.h` is the same shape one patch shallower — `0005`, `0006`, `0007`, `0015`. `0005`
and `0006` end at snapshots; `0007`'s b-side does not exist, because `0015` rewrites
`ASM_SPEC` in that file after it. `0015`'s own b-side is the tree and is allowed to be,
since nothing later touches `wasm.h`.

Two more files are in the same position and are easy to miss because each has only one
later patch: **`wasm.cc`** (`0005`, then `0015`'s `TARGET_HAVE_TLS`) and **`wasm.md`**
(`0005`, then `0016`'s atomic instructions). In both, `0005`'s side ends at an
unsnapshotted boundary and `0015`/`0016`'s may end at the tree. `wasm-cg.cc` is the only
file `0005` touches that no later patch does.

**Check this mechanically rather than by eye.** Every command above is a claim that no
later patch edits the file; nothing enforces it, and four of them were silently wrong by
the time `0018` landed — `0005`, `0007`, `0010` and `0012`, between them covering five
files. Re-derive the ownership instead of trusting the prose:

```sh
cd Tools/gcc-toolchain/wasm
for f in $(grep -ho '^--- a/\S*' 0*.patch | sed 's|^--- a/||' | sort -u); do
  owners=$(grep -l "^--- a/$f\$" 0*.patch | grep -v '^000[34]' | cut -c1-4 | tr '\n' ' ')
  case "$owners" in *" "*" "*) echo "$f: $owners";; esac
done
```

Any file with more than one owner may appear with a tree b-side **only** in its last
owner's command. That is the invariant; the list above is what it currently yields.

`libgcc/config/wasm/t-wasm` is in three of our patches — `0009`, `0011`, `0018` — and is
still the easy case, because every boundary has a snapshot: `0009` writes it against
`0001`'s copy in `baseline-0008/libgcc-wasm/`, `0011` appends to it from
`baseline-0010/libgcc-wasm/` (the file as `0009` left it) to `baseline-0017/libgcc-wasm/`
(the file as `0011` left it, since nothing between `0011` and `0018` touches it), and
`0018` appends from there to the tree.

So each baseline has to be kept in step with every file the *next* patch does not touch, or
that patch's next re-cut picks up its predecessor's work. Then check the result
round-trips: apply the patches in order to a fresh copy of the baselines and `cmp`
every file against the tree they were cut from.

All four of those unsnapshotted boundaries are the same accident repeated: a patch is cut
against the tree while it is the last to touch the file, and the *next* patch to touch it
is what invalidates the command — silently, in a document, long after the fact. Taking the
snapshot belongs with adding the patch, not with re-cutting the old one. **Prefer adding a
patch to re-cutting one.** A new patch
applies cleanly to the tree already on disk and keeps every earlier stamp valid, where an
edited one makes `apply_patches` refuse and demand the source tree be deleted — which is
a full re-extract and a full rebuild of libgcc and libstdc++. That is the only reason the
weak-reference change is `0010` and not a hunk in `0008`, where it would have read more
naturally.

## Local changes: `0002-local-Windows-host-portability.patch`

Upstream develops this on Linux, so two things in it are LP64/glibc-only and stop the
backend compiling on a mingw-w64 host. Both are host-side only — neither changes a byte of
generated wasm — and both are worth sending upstream.

1. **`TARGET_ASM_INTERNAL_LABEL` signature.** `wasm_output_internal_label` is defined
   taking `size_t`, but the hook has long been declared
   `void (*) (FILE *, const char *, unsigned long)`. Those are the same type on LP64 and
   different under LLP64, so `TARGET_INITIALIZER` fails to convert and `wasm.o` does not
   build. Changed to `unsigned long` to match the hook. The sibling
   `wasm_generate_internal_label` is left alone deliberately: it is reached through the
   `ASM_GENERATE_INTERNAL_LABEL` macro, not a hook, so it is never type-checked against a
   function-pointer type.
2. **`open_memstream`.** Used to buffer the module body so declarations can be emitted
   ahead of it. It is a glibc extension and does not exist in mingw-w64's CRT. Replaced
   with `tmpfile ()` plus a `rewind`/`fread`/`fwrite` copy, which is plain C and keeps the
   same structure and output.

   That change also has to hand the real output file back —
   `asm_out_file = saved_asm_out_file` — because `toplev.cc` fcloses whatever
   `asm_out_file` points at when compilation finishes. Upstream leaves the memstream
   there, so on current upstream toplev closes the buffer and never closes the actual
   output file; it survives only because process exit flushes it.

## Local changes: `0005-local-emit-llvm-mc-s-assembly-dialect.patch`

Unlike `0002`, which fixes a plain host-portability bug, this one takes a side in an open
question: **which assembler the backend should target.** It moves GCC onto `llvm-mc`'s
dialect, measured as carrying everything rwat lacks, and it is what turns `static` and
`funcptr` green. It is not offerable upstream as-is — it is a measurement, and the probe
table is what it buys. Seven changes, across `wasm.h`, `wasm.md`, `wasm-asm.cc` and
`wasm.cc`:

1. **`ASM_SPEC` names `llvm-mc`**, with the triple spelled out: LLVM calls the OS-less
   wasm target `unknown`, GCC's `config.sub` canonicalises the same target to `none`.
   `ASM_COMMENT_START` becomes `#`, which is the comment character the wasm assembler
   actually has — `;;` was wat's, and would now be a parse error.

2. **Every insn template goes from folded to flat.** `"(%M0.set %0 (<types>.<opname>
   (%M1.get %1) (%M2.get %2)))"` becomes
   `"%i1\n\t%i2\n\t<types>.<opname>\n\t%M0.set %0"`: operands on their own lines, in
   evaluation order, ahead of the opcode that consumes them. ~35 templates, nearly all of
   that one shape, so the work is in the operand codes rather than in the templates. `%i`
   and `%o` now emit a whole push or pop instruction; `%f` is a memory operand's offset
   immediate, which moves from in front of the address to after the opcode; `%&` is a
   branch depth, `%!` the control local, `%~` the function table.

3. **Locals are indices, not names.** `.local` is positional, so `$local_7` has no
   spelling: parameters take 0..n-1 in signature order and each `.local` entry follows.
   `wasm_asm_start_function` gathers the parameter and local registers before printing
   anything and assigns indices in the same walk that declares them, which is the only way
   the two cannot drift apart. `local_of` is the single place that resolves a register, and
   it asserts rather than printing an index that would silently mean some other local.

4. **Branches name their target by depth.** `br`/`br_if` to the dispatch loop is `br N`
   where N is the number of `block`s still open, so `block_depth` is set to the full height
   by the staircase in `wasm_asm_start_function` and decremented wherever
   `wasm_output_internal_label` closes one. `br_table` takes its targets as `{0, 1, …}`.

5. **Symbols and sections are directives.** `.type`/`.section`/`.globl`/`.weak`/`.hidden`/
   `.p2align`/`.size` in place of `(@sym …)` decorations, and a relocation is just a symbol
   in the operand — `i32.const sym`, `.int32 sym` — because the assembler picks
   `MEMORY_ADDR` or `TABLE_INDEX` from the symbol's own kind. Withholding `.globl` is what
   finally gives an internal symbol local binding, which the annotated dialect could not
   express at all: rwat emitted every symbol global and strong, so two TUs defining the
   same `static` collided. `two_tu` is the probe that would catch a regression there.

6. **Code and data are buffered separately.** The assembler emits a module's sections in
   the order it meets them and dies with "only data supported in data sections" on code
   reached after a data section, while GCC emits functions and objects interleaved in
   declaration order. So an object redirects output into a second buffer for as long as it
   is open, and `wasm_asm_file_end` writes the header, then all code, then all data.

7. **A static constructor is an `.init_array.<prio>` entry.** `TARGET_ASM_CONSTRUCTOR`
   was a no-op lambda, with the priority riding on an `(init_prio N)` symbol decoration
   that no assembler ever read — so constructors did not run, which is fatal to a hosted
   libstdc++ whose `ios_base::Init` and locale tables are constructors. `wasm_asm_constructor`
   now emits `.section .init_array.<prio>,"",@` plus a `.int32` of the function, into the
   *data* buffer because it is data. The section kind is a bare `@`: `@init_array`, which
   is how the directive reads on an ELF target, is rejected by the parser outright. There
   is deliberately no matching destructor hook — wasm has no `.fini_array` and the linker
   synthesises nothing for one, so a destructor runs only by having been registered with
   `__cxa_atexit` from a constructor. `init_array` in both probe scripts covers this.

Two more bugs it fixes, both upstream's, both invisible until libgcc exercised them:

- **Float constants were spelled in wat.** `f64.const +inf`; the assembler takes
  `infinity` and rejects both `inf` and the leading `+`. libgcc's complex multiply and
  divide are the first code anywhere to emit an infinity, which is why thirteen green
  probes never reached it. It also decided that a bare `nan` is the only NaN the dialect can
  express — no sign, no `:0x<payload>` — so a non-canonical one `sorry ()`s rather than
  silently becoming a different constant, and `print_nan_payload` is deleted with the suffix
  it wrote. **Half of that was wrong and `0007` corrects it:** the sign is accepted, only
  the payload is not. It was read rather than measured, which is the one place in this
  series that happened, and it cost `std::from_chars` for a float.
- **A libcall's signature was declared backwards.** `wasm_expand_call` says in its own
  comment that `cfun->machine->call->args` is in reverse argument order, and reverses it
  when building the call PARALLEL; `record_libcall` read the same vector forwards. So
  `__powidf2 (double, int)` was declared `(i32, f64)` while the call site pushed
  `(f64, i32)`. Invisible unless a callee has two parameters of *different* types, and
  then only against an assembler that type-checks the stack. `libcall` is the probe.

**A signature is derived from a type in exactly one place**, `signature_of`, because
upstream derived it in two that disagreed and nothing had ever compared them. The import
path (`print_type` on a `FUNCTION_TYPE`) listed only the declared arguments and ran the
argument list's trailing `void` through `pass_by_reference`, which answers yes and invents
a parameter; the definition path (`print_func_type`) added the three parameters that are
not in the argument list — a leading pointer for a result returned in memory, a trailing
one to the outgoing-args block for varargs or unprototyped functions — and took the result
through `aggregate_value_p`. Only the second is right, so it is the one that survived, and
the first is gone. An external varargs callee was the case where the disagreement bit:
the header declared `(i32) -> (i32)`, the call site pushed two values, and the module
trapped. `extern_varargs` in `build_gcc_wasm.py` is the probe for it, and it is red
against the first version of this patch.

Relatedly, `result_on_stack` is what decides whether `return` leaves a value, and it comes
from the signature's own result list rather than from the return register being live. Those
two are different questions for a result returned in memory: the register holds the hidden
pointer, so the local exists while the signature has no result, and keying `return` off the
local made it push a value the signature said was not there. Harmless as it happened
(`return` is stack-polymorphic, so the extra value was discarded), but it was two facts
where there is one. `sret` is the probe.

One thing it does **not** do, visible as an absence rather than as a failure:

- **Undefined data symbols are left to the linker**, and undefined functions are declared
  but not imported. A libcall whose `libgcc` is not on the link line therefore fails at
  link time rather than becoming an `env` import. That is the louder failure and the right
  default, but `env_import` in the dialect probe is the measured recipe for the other
  behaviour when something genuinely has to come from the host.

## Local changes: `0006-local-sysroot-and-libstdcxx-host.patch`

Four edits across `gcc/config.gcc`, `gcc/config/wasm/wasm.h`,
`libstdc++-v3/crossconfig.m4` and `libstdc++-v3/configure`, all about hosting a C++
runtime the target did not have when the series was written.

1. **`CC1PLUS_SPEC "-stdlib=libc++"` is gone.** Forcing it onto every C++ compile was a
   reasonable guess while the target had no C++ runtime and is actively wrong once it has
   one: the option makes the driver look for libc++'s headers and refuse libstdc++'s. It
   also only *exists* if configure was given `--with-gxx-libcxx-include-dir`
   (`gcc/configure.ac`: `ENABLE_STDLIB_OPTION`), so without that every C++ compile through
   the driver died on an unrecognized option rather than only the selftest that surfaced
   it — which is why `build_gcc_wasm.py` used to pass a path to a directory that never had
   to exist. With the spec gone, so is that argument.

2. **The target's system header directory is `/include`,** because that is where
   wasi-libc puts the headers and GCC otherwise looks under `usr/`. It is stated **twice**,
   which is upstream's own pattern rather than an oversight — `config/mingw` does the same
   — because the two statements have different consumers and only one of them works:

   - `native_system_header_dir=/include` in **`config.gcc`**'s wasm stanza reaches the
     generated Makefile, and so `fixincludes`, whose `stmp-fixinc` rule fails outright on
     a `BUILD_SYSTEM_HEADER_DIR` that does not exist.
   - `NATIVE_SYSTEM_HEADER_DIR "/include"` in **`wasm.h`** is what the preprocessor's
     default include list is built from. The macro reaches `cppdefault.cc` as a `-D` on a
     compiler command line, and **on an MSYS2 host a configure-supplied value does not
     survive that trip**: a bare `/include` looks like a POSIX absolute path to the
     argument converter, so it arrives as `C:/…msys64/include`, the sysroot is prepended
     to *that*, and the result cannot exist. A string literal in a header never passes
     through a shell. `config/mingw/mingw32.h` `#define`s the same macro for the same
     reason, citing the same bug (PR 52947).

   Each of these was a separate failure, in that order. The first presented as `stdio.h:
   No such file or directory` with `-print-sysroot` reporting the right sysroot — and
   `xgcc -E -v` names the cause only in its *ignoring nonexistent directory* lines, which
   come before the search list and are the easy part of that output to cut off.

3. **`crossconfig.m4` gains a `wasm*-*-*` arm**, which is the change that lets libstdc++
   be configured at all. Its target case list had none, and the final `*)` is
   `AC_MSG_ERROR([No support for this host/target combination.])` — so libstdc++ refused
   the target outright. **This, and not `configure.host`, is why there was never a wasm32
   libstdc++**; `configure.host` falls through to `os/generic` and `cpu/generic` on its
   own and needs nothing. The arm is shared with `*-mingw32*` rather than written fresh:
   every line of it is a feature *check* rather than an assertion, and the set is the
   right one here — a real libc with math and stdlib, no `/dev/random`, no iconv, no
   `uselocale`. A separate arm would have had to repeat all of it to say the same thing.

4. **The generated `libstdc++-v3/configure` gets the same one-line change**, because
   nothing in this build runs autoconf. `crossconfig.m4` alone would have no effect;
   editing only `configure` would be lost by the next person who does regenerate. Both,
   and the comment in the `.m4` explains the arm.

Unlike `0005`, this one is plausibly offerable upstream: it takes no side on anything, it
removes a hardcode that contradicts the target's own configuration, and adds two that the
target's own libc dictates. The `configure` hunk would be dropped in favour of a real
autoreconf.

## Local changes: `0007-local-one-only-and-aliases.patch`

Two things every C++ target has and this one did not: **link-once semantics** and **symbol
aliases**. Both were found by libstdc++ asking for them, and the first is the root cause of
what the second looked like.

`libsupc++/tinfo.cc:55` failed with `alias definitions not supported in this
configuration`, which reads like a missing `ASM_OUTPUT_DEF` and is really a missing
`MAKE_DECL_ONE_ONLY` one level up. The chain, each link checkable in the tree:
`defaults.h:300` derives `SUPPORTS_ONE_ONLY` from `MAKE_DECL_ONE_ONLY` being defined;
`c-family/c-cppbuiltin.cc:952` derives `__GXX_WEAK__` from that; `libsupc++/typeinfo:77`
derives `__GXX_TYPEINFO_EQUALITY_INLINE` from *that*; and its out-of-line branch defines
`std::type_info::__equal` as an alias of `operator==`. So a target without one-only is a
target whose libsupc++ cannot be compiled, whatever it does about aliases. Both are
implemented here anyway — the alias is what `__attribute__((alias))` in anyone's code
needs, and it is green.

Four changes, in `wasm.h` and `wasm-asm.cc`:

1. **`MAKE_DECL_ONE_ONLY` makes the decl weak**, which is how two definitions coexist, and
   `varasm.cc:make_decl_one_only` then also records a comdat group. Weak alone was already
   the fallback that branch takes when a target has no one-only; what the group adds is the
   linker *discarding* the duplicate rather than keeping an unreferenced copy, and keeping
   a group's members — a vtable and the rest of its class — together.

2. **Every symbol gets its own section**, `.text.<name>` or `.data.<name>`, and a one-only
   one gets `,"G",@,<group>,comdat`. The group key is GCC's own `DECL_COMDAT_GROUP` and not
   the symbol name, because several decls share one group. Data already had a section of
   its own; functions had a bare `.text`, and that had to go for a reason worth stating
   plainly: **`.text` does not end a comdat group.** A function emitted after a one-only
   one joins its group, and the linker then discards that function along with the
   duplicate copy — surfacing as `undefined symbol` naming a symbol the object plainly
   defines, which reads like a relocation bug and is not one.

3. **`SET_ASM_OP` is `.set`**, which is what gives `defaults.h`'s `ASM_OUTPUT_DEF` and with
   it `TARGET_SUPPORTS_ALIASES`; without the latter the front end rejects
   `__attribute__((alias))` before any of this is reached.

4. **`ASM_OUTPUT_DEF_FROM_DECLS` is the decl path**, because a function alias needs two
   things the plain spelling cannot carry: its own `.type` and a `.functype` in the module
   header, since a `call` naming the alias is rejected unless the signature is already in
   scope. It also emits the alias's own binding directives, which on an ELF target varasm
   would have done through `globalize_decl` — here `TARGET_ASM_GLOBALIZE_LABEL` and
   `TARGET_ASM_ASSEMBLE_VISIBILITY` are no-op lambdas, so nothing else writes them.

It also corrects one of `0005`'s own claims, which is in this patch rather than in that one
only because `wasm-asm.cc` is cut here now. **A NaN can carry a sign in this dialect**; what
it cannot carry is wat's `:0x<payload>` suffix. `0005` read the two as one restriction and
`sorry`ed on both, and the cost was precise: `fast_float`'s `-quiet_NaN ()` is what
`src/c++17/floating_from_chars.cc` is built from, so the target had no `std::from_chars`
for a float and the library build stopped there. The `sorry` stays for a payload-carrying
NaN, because printing a bare `nan` for one would silently substitute a different constant.
`nan_sign` and `nan_payload` in the dialect probe are now the measurement, the second
deliberately red — and `nan_sign` extracts the sign bit rather than returning the constant,
because an assembler that took `-nan` and produced a positive NaN would satisfy anything
weaker.

`comdat` and `alias` in `Tools/wasm_dialect_probe.py` are the measurements that the
assembler takes any of this, and `comdat_tu` in `build_gcc_wasm.py` is the tripwire on
GCC's own output: two translation units defining the same inline function and vtable,
linked and run. It is honestly a tripwire and not a measurement of the dedup — two weak
definitions also resolve to one, so no return value distinguishes the group from the
fallback — but it catches both ways the group emission goes wrong, and both were hit on the
way here.

This one is offerable upstream as it stands, unlike `0005`: it takes no side on any open
question, and it is what the series needs to host a C++ runtime at all.

## Local changes: `0008-local-import-signatures.patch`

Two bugs, one subject: **a function the assembler was never told about**. Both are what
stood between step 9's "everything but two objects" and an archived `libstdc++.a`, and
neither is about the two objects the step named.

1. **`__atomic_compare_exchange_N` is declared with a parameter it is not called with.**
   The builtin takes six arguments and the library function takes five: GCC drops `weak`
   on the way out because libatomic's ABI never had it. Both places that do it say so —
   `builtins.cc`'s `BUILT_IN_ATOMIC_COMPARE_EXCHANGE_*` case under *the weak parameter
   must be dropped to match the expected parameter list*, and
   `expand_ifn_atomic_compare_exchange_into_call` — and both then rebuild the `CALL_EXPR`
   keeping the original `CALL_EXPR_FN`. So the decl reached from the call still carries
   the six-parameter prototype, the import is emitted from that type, and a
   type-checking assembler rejects the five-argument call upstream deliberately emitted.
   `src/c++20/tzdb.cc` is the first thing to reach it.

   Worth recording that the plan had this backwards. It read the symptom —
   `expected [i32 ×6] but got [i32 ×5]` — as the *call site* dropping an argument, and
   named `record_libcall` as the suspect on the strength of `0005` having fixed a
   reverse-order bug in the same function. `record_libcall` turns out not to be called
   for this symbol at all: a `fprintf` in it never fired. The five-argument call is
   correct and the declaration is wrong, which is the opposite repair.

2. **A function whose address is taken in data is never declared.** `assemble_integer`'s
   `SYMBOL_REF` case already recognises exactly this case — it is where `table_used` is
   set — and its comment said a function pointer in data *needs nothing said about it*,
   because the assembler picks `MEMORY_ADDR` or `TABLE_INDEX` from the symbol's own kind.
   That is true and it is the wrong half: a symbol llvm-mc has never been told about has
   no kind, and it settles on data. The object then defines the name as data where
   another object defines it as a function, and wasm-ld refuses the pair with `symbol
   type mismatch`.

   `libsupc++/eh_unex_handler.cc` is the first place it bites, and it shows why nothing
   caught it earlier: the file is one line, `__unexpected_handler = std::terminate`, and
   it never *calls* `std::terminate`. Every probe that links a function pointer calls it
   too, so the declaration always came from the call. The fix routes the reference
   through `TARGET_ASM_ASSEMBLE_UNDEFINED_DECL` rather than declaring it in place, so a
   signature is still derived in one place; both that hook and `declare_func` are
   idempotent, so a function defined in this unit is unaffected.

The third hunk is libstdc++'s, not the backend's. **`src/c++23/print.cc` wants `::dup`,
which WASI does not have** — wasi-libc's `<unistd.h>` says so in a comment where the
declaration would be, so it is a design decision and not an omission. It is excluded with
`&& ! defined __wasi__`, matching the `! defined __AVR__` already on the same line, rather
than declared in the sysroot the way `setjmp` is: the caller `fclose`s what the function
returns, so a declared-and-undefined `dup` would archive libstdc++ and then fail to link
in any program that prints to a terminal. `__open_terminal` above it needs only `fileno`
and `isatty`, which WASI does have, and is left alone.

Both backend hunks are offerable upstream; the `print.cc` hunk is offerable to libstdc++
as an ordinary WASI portability fix.

## Local changes: `0009-local-terminating-unwinder.patch`

A new `libgcc/config/wasm/unwind-wasm.c` and the one line of `libgcc/config/wasm/t-wasm`
that builds it. Upstream's `LIB2ADDEH=` is right about the unwinders it removes — neither
the dw2 nor the sjlj one can work on a target whose `TARGET_EXCEPT_UNWIND_INFO` is
`UI_NONE` — and wrong about leaving libgcc with no `_Unwind_*` at all, because libstdc++
refers to four of them from objects that any hosted link pulls in.

**Four, measured off the link rather than read out of `unwind.h`**: `_Unwind_RaiseException`
and `_Unwind_Resume_or_Rethrow` from `eh_throw.o`, `_Unwind_DeleteException` from
`eh_catch.o`, `_Unwind_Resume` from `stdexcept.o` and `cow-stdexcept.o`.
`_Unwind_ForcedUnwind` and `_Unwind_Backtrace` were written first, on the expectation
that the personality routine would want them, and removed again: nothing pulls
`eh_personality.o` in, because with `UI_NONE` no function names a personality routine, so
the `_Unwind_Get*` accessors are not referenced either. **The second half of that is
expired and `0014` corrects it** — a libstdc++ locale member references
`__gxx_personality_v0`, so a link that pulls the locale objects pulls `eh_personality.o`
and all three accessors with it. The two it says nothing about are still out, on the rule
that the link names what libgcc has to define.

Every one of them is `abort ()`, and that is the design rather than a shortcut. With
`UI_NONE` no function carries unwind information, so there is no state in which returning
anything would be more correct than terminating. What it buys is not exceptions: it is
every program that does not throw, which until now could not be linked at all. A `throw`
terminates, which is the truth about the target.

`emutls.c` survives the assignment because `libgcc/Makefile.in` appends it to `LIB2ADDEH`
at `:448`, after including the `t-` fragment at `:382`.

This is offerable upstream, and is probably the shape upstream wants until the backend
implements the exception-handling proposal: it adds no capability and removes no honesty.

## Local changes: `0010-local-weak-references.patch`

One `if` in `assemble_import`. A **weak reference** — a symbol declared weak that nothing
defines — is a different thing from the weak *definitions* `0007`'s `MAKE_DECL_ONE_ONLY`
produces, and the target emitted nothing for one, so it came out as an ordinary undefined
symbol and failed the link.

libstdc++ is what asked. `c++config:755` derives `_GLIBCXX_USE_WEAK_REF` from
`__GXX_WEAK__`, which `0007` set to 1, and reads it as a promise that a function may be
declared weak and never defined — which is how `src/c++11/cow-stdexcept.cc` refers to
libitm's `_ITM_RU1`, `_ITM_memcpyRtWn`, `_ITM_memcpyRnWt` and `_ZGTtnam` without
depending on libitm. So the target made a promise with nothing behind it, and a
`std::string` could not be linked.

The alternative was to withdraw the promise: a `config/cpu/wasm/cpu_defines.h` defining
`_GLIBCXX_USE_WEAK_REF 0` and a `cpu_defines_dir` arm in `configure.host`, which is what
nvptx and gcn do. `weak_undef` in `Tools/wasm_dialect_probe.py` is why it is this way
instead — llvm-mc marks the symbol `undefined binding=weak` and wasm-ld leaves it
unresolved, so the capability is real and the target was simply not emitting it. Opting
out would have left `__attribute__((weak))` quietly broken for everyone else's code.

The directive is written where an undefined symbol's other directives are written, which
also gives a weak data reference its binding — the one thing this target has to say about
an undefined data symbol. Offerable upstream.

## Local changes: `0011-local-multi3.patch`

A new `libgcc/config/wasm/multi3.c` and the one line of `libgcc/config/wasm/t-wasm` that
builds it. Same shape as `0009`, and for a related reason: a symbol libstdc++'s own
objects refer to from any hosted link, which libgcc as configured does not define.

**The ecosystem's data model is clang's.** Every prebuilt wasm library in existence was
built by clang, whose wasm32 has `__int128` and an IEEE binary128 `long double` and calls
for the compiler-rt routines behind both. GCC's wasm32 has neither: `wasm.h` caps
`MAX_FIXED_MODE_SIZE` at 64 and `wasm.cc`'s `scalar_mode_supported_p` vetoes `TImode`.
Nineteen compiler-rt symbols are missing for that reason, and **one of them was actually
in the way** — wasi-libc's `intscan.c.obj` calls `__multi3` from the integer parser that
libstdc++'s locale setup reaches, so every hosted C++ link failed on it.

**The signature is measured, not assumed**, and the file says where from:

```sh
wasm-objdump -x intscan.c.obj   # after llvm-ar x libc.a intscan.c.obj
#  - type[3] (i32, i64, i64, i64, i64) -> nil
#  - func[2] sig=3 <env.__multi3>
```

A result pointer first, then each 128-bit argument as two i64s, low word first. Declared
in exactly those terms, with no `__int128` anywhere, GCC emits that signature. Getting it
wrong is not quiet — `wasm-ld` compares an undefined symbol's signature against the
definition and says `function signature mismatch` — which is why `include_vector` linking
cleanly, rather than the `multi3` probe, is what checks the reading.

### Why it is hand-written, which is the finding of the step

The generic routine is `libgcc2.c`'s `__muldi3` built at `LIBGCC2_UNITS_PER_WORD 8`, where
`DWtype` is `TImode` and `__NDW(mul,3)` spells `__multi3`. Three lines reach it, and they
were written and measured before this file was:

- `MAX_FIXED_MODE_SIZE` 128, so a 128-bit integer type can be formed;
- `MIN_UNITS_PER_WORD 4` withheld from libgcc under `#ifndef IN_LIBGCC2`, because
  **libgcc derives its word size from `MIN_UNITS_PER_WORD`, not `UNITS_PER_WORD`**
  (`libgcc2.c:39-46`) — which is why there was never a `_multi3.o` to be empty. This is
  the spelling `i386.h:766`, `mips.h:1651`, `riscv.h:170` and `loongarch.h:132` use;
- the `m != TImode` veto dropped from `TARGET_SCALAR_MODE_SUPPORTED_P`.

The compiler built. **libgcc did not**: seven objects ICE, in `emit_move_multi_word`
(`expr.cc:4545`) and in `extract_insn` on

```
(insn 2 (set (subreg:DI (reg:TI 27) 0) (reg:DI 24)))
```

The target has no move pattern for a word of a TImode value, because
`constraints.md`'s `wasm_register_operand` and `wasm_subregister_operand` and
`wasm-cg.cc`'s `hard_regno_mode_ok` all return false for `TImode`. Those exclusions read
like *split this into word-sized halves* and are in fact *reject this*, consistent with a
`scalar_mode_supported_p` that was never meant to see one; `wasm_function_arg`'s TImode
arm, which passes one as 2×i64 in LE order, is an unfinished start rather than evidence
the rest works. Teaching the move patterns and predicates about `subreg:DI` of `TImode` is
real back-end work and is not in this patch.

So this file supplies the one symbol that was in the way and leaves `__int128`
unsupported, which is the honest state: a compiler that ICEs on ordinary `__int128` code
would be worse than one that says it has none.

**This does not touch `long double`.** The other half of the mismatch — clang's wasm32
`long double` is binary128, GCC's is `double`, and `wasm-ld` warns that our `strtold`
returns `f64` where `libc.a(strtod.c.obj)` returns binary128 — needs
`LONG_DOUBLE_TYPE_SIZE 128`, TFmode and a `t-softfp` arm. `strtold` would be silently
wrong rather than unlinked, so it is a fork in the plan rather than a bug with a patch.

`multi3` in `build_gcc_wasm.py` is the measurement, and it computes rather than links on
purpose: a missing symbol is a link error anyone notices, while a 64×64→128 multiply that
drops the carry out of its two middle partial products links perfectly and is wrong only
on inputs nobody picks. The implementation was differential-tested against a native
`unsigned __int128` over three million random pairs plus the power-of-two and all-ones
edges before it was committed.

Offerable upstream, in the same way `0009` is: it adds no capability, claims none, and
makes the target linkable against the objects everyone else's compiler produces.

## Local changes: `0012-local-host-wide-int-operands.patch`

One `fprintf` format string, and it was a **silent miscompile of every 64-bit constant
that does not fit in 32 bits**.

`wasm_print_operand`'s `CONST_INT` case printed `INTVAL (value)` with `"%ld"`. `INTVAL`
yields a `HOST_WIDE_INT`, which is always 64-bit; `long` is 32-bit on an LLP64 host. So
`fprintf` read half the argument and printed it as a signed 32-bit number, and a DImode
`0xffffffff` came out as

```
i64.const	-1
```

which is `0xffffffffffffffff`. `x & 0xffffffff` therefore assembled as `x & ~0` — the
mask was there, the instruction was there, and the constant was wrong. `assemble_integer`
below it already uses `HOST_WIDE_INT_PRINT_UNSIGNED` for exactly this reason; the operand
path had been missed. Same family as `0002`: upstream develops on Linux, where `%ld` *is*
64-bit and the code is correct.

**Found by the `multi3` probe, which is the point of having written it to compute.** The
hand-written 64×64→128 multiply in `0011` is built out of `a & 0xffffffff` and `a >> 32`,
so on this host every one of its four partial products silently became a full 64-bit
product. It had already been differential-tested against native `unsigned __int128` over
three million pairs, so the arithmetic was not in doubt and the toolchain was. A probe
that only linked would have been green.

Two things worth keeping about how it was localised, because both were nearly missed:

- **The first debug probe was blind.** `__multi3 (p, 3, 0, 5, 0)` returns 15 under *any*
  permutation of its four arguments, so it cleared the routine and the ABI at once and
  proved nothing about either. Four distinguishable operands (`1, 2, 3, 4`) are what
  separate "the ABI is right" from "the operands happen to be symmetric".
- **The wrong answer named its own cause.** The module's result for
  `(a & 0xffffffff) * (b & 0xffffffff)` was bit-for-bit the low word of the *unmasked*
  `a * b`, which says the mask did nothing rather than that the multiply was wrong — and
  that is a two-line read of the emitted `.s` rather than a bisect.

Offerable upstream, with `0002`.

## Local changes: `0013-local-module-initializer-decl.patch`

One line in `gcc/cp/module.cc`, and the only patch of ours in the C++ front end — `0006`
and `0008` already reach outside the backend, but into build and library files rather than
into a compiler. It is also the one most plainly offerable upstream: the reproducer has no
wasm in it.

`module_add_import_initializers` synthesizes, per imported module, the decl for that
module's global initializer — `build_lang_decl (FUNCTION_DECL, …)`, `TREE_PUBLIC`,
`determine_visibility` — and calls it from the consumer's static-initialization function.
It never sets **`DECL_EXTERNAL`**, and `varasm.cc`'s `assemble_external` returns
immediately for a decl that is not external, so `ASM_OUTPUT_EXTERNAL` — here
`wasm_handle_import`, the only route into `import_map` — is never reached for it. The
module header is then written with no `.functype` for the callee and `llvm-mc` refuses the
unit:

```
error: symbol _ZGIW2mi: missing .functype
        call    _ZGIW2mi
```

The decl *is* external: the function is defined in the module interface's own object, and
this decl exists only to call it. Saying so is the whole patch.

**Why no other target noticed.** `assemble_external` is a no-op unless a target defines
`ASM_OUTPUT_EXTERNAL`, which only a handful do, and of those none needs a declaration
before a *call* — a missing `.globl`/`.type` for a called symbol costs an ELF target
nothing. So the bug has been latent since module initializers were implemented.

**Why nothing here noticed earlier.** `import_std` emits no `_ZGIW` call at all, so until
step 14's engine rows nothing in this pipeline had assembled a consumer of a named module
with an initializer. `module_init` in `build_gcc_wasm.py` is now the narrowest subject for
it: a two-TU freestanding probe, an `export module mi;` with a dynamic initializer and a
second unit that imports it. Its `expect` is what makes it a measurement rather than a
compile — the value comes back only if the initializer actually ran.

Step 15 recorded this blocker as *backend-side and `0008`-shaped*, and that was wrong in a
way worth leaving written down: the symptom is identical to `0008`'s second bug — a
referenced-but-not-defined function whose decl never reaches the declare-external path —
but there the decl was fine and the backend was not asking, while here the backend asks
correctly and the decl does not qualify. The backend-side repair was considered and
rejected: declaring on reference in `wasm_print_operand`, the way `assemble_integer` does
for a function pointer in data, would route **every** call through
`import_map`/`assemble_import`, including calls to comdat functions defined in this very
unit — `DECL_WEAK` holds for those, so `0010`'s `.weak` would be emitted for each. That is
a behavioural change at template-call scale in exchange for one missing declaration.

## Local changes: `0014-local-personality-accessors.patch`

Three more `abort ()` bodies in `0009`'s `unwind-wasm.c`: `_Unwind_GetRegionStart`,
`_Unwind_GetTextRelBase` and `_Unwind_GetDataRelBase`. `0009` wrote the first two of those
and removed them again on a measurement, and the measurement has expired. Its reasoning was
that with `UI_NONE` no function names a personality routine, so nothing pulls
`eh_personality.o` into a link and nothing refers to the accessors it needs. True of the
*engine*, and not of the library underneath it: a libstdc++ locale member references
`__gxx_personality_v0`, `eh_personality.o` is where that lives, and it refers to all three.

So the finding is about the method rather than the symbols. **The link is the authority on
what libgcc has to define**, not an argument from `TARGET_EXCEPT_UNWIND_INFO`;
`_Unwind_ForcedUnwind` and `_Unwind_Backtrace` stay out on the same rule, because nothing
has named them. `0009`'s own comment said the accessors were unreferenced, so this patch
rewrites it — a comment that contradicts the code three lines below it costs the next
reader more than the code does.

`abort ()` is honest here for a stronger reason than it is in `0009`. Each accessor takes a
`struct _Unwind_Context *`, the only thing that constructs one is an unwind in progress, and
`_Unwind_RaiseException` terminates before one begins. They are unreachable by construction
rather than unimplemented, so there is no probe for them and there should not be:
`throw_catch`'s honest `trapped` status is the row that covers this whole bargain.

Offerable upstream with `0009`, and really a correction to it: it adds no capability and
claims none.

## Local changes: `0015-local-native-tls.patch`

The target had no thread-local storage at all. GCC's fallback for that is emulated TLS,
and emutls is not a slower native TLS — it is a *different object model*: a `__thread`
variable becomes a control object plus a per-thread copy that `__emutls_get_address`
allocates on first use. Two compilers cannot share a variable across that boundary, and
every prebuilt wasm library in existence is built by clang, which gives a thread-local a
real `.tdata` segment and no control object at all.

`wasi-libc`'s `errno` is exactly that variable. Step 17 met it as `__emutls_v.errno`
undefined and worked around it in the sysroot, rewriting `extern _Thread_local int errno`
to drop the keyword — correct for the *non-threads* variant, whose clang-built `libc.a`
really does put errno in ordinary data. On the threads variant the header is telling the
truth, so that rewrite would make C++ read a non-thread-local address libc never writes:
every thread but the first would see another thread's errno, a green link bought with a
silently wrong answer. This patch is what let the rewrite be deleted rather than inverted.

Four pieces, and the last is the one that is easy to miss:

| Piece | What it does |
|---|---|
| `TARGET_HAVE_TLS` | Stops the emutls lowering. One line, and the rest follows from it being true. |
| `.tdata` + `"T"` | A thread-local definition gets a `.tdata.<name>` section carrying the `"T"` flag. `emit_symbol_section` grows a flags argument rather than a near-copy. |
| `@TLSREL` | The address: `global.get __tls_base`, `i32.const sym@TLSREL`, `i32.add`. |
| features | `-mattr=+atomics,+bulk-memory` in `ASM_SPEC`, **and** a `target_features` custom section in the object. |

**The `"T"` flag is load-bearing and its absence is invisible.** The section *name* alone
leaves an ordinary data symbol that happens to be called `.tdata.counter`; the flag is
what makes the assembler give the symbol the `tls` kind and a relocation against it
`R_WASM_MEMORY_ADDR_TLS_SLEB`. Getting it wrong produces a program that works until
there is a second thread, because a single-threaded link places the block at an address
where reading it through `__tls_base` still returns the right value. That is why the
`tls` subject in `Tools/wasm_dialect_probe.py` asserts `tls binding=local` by name
rather than trusting a green run — the same rule every other subject there follows.

**No new pattern in `wasm.md`, and no `legitimize_address` hook.** A thread-local's
address is not a link-time constant, so it takes three instructions where an ordinary
symbol takes one — and that is still a drop-in replacement, because the operand code is
emitting text for a stack machine and may push its value however it likes provided it
leaves exactly one. The `*local_const_addr` pattern that already handles address-taking
needed no change at all. The `i` operand mode is asserted, because reaching this without
it means the symbol is being *named* rather than addressed, which happens in data, where
a TLS offset cannot be relocated — there `@TLSREL` would be silently wrong rather than
rejected.

**The `target_features` section cannot be left to the assembler, and this is the
measurement that reshaped the step.** `-mattr` tells `llvm-mc` which instructions to
*accept*; it records nothing in the object. `wasm-ld` then refuses thread-local storage
outright:

```
wasm-ld: error: 'bulk-memory' feature must be used in order to use thread-local storage
wasm-ld: error: 'atomics' feature must be used in order to use thread-local storage
```

A hand-written subject with TLS, `-mattr` and no section is rejected; the same subject
links once the section is there. **So TLS and the atomic instructions cannot be delivered
one at a time** — the scoping plan had `0015` landing alone behind `--disable-threads` as
a fallback, and that option does not exist.

The two halves of that are not symmetric, and the first version of this section had it
wrong in a way worth recording. **TLS itself assembles with no `-mattr` at all** and wants
the features only at the link; **an atomic instruction without them is rejected outright**,
`instruction requires: atomics`. The claim that `llvm-mc` "mis-type-checks rather than
rejecting" is true of `+exception-handling`, which is where step 12 measured it, and was
carried over here by analogy without being checked. It matters because the two failure
modes are told apart by whether anything complains: `Tools/build_gcc_wasm.py` assembled
every probe with no `-mattr` for a whole step and stayed green, because no probe's own
translation unit emitted an atomic. `ASM_FEATURES` and `check_asm_features` there now hold
the one copy of this list and refuse to run when it drifts from `ASM_SPEC`. The section is emitted unconditionally,
matching `ASM_SPEC`: every object this compiler produces is for a target with these
features whether or not a given unit reached for them, and declaring the same set
everywhere is what lets `wasm-ld`'s default `--check-features` succeed against the threads
sysroot, whose own objects declare exactly these two.

`__tls_base` is declared like the table rather than like `__stack_pointer` — only where
something in the unit reached for it — because declaring a global nothing uses still
produces an import, and an unused import is a module that fails to instantiate against a
host that was not told to supply it.

## Local changes: `0016-local-atomics.patch`

The backend had no atomic patterns, so the middle end expanded every `__atomic` builtin
into a libcall: `__atomic_load_4`, `__atomic_fetch_add_4` and the rest of libatomic's ABI.
libatomic has no wasm32 port and `--disable-libatomic` is passed, so each of those was an
undefined symbol at the link.

**This was invisible until the thread model changed, which is why it belongs to this step
and not an earlier one.** libstdc++ uses `__gnu_cxx::__atomic_add_single` — plain
non-atomic arithmetic — for as long as no gthread model exists. The moment
`--enable-threads=posix` makes `__GTHREADS` true it switches to the builtins, so this
patch is precisely what `--enable-threads=posix` costs.

The instructions map onto the optabs closely enough that most of the patch is mechanical.
Three things are worth stating because the diff does not say them:

- **The memory-model operand is matched and ignored, everywhere.** wasm has exactly one
  ordering and it is sequential consistency: there is no acquire, release or relaxed form
  of any of these instructions. A weaker model is therefore served by being given a
  stronger guarantee than it asked for, which is permitted. The operand still has to be
  matched, because it is part of the optab's shape. The same argument retires `is_weak` in
  the compare-exchange — a weak one may fail spuriously and this one never does.
- **`opname` is both names.** It already maps each rtl code to the string wasm spells it
  with, and — checked against `optabs.def` — to the string the optab is named with too,
  including `ior` → `"or"`, so `atomic_fetch_<opname>` is correct on both sides. `nand` is
  left out of the code iterator because wasm has no `rmw.nand`; the middle end turns that
  one into a loop.
- **`atomic_compare_and_swap` is the one shape mismatch.** wasm's `cmpxchg` returns the
  value it found; the optab wants that *and* a boolean saying whether the exchange
  happened. So it is a `define_expand` over an insn, with the boolean produced by
  `emit_store_flag` comparing found against expected — the same answer a flag-setting
  target would report. `emit_store_flag` rather than `gen_cstore<mode>4` directly, because
  `cstore` here is defined over SI and DI only, so a QI or HI compare-exchange needs its
  operands widened first.

A sub-word atomic carries its width in the mnemonic and an unsigned-extension suffix
(`i32.atomic.rmw8.add_u`) where a word-sized one carries neither (`i32.atomic.rmw.add`),
so one template covers all four widths with two mode attributes empty. Every atomic is an
`i32` or `i64` on the operand stack — there is no `i8` value type — which is why
`atomic_type` is a separate attribute from `types`.


## Local changes: `0017-local-gthr-no-weak-refs.patch`

This is what `--enable-threads=posix` actually cost outside the backend, and it was
found by the build rather than predicted. It is the first of ours in libgcc's
target-independent files — `0009`, `0011` and `0014` are in libgcc, but under
`config/wasm/`.

**Two things were missing, and only one needed a patch.** The first error was
`'pthread_cancel' undeclared here (not in a function)`: wasi-libc has no cancellation
support by stated design, its `pthread.h` declaring `pthread_cancel` only under
`__wasilibc_unmodified_upstream`. That was fixed on its own first, extending the
`__BIONIC__` arms gthr-posix.h already carries at both the `__gthrw` list and
`GTHR_ACTIVE_PROXY` — bionic has no `pthread_cancel` either.

**That patch was then deleted as dead code**, and the reasoning is worth keeping because
it is not obvious from either file: both of its hunks live inside
`#if SUPPORTS_WEAK && GTHREAD_USE_WEAK` regions (gthr-posix.h:116, and :188–299 for the
proxy block), so with `GTHREAD_USE_WEAK` at 0 the `__gthrw(pthread_cancel)` line expands
to nothing and the whole proxy block is skipped. Measured, not argued: with it reverted
and this patch applied, `emutls.o` compiles. If `GTHREAD_USE_WEAK` is ever turned back on
here, gthr-posix.h needs `__wasi__` beside `__BIONIC__` in those two places again.

**The weak references themselves do not assemble**, which is what this patch fixes.
Every `__gthrw` alias failed:

```
error: symbol pthread_key_create: missing .functype
error: type mismatch, expected [] but got [i32]
```

A weakref alias is *referenced* without GCC ever declaring the target symbol external, so
`ASM_OUTPUT_EXTERNAL` — and therefore `wasm_handle_import` — is never reached for it, and
the call arrives at llvm-mc with no signature. The type mismatch on the next line is the
usual trailer for an unknown signature, not a second bug.

The fix is `GTHREAD_USE_WEAK 0` for `__wasi__`, beside the `__MINGW32__` and glibc-2.34
arms in `gthr.h`, **and it is correct rather than expedient**. wasi-libc keeps every
`pthread_*` API in libc: `llvm-ar t libpthread.a` shows an empty 8-byte archive, and
`llvm-nm --defined-only libc.a` finds `pthread_create`, `pthread_mutex_lock` and
`pthread_key_create` there. That is precisely the case gthr.h's glibc-2.34 arm exists for —
"no need to link separately with -lpthread" — so there is no separate library for a weak
reference to be weak against. It is also why no link line here carries `-lpthread`.

**What this leaves open, named rather than quietly avoided.** A weak reference to a
function that is actually *called* still has no signature on this target. `0010` covers the
case libstdc++ uses — a weak reference that is never called, which `weak_undef` in
`Tools/wasm_dialect_probe.py` measures — so nothing in the engine or libstdc++ needs more
than that today. Teaching the backend to follow weakref aliases into the import map is the
real fix and is **not done**; this patch sidesteps the only consumer of it.

**One consequence worth stating because it is invisible otherwise:**
`__gthread_active_p` now always answers true, so a `std::mutex` really locks rather than
compiling to nothing. That is correct for this sysroot, where the pthreads are always
present, and it is what makes the `std_mutex` probe a measurement rather than a tautology.


## Local changes: `0018-local-called-function-decls-and-muloti4.patch`

Two unrelated link blockers, together because they want the same rebuild. This is the
patch that takes the engine from "every unit translates" to a linked `engine.wasm`.

**The declaration.** `0013` and `0017` each met one instance of a single class: GCC emits
a call, nothing ever marks the callee external, `assemble_external` therefore declines it,
`ASM_OUTPUT_EXTERNAL` never fires, and llvm-mc rejects the unit with
`symbol <name>: missing .functype`. `0013` fixed its instance in the front end by setting
`DECL_EXTERNAL` on the synthesized decl; `0017` sidestepped its instance by switching the
weak references off. This one fixes the class where it can be fixed once, in the backend.

The instance that forced it: a module defining an `inline thread_local`. Every unit that
touches the variable emits two comdat symbols — the variable and the `_ZTW` access wrapper
— while the `_ZTH` init function those wrappers call is emitted once, in the module
interface's own object, as an alias to that unit's `__tls_init`. Measured in the engine's
own tree, where `gse.diag` declares `inline thread_local thread_buffer tls`:

```
link-0068.s  (Diag/Trace.cppm)   .functype _ZTHN3gse5traceW3gseW4diag3tlsE () -> ()
                                 __tls_init:  ...
                                 .set _ZTHN3gse5traceW3gseW4diag3tlsE, __tls_init
link-0389.s  (Diag/Trace.cpp)    call _ZTHN3gse5traceW3gseW4diag3tlsE     <- no .functype
```

So the definition is real and reachable — `0007`'s alias path declares it correctly on the
interface side — and only the *consumer's* declaration was missing. `get_tls_init_fn`
(`gcc/cp/decl2.cc`) builds that decl with `DECL_EXTERNAL` copied from the **variable**,
which is false in a unit that defines the variable, so the decl is never offered to
`assemble_external` at all. That is `0013`'s bug one layer out, and it is why
`Diag/Trace.cpp` was the single unit of 497 that would not assemble, and why the eight
undefined `gse::trace::*` symbols in that link were simply that unit's own contents.

Fixed on the target side rather than in the front end, because the backend already carries
this exact repair one case over: a function's address *stored in data* reaches
`wasm_assemble_integer`, which routes it through `wasm_handle_import` for the same reason
and says so in its comment. A function named in the *instruction stream* — a call's
callee, or an address pushed in code — had no such site. The new site is the `SYMBOL_REF`
arm of `wasm_print_operand`, guarded on the symbol carrying a `FUNCTION_DECL` so it cannot
fire for data, and routed through the same hook so one place still derives a signature.

It is safe to declare from inside operand printing, which is the thing worth checking in a
backend that buffers its body to reorder declarations ahead of it: `wasm_handle_import`
only records into `import_map`, writing nothing, and that map is drained in
`wasm_asm_file_end` ahead of both `func_decls` and `flush_buffer`. And it cannot corrupt a
local definition's signature: `declare_func` dedups by name, and a function defined here
registers its own RTL-derived signature during body emission, before the drain.

**This may also close `0017`'s open item** — a weak reference that is actually called now
reaches a declaration by this path — but that is unmeasured and should stay listed as open
until something exercises it, since `GTHREAD_USE_WEAK` is still 0 and nothing calls one.

**`__muloti4`** is `0011`'s story exactly, found the same way. This compiler can never emit
a call to a TImode routine, and wasi-libc's `clock_nanosleep.c` calls this one, so it is
undefined in any link whose program can sleep — whether or not that program ever reaches
the call, which is why "is it reachable?" is the wrong question for it.

The ABI is read off the caller rather than assumed, as `multi3.c` insists:

```
$ decode clock_nanosleep.c.obj
  import env.__muloti4: type[1] (i32, i64, i64, i64, i64, i32) -> ()
```

`__multi3`'s ABI with the `int *overflow` out-parameter appended: result through a pointer
passed first, each 128-bit argument two i64s low word first. The built `muloti4.o` exports
exactly that type.

The overflow half is the part that could be wrong *quietly*, so it is computed from a full
256-bit product of the magnitudes in four limbs rather than by the division compiler-rt
uses — that would have meant writing `__udivti3`, a second routine this target also lacks.
The bound is asymmetric because two's complement is: a negative result may reach 2^127
exactly, a positive one may not. `magnitude` yields 2^127 for MIN, which is what lets MIN
need no special case here where compiler-rt needs three.

Checked against `__int128` on the build host rather than by a probe, because nothing this
compiler emits calls the routine and a probe would have to hand-write the call: every pair
from a boundary set built around MIN, MAX and the powers of two either side of each word
edge, plus four million random pairs across the magnitude range, product and flag both,
**0 failures in 4,000,361 cases**. A `muloti4` subject in `Tools/wasm_dialect_probe.py`
would make that repeatable here and is **not done**.
