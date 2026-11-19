# A WebAssembly pipeline for GSE

The goal is to run the engine in a browser. This file records the design spike that
decides *how* the compiler half of that happens, and the verdict it reached.

GCC citations are against trunk (GCC 17, `master` as of 2026-10-11), read from a sparse
checkout of `gcc.gnu.org/git/gcc.git`. The installed toolchain is `17.0.0 20260809` and
the vendored recipe pins snapshot `20260602`
(`Tools/gcc-toolchain/recipe/PKGBUILD:36`), so the source read here is the source we
would be patching.

## Verdict

**GO, and the work is far smaller than scoped — because someone is already doing it
upstream.**

The spike set out to decide whether *we* should write a GCC `wasm32` backend. The answer
is that we should not: one exists as an in-flight RFC, the GCC steering committee has
approved the direction, and it is aimed at GCC 17 — the exact trunk series this project
already builds weekly. Our job changes from "write a backend" to "build it, measure it,
and push the C++ enablement it is missing."

The scope's decision rule was: if GCC cannot defer jump emission past reload, path B is
dead and we fall back to shipping a browser-shaped subset. It does not trigger, and the
upstream backend independently confirms both load-bearing conclusions of the spike
(see Q1 and Q2) — it sets `TARGET_NO_REGISTER_ALLOCATION true` and registers its pass with
`INSERT_PASS_BEFORE (pass_free_cfg, 1, ...)`, which are the two mechanisms this spike
identified from nvptx before knowing the backend existed.

**The real blocker is not control flow. It is exceptions.** That has survived every step,
but where they bite has moved twice and the current answer is the last one: step 8 thought
they stopped libstdc++ *compiling*, step 9 removed that guard and found they did not appear
in the build at all, and step 10 archived `libstdc++.a` and met them at the **link** —
`wasm32` has no unwinder, and `-fno-exceptions` does not get around it. Linking looked like
a second blocker from step 3 to step 7 and is closed; libstdc++ itself is closed as of step
10. See "What actually gates us".

## Upstream state (the thing that changes the plan)

| Fact | Detail |
|---|---|
| Patch series | `[RFC PATCH 0/3] wasm: New backend`, gcc-patches, 2026-05-05 |
| Author | "feedable" (`feedabl3@gmail.com`) |
| Size | 78 files, ~2,991 insertions; `gcc/config/wasm/` is ~2,600 lines |
| Files | `wasm.h`, `wasm.cc`, `wasm.md` (495 lines), `wasm-asm.cc` (945), `wasm-cg.cc` (621), `wasm-passes.cc` (153) |
| Status | RFC. Steering committee approved **the direction and contribution, not the implementation**; still needs GCC Global Reviewer sign-off |
| In trunk? | **No.** Verified: `git ls-tree -r master -- gcc/config/wasm` is empty, `config.gcc` has no `wasm` stanza, and no `wasm` branch exists on `origin` |
| Target release | Possibly GCC 17.1, if it matures over the coming months |
| Docs | `gcc.gnu.org/wiki/WebAssemblyBackend` carries build instructions and the live feature-status table (blocked to automated fetch by Sourceware's Anubis; read it in a browser) |

Known missing, per the author's own status notes: **WASM reference types, tables,
exceptions, debug info, data sections**, among others.

Design choices it has already made, which we inherit:

- **Control flow is a `br_table` dispatch loop, not a relooper.** Every basic block becomes
  a `block` inside one `loop`, dispatched on a control variable:
  `(loop $control (block $0 …) (block $1 …) (br_table $0 $1 … (local.get $control)))`.
  This sidesteps structuring entirely at a real performance and size cost. It is the
  universal application of the dispatch-loop fallback this spike had identified only as the
  *irreducible* case. Discussion around the series names Ramsey, "Beyond Relooper"
  (ICFP 2022) as the better algorithm — that is the natural place for us to contribute if
  codegen quality becomes the problem.
- **No stackification and no local allocation.** Register-based codegen emitting
  `local.get`/`local.set`, one local per pseudo, leaning on the browser JIT. Same bargain
  LLVM's wasm backend makes initially; costs size, not correctness.
- **Exceptions explicitly off:** `TARGET_EXCEPT_UNWIND_INFO` returns `UI_NONE`.
- **Varargs:** the stack pointer passed as an implicit first argument — the same recipe
  nvptx uses (see Q3).
- **Indirect calls:** `call_indirect` through a table, with the callee wrapped in
  `UNSPEC_VOLATILE`.
- **Module assembly is buffered** during codegen and prepended with imports and globals at
  `wasm_asm_file_end` — the same deferred-emission hatch nvptx uses for `func_decls`.

## What actually gates us

Ranked by what stands between this engine and a browser. Control-flow structuring, the
thing the scope called "the real risk", is **not** on the critical path — upstream already
has a working answer, and a crude one is good enough to start.

These items get inserted into and reordered as things are measured, so **refer to them by
name, never by number.** Inserting "Linking" already silently broke two prose references
that cited ordinals, and one of them read as closing the largest remaining blocker.

1. ~~**Exceptions, as a link blocker.**~~ **CLOSED, 2026-10-16 — see "Step 12".** Worth
   keeping the history straight, because this item was reworded in four consecutive steps:
   step 8 had it blocking libstdc++'s *compile* through `<csetjmp>`; step 9 removed that
   guard and narrowed the claim to *untested*; step 10 archived `libstdc++.a` and the link
   named four `_Unwind_*` plus libitm's `_ITM_*` and `_ZGTtnam`; step 12 supplied both
   halves. `0009` gives libgcc the four `_Unwind_*`, each `abort ()` — **four, measured
   off the link rather than the six this file estimated**, because nothing pulls
   `eh_personality.o` in under `UI_NONE`. `0010` emits `.weak` for a weak reference, which
   is what the `_ITM_*` always were and what the target had never been able to express. A
   hosted C++ program now runs: `include_vector` returns 385.

   **What is left is the engine side, and it is not a flag.** A `throw` terminates — the
   unwinder aborts, measured, because with `UI_NONE` there is no unwind information to
   walk. Real unwinding means the WebAssembly exception-handling proposal in the back end,
   which step 12 measured as **reachable**: `eh_instructions` assembles, links and runs a
   `try_table` that catches a thrown tag, so what remains is GCC's EH region generation
   and not a tooling question. The sjlj path is still not an alternative — it needs a
   `setjmp` this sysroot does not have. Meanwhile the engine really does use `catch`:
   around 111 files under `Engine/Source`, eight of them
   in `AssetRegistry.cppm` alone. Nothing here was ever "attached" to exceptions — the
   measurement is what says whether they can simply be dropped, and the answer is that
   dropping them is a real engine-side job rather than a flag.
2. ~~**Symbol aliases.**~~ **CLOSED, 2026-10-16 — see "Step 9".** And misdiagnosed where
   it was written: the missing feature was `MAKE_DECL_ONE_ONLY`, not `ASM_OUTPUT_DEF`.
   Without link-once semantics the C++ front end sets `__GXX_WEAK__=0`, libstdc++ then
   takes typeinfo's out-of-line-equality branch, and *that* branch is what wants an alias.
   `0007` implements both, one-only with real comdat groups.
3. ~~**libstdc++'s last two objects.**~~ **CLOSED, 2026-10-16 — see "Step 10".**
   `libstdc++.a` archives: 199 members, 4.9 MB. And misdiagnosed where it was written,
   both halves in the same direction — each named the call site where the *declaration*
   was at fault. `tzdb` is not a call-marshalling bug: `builtins.cc` drops
   `__atomic_compare_exchange_N`'s `weak` argument deliberately, says so in a comment,
   and rebuilds the `CALL_EXPR` keeping the six-parameter decl, so the five-argument call
   is **correct** and the declaration emitted from that decl is wrong. `record_libcall`,
   named here as the suspect, is not called for the symbol at all. `print.cc` was filed as
   a header gap to unguard like `setjmp`; wasi-libc has no `dup` at all, by stated design,
   and `print.cc` *calls* it where nothing calls `setjmp`, so the code path is excluded
   instead. A third bug had to be fixed that nothing could have reached: a function whose
   address is taken in data was never declared, so llvm-mc typed it as data.
   `0008-local-import-signatures.patch` carries all three.
4. **A `long double` and `__int128` ABI mismatch with the prebuilt sysroot.** This
   **replaces** the "seven C library names" item that stood here, which was wrong about
   its own cause: wasi-libc's `<stdlib.h>` declares all seven of `_Exit`, `atoll`,
   `llabs`, `lldiv`, `lldiv_t`, `strtoll` and `strtoull`, unguarded — they were read there
   line by line. What withholds them is `<cstdlib>`'s `_GLIBCXX_USE_C99_STDLIB`, left
   undef because libstdc++'s own configure *link* test for them failed, on `__multi3`.
   `wasm.h` caps `MAX_FIXED_MODE_SIZE` at 64 and `wasm.cc`'s `scalar_mode_supported_p`
   vetoes `TImode` outright, so GCC's wasm32 has no `__int128` and `long double` is
   `double`, and libgcc builds no TImode or TFmode routine — those archive members exist
   and are empty. clang's wasm32, which built this sysroot, has `__int128` and an IEEE
   binary128 `long double` and calls for both: nineteen compiler-rt symbols are missing
   for that reason, and `strtold` is an outright ABI disagreement that would be silently
   wrong rather than unlinked. **Every prebuilt wasm library in existence was built by
   clang**, so this is ecosystem interoperability rather than a corner case, and it is a
   fork in the plan rather than a bug with a patch — see "Step 10" for the two options.
   **Now the top item, and no longer only `import_std`'s** (2026-10-16, step 12): with the
   unwinder and the weak references closed, `include_vector` and `throw_catch` fail at the
   link on `__multi3` alone, wanted by wasi-libc's `intscan.c.obj` through libstdc++'s
   locale setup. Both run when it is stubbed, so this one symbol is what stands between
   the probe table and a green hosted row. `__multi3` is separable from the rest — its
   signature is unambiguous, unlike `strtold`'s — so the fork can be opened at its
   cheapest point rather than all at once.
3. ~~**Linking.**~~ **CLOSED, 2026-10-15 — see "Step 7".** GCC's output and `rwat`, the
   assembler its `ASM_SPEC` used to target, did not share an annotation dialect, and rwat
   could not have carried the result whatever the spelling: it has no `MEMORY_ADDR` or
   `TABLE_INDEX` relocation and no data symbol kind, so a static could not be placed and
   every engine TU has statics. The owner chose the second of the two measured options and
   GCC now emits llvm-mc's dialect. Every probe links and runs, including `static`,
   `funcptr` and a two-TU internal-symbol collision, so everything below this line can be
   verified again.
5. ~~**libstdc++ for `wasm32`.**~~ Was "C++ at all". **CLOSED, 2026-10-16 — see "Step 8",
   "Step 9" and "Step 10".** `cc1plus` builds, libgcc builds, a wasi-libc sysroot supplies
   the libc, libstdc++ configures — which needed a `crossconfig.m4` arm, not the
   `configure.host` stanza this file used to blame — and `make all-target-libstdc++-v3`
   now exits 0 with an archived `libstdc++.a`. The library *building* is no longer a gate
   on anything. What a program that links it needs is the unwinder under "Exceptions", and
   what the `std` module needs is the ABI mismatch above; neither is this heading.
5. ~~**Reflection and contracts on a new target.**~~ **CLOSED, 2026-10-12 — see
   "Step 1 and 2".** Both are frontend features and both turned out to be target-agnostic.
6. **32-bit cleanliness.** wasm32 is ILP32. The engine is x86-64 only. **Partly closed,
   2026-10-16 — see "Step 11".** The type-level half is done, by reading and by a host
   compile: the five places a 64-bit file size was narrowed to `size_t` for a whole-file
   allocation now report through the error channel each already had, and nothing outside
   `Archive` and `Pack` writes an engine-defined struct to a file. The half a 64-bit host
   cannot police is still open, and is now named rather than assumed: `size_t` arithmetic
   overflow, everything `-Wconversion` would say, and the address space itself. It was the
   cheapest item on this list and it stays the only parallelisable one — see
   "Engine-side consequences".
7. **Everything non-compiler** — WebGPU backend, Win32 → browser, threads → workers. Gated
   on every compiler-side item above, so do not start it.

## Why not GCC IR to LLVM IR

Recorded because it was the original proposal and it will come up again.

- GIMPLE has no stable serialization. The only on-disk form is LTO bytecode: undocumented,
  version-locked, readable only by linking GCC's internals. The work is a plugin against
  private headers, on a tree that pulls a fresh trunk snapshot weekly through a patch stack
  that already breaks on drift.
- GIMPLE is past the C++ frontend *and* partly target-lowered. Exceptions are region trees
  plus `__cxa_*`, vtables are address arithmetic, atomics are target builtins, TLS is in a
  target model, and `-mavx2` has become x86 vector ops
  (`Engine/cmake/GSEEngine.cmake:36`). Wasm needs a different EH model, 128-bit SIMD only,
  and a different TLS model. The translation runs *upward* from lowered code, and the
  information needed to do it correctly was discarded before GIMPLE existed.
- DragonEgg — LLVM's backend as a GCC plugin, the closest anyone got to bridging these two
  IRs — had LLVM's funding and was abandoned as unmaintainable against GCC churn.
- It does not remove the dependency it was proposed to remove. A translator inherits
  whatever frontend fed it, so GCC trunk stays on the critical path either way.
- And it is now moot: the supported path exists upstream and is 2,600 lines, not a
  middle-end fork.

The premise behind the proposal was right — we cannot switch compilers, so GCC must be the
frontend. Only the mechanism was wrong.

## Why GCC must be the frontend

Settled on direct experience rather than survey: the clang P2996 fork is too immature —
materially more breaking bugs than GCC trunk, which works. Path A (Emscripten/clang) is
closed, and with it any plan needing a non-GCC frontend.

The lock-in is large and not worth re-measuring: 461 files under `Engine/` use reflection,
contracts or `import std`; `Math/Units/Quantity.cppm` alone has 90 reflection uses; even
the test runner is reflective (`Engine/Tests/Main.cpp:15`).

## Q1 — Where does GCC commit to arbitrary jumps?

**Answer: at `pass_outof_cfg_layout_mode`, and it does not block us — the commitment is to
a linear insn chain, not to a jump encoding.** Upstream's `INSERT_PASS_BEFORE
(pass_free_cfg, 1, pass_count_labels)` confirms the window is usable.

### The late RTL pipeline

From `gcc/passes.def:464-571`:

| Line | Pass | Note |
|---|---|---|
| 464 | `pass_into_cfg_layout_mode` | edges authoritative from here |
| 505 | `pass_outof_cfg_layout_mode` | **jumps materialized here** |
| 518 | `pass_ira` | skipped for us, see Q2 |
| 519 | `pass_reload` | skipped for us |
| 523 | `pass_postreload` (container) | skipped for us |
| 562 | `pass_free_cfg` | frees the insn→bb map only; **upstream inserts its pass just before this** |
| 563 | `pass_machine_reorg` | `TARGET_MACHINE_DEPENDENT_REORG` |
| 568 | `pass_shorten_branches` | no-op for us, no `length` attr |
| 571 | `pass_final` | linear walk, target prints each insn |

In cfglayout mode the CFG edges are the authority and branches for fallthrough and block
order are **not** materialized. `fixup_reorder_chain` does it in two explicit phases:

- `gcc/cfgrtl.cc:3918` — "First do the bulk reordering -- rechain the blocks without
  regard to the needed changes to jumps and labels."
- `gcc/cfgrtl.cc:3960` — "Now add jumps and labels as needed to match the blocks new
  outgoing edges."

So `pass_outof_cfg_layout_mode` (`gcc/cfgrtl.cc:3805`, calling `cfg_layout_finalize` at
3813) is the commitment point.

### Why the commitment is not fatal

**The CFG survives, and is recomputable after it is freed.** `pass_free_cfg` only calls
`free_bb_for_insn()` (`gcc/cfgrtl.cc:502`) — it drops the insn→bb mapping, not the ability
to rebuild it. nvptx relies on exactly this and says so:

> `/* We are freeing block_for_insn in the toplev to keep compatibility with old
> MDEP_REORGS that are not CFG based.  Recompute it now.  */`
> `compute_bb_for_insn ();`
> — `gcc/config/nvptx/nvptx.cc:5809-5811`

`nvptx_reorg` then runs `df_analyze()` and calls `thread_prologue_and_epilogue_insns()`
itself (`nvptx.cc:5812-5828`). Heavy CFG-based work in machine_reorg on a virtual target is
established practice.

**The target owns the emitted text.** `pass_final` walks the insn chain linearly and asks
the target to print each insn; there is no target-independent notion of a jump
*instruction* to fight, only of insn *order*. Corroborating: nvptx.md declares three
`define_attr` and none is `length`, so `pass_shorten_branches` has nothing to shorten.

### Registering a target pass

`PASSES_EXTRA` in the target's `t-<arch>` fragment. `bpf` is the in-tree precedent and was
the only user before the wasm RFC:

- `gcc/config/bpf/t-bpf:17` — `PASSES_EXTRA += $(srcdir)/config/bpf/bpf-passes.def`
- `gcc/config/bpf/bpf-passes.def:19` — `INSERT_PASS_BEFORE (pass_lower_omp, 1, pass_lower_bpf_core);`

### Irreducible control flow

Smaller than it looked, and now doubly so. GCC already finds it:

- `gcc/cfg-flags.def:52` — `DEF_BASIC_BLOCK_FLAG(IRREDUCIBLE_LOOP, 2)`
- `gcc/cfg-flags.def:146` — `DEF_EDGE_FLAG(IRREDUCIBLE_LOOP, 7)`
- `gcc/cfgloopanal.cc:75` — `mark_irreducible_loops ()`, which "throw[s] away all latch
  edges and mark[s] blocks inside any remaining cycle" (`cfgloopanal.cc:62-66`)

And upstream's `br_table` dispatch loop is correct for *all* CFGs, reducible or not, so
there is no correctness gap here at all — only codegen quality.

## Q2 — Can a target present an infinite register file?

**Answer: yes. nvptx does not allocate registers at all, and the wasm RFC makes the same
choice. PRE-SOLVED.**

GCC has a documented target flag for exactly this shape of target:

> `TARGET_NO_REGISTER_ALLOCATION` — "True if register allocation and the passes following
> it should not be run. **Usually true only for virtual assembler targets.**"
> — `gcc/doc/tm.texi:10547-10551`, defined at `gcc/target.def:7470-7474`

nvptx is the only in-tree user (`gcc/config/nvptx/nvptx.cc:7911-7912`); the wasm RFC sets
`#define TARGET_NO_REGISTER_ALLOCATION true` as well.

What it buys, each independently verified:

- **IRA and reload do not run.** Both gates are literally
  `return !targetm.no_register_allocation;` — `gcc/ira.cc:6236` (`pass_ira`) and
  `gcc/ira.cc:6281` (`pass_reload`).
- **The whole postreload group is skipped**, while a parallel group still runs.
  `pass_postreload` gates on `reload_completed` and asserts the flag is false if it ever
  fires (`gcc/passes.cc:688-691`); `pass_late_compilation` gates on
  `reload_completed || targetm.no_register_allocation` (`gcc/passes.cc:732`). Documented at
  `passes.def:520-522`. Note `pass_reorder_blocks` (inside postreload, `passes.def:553`)
  never runs for us.
- **Pseudos are legal all the way into assembly output.** `output_operand`'s "X must not be
  a pseudo reg" assertion is explicitly waived:
  `if (!targetm.no_register_allocation) gcc_assert (… REGNO (x) < FIRST_PSEUDO_REGISTER);`
  — `gcc/final.cc:3646-3648`.

The emission mechanism maps 1:1 onto wasm locals. nvptx keeps a tiny hard register file,
`FIRST_PSEUDO_REGISTER 16` (`nvptx.h:115`), prints any non-hard regno directly —
`fprintf (file, "%%r%d", regno)` (`nvptx.cc:598`) — and declares every live pseudo at
function entry:

> `/* Declare the pseudos we have as ptx registers.  */`
> `for (int i = LAST_VIRTUAL_REGISTER + 1; i < maxregs; i++)` … `.reg<type> %r<i>;`
> — `gcc/config/nvptx/nvptx.cc:1620-1636`

For wasm the same loop emits `(local $i <type>)`. Unused pseudos are pruned first:
`nvptx_reorg` sets `regno_reg_rtx[i] = const0_rtx` where `REG_N_SETS` and `REG_N_REFS` are
both zero (`nvptx.cc:5833-5837`), and the declaration loop skips those.

**Cost in IRA: none, because IRA never runs.** The cost moves: nothing coalesces locals, so
one wasm local per pseudo. Upstream accepts this ("no register allocation of locals, no
folding of an address into the offset of a load"), as does LLVM initially.

## Q3 — Indirect calls and varargs

Read from nvptx; upstream's choices match, which is a good sign for both.

### Varargs — PRE-SOLVED, days of work

nvptx implements varargs in roughly 15 lines of target code and inherits the rest from
GCC's generic lowering. There is **no** `TARGET_SETUP_INCOMING_VARARGS`, no
`TARGET_MUST_PASS_IN_STACK`, no `va_list` type override, no `expand_builtin_va_start`
override — a grep for `VA_LIST|SETUP_INCOMING_VARARGS|MUST_PASS_IN_STACK` across
`gcc/config/nvptx/` returns **zero** hits.

The mechanism is one hidden trailing pointer to the outgoing-args buffer:

- `TARGET_STRICT_ARGUMENT_NAMING` → `nvptx_strict_argument_naming` (hook at
  `nvptx.cc:7877-7878`, body `nvptx.cc:763-778`), whose comment states the whole strategy:
  "for nvptx, we know how to handle functions declared as stdarg: by passing an extra
  pointer to the unnamed arguments."
- Unnamed args get no register, so the middle end spills them to the outgoing-args block:
  both `TARGET_FUNCTION_ARG` (`nvptx.cc:715-720`) and `TARGET_FUNCTION_INCOMING_ARG`
  (`nvptx.cc:725-738`) open with `if (arg.end_marker_p () || !arg.named) return NULL_RTX;`
- `ACCUMULATE_OUTGOING_ARGS 1` (`nvptx.h:170`) guarantees that contiguous block exists.
- Callee side: a stdarg function's prototype gains an extra pointer parameter, at
  `nvptx.cc:1558` (`if (stdarg_p (fntype))`).
- Caller side: the value passed is just the stack pointer, added as one more `USE` in the
  call PARALLEL — `nvptx.cc:1925` (`if (cfun->machine->is_variadic)`), with flags set at
  `nvptx.cc:1849-1855`.
- The caller materializes the buffer as a PTX `.local` array plus `cvta.local`, via
  `init_frame` (`nvptx.cc:1261-1276`), reached from `nvptx.cc:1581`.

So `va_list` is the default pointer, `va_start` the default expansion, `va_arg` the default
pointer bump — all needing only "load from a pointer at a computed offset". The ISA having
no varargs is irrelevant at this layer. Wasm's linear memory is a strictly better buffer
than PTX `.local`, and upstream already does this: `wasm_expand_call` detects `vararg_p`
and inserts the stack pointer as an implicit first argument.

### Indirect calls — half pre-solved, half genuinely new

Pre-solved: deriving an exact callee signature at the call site, from RTL modes, at
asm-output time. nvptx decides direct-vs-indirect on one line —
`bool needs_tgt = register_operand (callee, Pmode);` (`nvptx.cc:2787`) — then emits a local
label carrying a `.callprototype` before the call (`nvptx.cc:2811`) and appends that label
as the call's final operand (`nvptx.cc:2875`). The prototype text is reconstructed from the
call insn's PARALLEL of argument `USE`s rather than from any tree type
(`write_fn_proto_from_insn`, `nvptx.cc:1151-1193`, with `s << "\t.callprototype "` at
`nvptx.cc:1160`); the PARALLEL is built in `nvptx_expand_call` from `TARGET_CALL_ARGS`
records. The declaration-dedup-and-flush plumbing is also reusable: three hash tables plus
a `static std::stringstream func_decls` (`nvptx.cc:127`) emptied at `TARGET_ASM_FILE_END`
(`nvptx.cc:6134`).

Also worth copying: `NO_FUNCTION_CSE 1` (`nvptx.h:256`), which stops GCC turning direct
calls into indirect ones.

Absent from nvptx, because PTX function pointers are real addresses: a **function table
with slot assignment**, and **signature interning into type indices**. nvptx's
`IND_FUNC_MAP` (`nvptx.cc:5997-6037`) is an offload symbol map for `mkoffload`, not an ISA
table, and `.callprototype` is per-call-site text with no interning. Upstream's wasm
backend has had to build this — it emits `call_indirect` against a table — which is
precisely why "tables" and "reference types" appear on its missing-features list. Not our
problem to solve from scratch.

## Q4 — Minimum viable target

Mostly moot now that a backend exists, but it calibrates how big the remaining work is.

**Registration is trivially small** — the whole nvptx stanza in `config.gcc` is ~12 lines
across two places:

- `config.gcc:547-552` — `cpu_type`, `c_target_objs`, `cxx_target_objs`, `extra_options`
- `config.gcc:2930-2938` — `tm_file="${tm_file} newlib-stdint.h"`, `use_gcc_stdint=wrap`,
  `tmake_file="nvptx/t-nvptx"`

Note nvptx does **not** include `elfos.h`: a virtual target opts out of the ELF assembly
model entirely.

**Scale, for comparison:**

| | nvptx | wasm RFC |
|---|---|---|
| main `.cc` | 8,005 lines | ~2,600 total across 4 files |
| `.md` | 2,603 lines, 172 `define_insn` + 46 `define_expand` | 495 lines |
| `.h` | 399 lines | — |
| target hooks | 64 (`nvptx.cc:7840-8001`) | — |

Of nvptx's 64 hooks, ~22 are mandatory for any target (calling convention:
`FUNCTION_ARG`, `FUNCTION_INCOMING_ARG`, `FUNCTION_ARG_ADVANCE`, `FUNCTION_VALUE`,
`RETURN_IN_MEMORY`, `PASS_BY_REFERENCE`, `PROMOTE_FUNCTION_MODE`; asm output:
`PRINT_OPERAND`, `PRINT_OPERAND_ADDRESS`, `ASM_FILE_START/END`, `ASM_INTEGER`; modes:
`LEGITIMATE_ADDRESS_P`, `MODES_TIEABLE_P`, `HARD_REGNO_NREGS`, `CAN_CHANGE_MODE_CLASS`);
~20 are OpenACC/SIMT-specific and droppable. Several are satisfied by a literal or a stock
hook — `TARGET_OMIT_STRUCT_RETURN_REG true`, `hook_bool_const_tree_true`,
`TARGET_NO_REGISTER_ALLOCATION true`.

**The escape hatches are the real finding**, and they are all supported and documented:

- Function output fully overridden as text: `ASM_DECLARE_FUNCTION_NAME` and
  `ASM_DECLARE_FUNCTION_SIZE` redirected to C functions (`nvptx.h:262-268`).
- **Prologue/epilogue are printf'd text, not RTL.** `init_frame` (`nvptx.cc:1261-1276`) and
  `init_softstack_frame` (`nvptx.cc:1281-1330`) are pure `fprintf`. There is no `prologue`
  expander at all — only `epilogue` (`nvptx.md:1632`), and `return` delegates to
  `nvptx_output_return` (`nvptx.cc:1799-1811`), which prints text then returns `"ret;"`.
- Sections faked: "Fake 'sections support', just for 'exception_section'"
  (`nvptx.cc:6040-6042`), with `USE_ANCHORS_FOR_SYMBOL_P → false`.
- Deferred text buffered into a `std::stringstream` and flushed at `ASM_FILE_END`
  (`nvptx.cc:127` → `:6134`) — exactly the hatch wasm needs for type and table sections,
  and exactly what upstream does with imports and globals.
- `sorry()` as a first-class strategy: EH (`nvptx.md:1643`), nonlocal goto, label values.
  A target can be legitimately in-tree while refusing whole subsystems.
- `ASM_OUTPUT_DEF` defined purely to assert (`nvptx.h:335-355`), with a comment admitting
  as much.

## Revised plan

The spike's step 2 ("write a walking skeleton target") is **cancelled** — upstream has one.
Replacing it:

1. **Get the series and build it. DONE — see "Step 1 and 2: what was measured".**
   There was a v2 (five patches, a Sourceware Forge PR, 2026-05-28), which supersedes the
   RFC and folds in the x86 prerequisite. It is vendored at `Tools/gcc-toolchain/wasm/`
   and built by `Tools/build_gcc_wasm.py`; `Tools/install_msys2.py` handles the MSYS2
   prerequisite.
2. **Measure the C++ gap, which is the actual decision. DONE — see below.** Every
   frontend answer came back better than expected; the libstdc++ answer came back as
   predicted, and is now the contribution target.
3. **Run the hand-written subject. DONE — see "Step 3: execution".** A hand-written
   `-fno-exceptions`, no-std C++ TU was the real first subject, and it executes: loops,
   branches and varargs all produce the right answer in node. It also turned up a blocker
   nobody had on the list — there is no working linker for this output. The `gse.math`
   probe is still the right first *engine* code, for the reasons the scope gave
   (`Import/Math.cppm` imports only `std` plus its own partitions, 166 reflection uses
   across `Math/`, its own oracle in `Math/Tests/Units.cppm`), and is still gated on
   `import std`.
4. **Contribute where we are blocked.** Linking is done and is ours, not upstream's —
   `0005` is a local patch and not offerable as it stands, because it swaps the assembler
   the series targets. Step 8 took libstdc++ as far as it goes without exceptions: the
   compiler, libgcc and a wasi-libc sysroot are all in place and the library configures.
   What is left, in order: **symbol aliases** (`ASM_OUTPUT_DEF`, small and plainly
   offerable upstream), then **exceptions**, which is the one that is not small. The
   committee has already blessed the direction, so this is upstream work with receptive
   maintainers rather than a private fork — and `0006`, unlike `0005`, is offerable as it
   stands.

Parallel and worth starting now because it is cheap and certain under every path:
**32-bit cleanliness** (the item of that name in "What actually gates us").

## Step 1 and 2: what was measured

Measured 2026-10-12 against GCC snapshot `17-20260524` plus the vendored v2 series, built
by `Tools/build_gcc_wasm.py` (target `wasm32-unknown-none`, `make all-gcc`). Reproduce with
`python Tools/build_gcc_wasm.py`.

| Question | Answer |
|---|---|
| Is there a newer series than the RFC? | **Yes, v2**: 5 patches, 84 files, 3,107 insertions, forge PR 160, merge base `r17-901` (2026-05-23) |
| Do the patches apply? | **Yes, cleanly** to the contemporary snapshot — line offsets only, zero rejects |
| Does `cc1`/`cc1plus` build for the target? | **Yes**, on a Windows host, after two small portability fixes of ours |
| Does it compile a trivial C++ TU? | **Yes**, and emits plausible wasm text — one `block` per basic block inside a `loop` with a `br_table`, exactly as the series describes |
| Does `-freflection` work? | **Yes.** `^^T` and `typename [:r:]` compile and constant-fold; a `static_assert` over a spliced type passes |
| Does `-fcontracts` parse? | **Yes.** A `pre` contract on a function compiles |
| Is there a libstdc++ for `wasm32`? | **Not built, and `--disable-libstdcxx` was passed.** This row used to read "`configure.host` has no `wasm` stanza at all, so there is no `std` module either" — the second half held, the first half was misleading. See "Step 8". |

**Reflection and contracts being target-agnostic is the single most valuable result here.**
It closes the **Reflection and contracts on a new target** item of "What actually gates
us" — that item only. The engine's heaviest dependency on GCC is not what stands in the
way.

### Exceptions are worse than "unsupported"

`wasm.cc:123` sets `TARGET_EXCEPT_UNWIND_INFO` to `UI_NONE`, so no unwind tables are
emitted. But a `try`/`catch`/`throw` TU **compiles without a diagnostic**, emitting calls
to imported `__cxa_allocate_exception`, `__cxa_throw`, `__cxa_begin_catch`,
`__cxa_end_catch` and `_Unwind_Resume`.

So the failure mode is not a build error that stops you. It is a module that references an
unwinder which does not exist for `wasm32`, with no tables for a personality routine to
read even if it did. The **Exceptions** blocker stands, and anyone measuring progress by
"did it compile" will mis-read it.

### Host portability of the series

The backend is developed on Linux and has two LP64/glibc assumptions that stop it building
on a mingw-w64 host. Both are fixed in
`Tools/gcc-toolchain/wasm/0002-local-Windows-host-portability.patch`, kept separate from
upstream's bytes, documented in the README there, and both are worth sending upstream:
`TARGET_ASM_INTERNAL_LABEL` declared with `size_t` instead of the hook's `unsigned long`,
and `open_memstream` for buffering the module body.

Two further things that cost time and are not in the series at all:

- **`wasm32-unknown-unknown` does not configure.** `config.sub` rejects OS `unknown`; the
  canonical triple is `wasm32-unknown-none`.
- **`wasm.h` hardcodes `CC1PLUS_SPEC "-stdlib=libc++"`**, which only exists if configure
  was given `--with-gxx-libcxx-include-dir`. Without it *every* C++ compile through the
  driver fails on an unrecognized option — not just the C++ selftest that surfaces it.

### What this does not establish

`make all-gcc` stops before any target library and there is no wasm32 libc here, so
nothing has been *linked*. Code now runs, though — see the next section, which supersedes
the "nothing has been linked or run" this paragraph used to claim. The engine's own code
is still gated on `import std`.

## Step 3: execution — GCC-compiled WebAssembly runs

Measured 2026-10-15, same compiler as above. `python Tools/build_gcc_wasm.py` now takes
probes that carry an expected value all the way through: `xg++ -S` → `wat2wasm` → `node`.

| Probe | What it proves | Result |
|---|---|---|
| `arith` | The `br_table` dispatch loop the backend emits for every CFG computes the right answer through a loop and a branch | **ran, 73** |
| `varargs` | The implicit stack-pointer argument and the outgoing-args block in linear memory work, exactly as Q3 predicted from nvptx | **ran, 100** |
| `funcptr` | `call_indirect` through the function table | expected red — **traps**, "table index is out of bounds" |
| `static` | Data segments and their addresses | expected red — returns **2002, want 2040** |

**The headline is the first two rows.** Unstructured control flow through a `br_table`
dispatch loop, and varargs through a hidden pointer, are the two mechanisms this whole
spike reasoned about from nvptx source. They are now observed working, not inferred.

Both reds have the same single cause, and it is not GCC: **there is no working linker for
this output.** GCC does emit data segments and function-table references — `(data (@sym
(align 4)) (i32.const 0) …)` and `(i32.const 0 (@reloc data $sym))` — but as
*relocations*, all at address zero, for a linker to place. Without one, two statics
overlap at address 0 (hence 2002, not 2040) and an indirect call indexes an empty table.
Upstream's "data sections missing" status line is therefore misleading: the segments are
there, the placement is not.

Why there was no linker, as step 3 read it: the annotations GCC emits are
[rwat](https://github.com/Spxg/rwat)'s, not wabt's, and no released rwat emits a
symbol-table entry for a *global*. GCC's `__stack_pointer` is an imported global, so
`wasm-ld` drops it and the linked module fails to validate on `global.get 0` — every
probe, not just the relocating ones. **Step 4 superseded this**: the global is worked
around by a single `--no-gc-sections`, and the actual blocker is a dialect mismatch
between GCC and rwat. The full measurement is in `Tools/gcc-toolchain/wasm/README.md`.

So step 3's harness ran only the **no-linker** path: wat2wasm with no flags (it skips
annotations it does not know and keeps the three `env` imports) plus a plain `(export …)`
appended, since the backend expresses the export only as a `(@sym (name …))` annotation
that wat2wasm ignores. It survives as the `unlinked` pipeline, which is still the one that
says whether the codegen is right. The reds ship as probes rather than prose: each is
marked `blocked` with the exact status it is known to fail with, reported red without
failing the run, and reported loudly if it ever starts passing. That makes each gap a
tripwire instead of a paragraph someone has to remember.

### What step 3 adds to the gating list

Linking now sits between exceptions and libstdc++: a `std` module that cannot be
linked is not usable either, and every engine TU has statics. Nothing else on the list can
be *verified* without it. Step 3 read the cause as one missing symbol-table entry; "Step
4: linking" measured it properly and it is six divergences, of which the global symbol is
the one with a workaround. Read step 4 for the cause, not this section.

## Step 4: linking — one TU links and runs, relocations do not

Measured 2026-10-15, same compiler. `Tools/build_gcc_wasm.py` now takes every run probe
through **two** pipelines and reports both, because "is this codegen right" and "does this
output survive a link" are different questions with different answers. `unlinked` is
wat2wasm alone, as in step 3. `linked` is the real one: rwat (pinned, `RWAT_VERSION`) to a
relocatable object, then `wasm-ld`.

| Probe | unlinked | linked |
|---|---|---|
| `arith` | ran, 73 | **ran, 73** |
| `varargs` | ran, 100 | wrong result — 65520 |
| `funcptr` | traps | assemble failed |
| `static` | 2002, want 2040 | assemble failed |

**The headline is `arith` linked.** A GCC-compiled wasm object now goes through a real
relocatable assembler and a real linker and still computes the right answer, which step 3
could not claim at all. Reaching it needed one flag and no patches: `--no-gc-sections`.
GCC's `__stack_pointer` is an *imported* global with no symbol-table entry, so with
collection on `wasm-ld` drops the import and every module dies on `global.get 0`; with it
off the linker substitutes its own `__stack_pointer` at the same index and the same 65536
initial value. So the global, which step 3 named as the blocker, is not one.

**The real blocker is that GCC and rwat do not share an annotation dialect.** Six separate
divergences, tabulated with their effects in `Tools/gcc-toolchain/wasm/README.md`. Two
decide the shape of the work:

- GCC spells a relocation *inside* the instruction — `(i32.const 0 (@reloc data $lo))` —
  while rwat's `(@reloc)` is a bare marker placed *after* one. rwat 0.1.10 rejects GCC's
  form; 0.1.7 parses and ignores it. And rwat implements no `MEMORY_ADDR` relocation and
  no data or global symbols at all, so statics are a new rwat *feature*, not a fix.
- GCC emits a direct call as plain `call $sym` with no marker, so rwat requests no
  relocation even for the one class it does implement. `wasm-ld` renumbers the function
  section anyway, and `call $total` becomes `call $__wasm_call_ctors`. **The module still
  validates.** That is why `varargs` regresses from a correct 100 to 65520 the moment it
  is linked, and it is the most dangerous row in the table: a silent miscompile that
  "it linked and ran" would have reported as success.

### The open decision

Which side moves is not ours to settle alone, and it was scoped on the assumption that
rwat needed three small additions. It needs more than that, and GCC needs changes too:

- **Patch rwat to GCC's dialect** — accept the in-instruction `(@reloc …)` forms,
  `(@sym local|weak …)`, and add data and global symbols plus `MEMORY_ADDR` relocations.
  The largest piece, and it asks a third-party crate to adopt a dialect with one producer.
- **Patch GCC to rwat's dialect** — emit `(@reloc)` as a trailing marker, mark direct
  calls, and drop the binding keywords rwat cannot parse. Smaller, and arguably correct:
  rwat is the tool GCC's own `ASM_SPEC` names, so GCC is the side that is off-spec. But it
  still needs the data and global symbol support rwat does not have.
- **Teach wabt the annotations** — rejected in scoping and still rejected: wabt has no
  linking or reloc-section writer at all.

The honest reading is that both sides move, and the first upstream conversation is worth
having before either patch is written. Until then the probe table is the tripwire: every
red row carries its measured status, so a change on either side announces itself.

**Settled 2026-10-15**, by the owner, against that recommendation: patch GCC first and have
the conversation with measurements in hand. See "Step 5" — it turns out the GCC side is
smaller than this section estimated, and the rwat side larger.

## Step 5: the GCC side of the dialect moves — three of five probes link

Measured 2026-10-15, same compiler, after `Tools/gcc-toolchain/wasm/0004-local-emit-rwat-s-annotation-dialect.patch`.
"The open decision" in step 4 above was settled by the owner in favour of patching GCC
rather than waiting on an upstream conversation, on the argument that section itself
records: `wasm.h`'s `ASM_SPEC` names rwat, so GCC is the side that is off-spec.

Two changes, both read out of rwat 0.1.10's own source rather than inferred from its
behaviour:

- **A direct call carries `(@reloc)`.** rwat only requests a relocation for an instruction
  carrying the marker. The marker does not have to trail the instruction textually, which
  is what step 4 assumed: rwat's scanner recurses into folded groups and records the `call`
  keyword's span, so `(call $f (local.get $a) (@reloc))` attaches correctly. Two tokens in
  `wasm.md`.
- **Sym decorations move to a sibling `(@gcc.binding …)`.** rwat parses only `(@sym)` and
  `(@sym (name "…"))`, so ` local`/` weak`/` hidden`/`(init_prio N)` inside the annotation
  was what made almost all real code fail to assemble. Moving rather than deleting them
  keeps the linkage in the text — rwat skips annotations it does not know — though not in
  the object, where every symbol still lands global and strong.

| Probe | unlinked | linked |
|---|---|---|
| `arith` | ran, 73 | ran, 73 |
| `varargs` | ran, 100 | **ran, 100** (was 65520) |
| `local_linkage` | ran, 49 | **ran, 49** (new probe; `assemble failed` before) |
| `funcptr` | traps | assemble failed |
| `static` | 2002, want 2040 | assemble failed |

`local_linkage` is new: a `static` callee, which is the shape the README calls "almost all
real code" and which nothing measured before, because `baseline`, `contracts` and
`reflection` carry no `expect` and are therefore compile-only.

**This does not close the Linking blocker.** The two red rows are not a dialect difference
and no GCC change can reach them: rwat implements no `MEMORY_ADDR` and no `TABLE_INDEX`
relocation and has no data or global symbol kind, so a static cannot be placed and a
function pointer cannot be formed. Those need a feature in rwat or a different assembler,
and the upstream conversation — about rwat's gaps now, not about who spells `(@reloc)` how
— is still owed. It can now be had with a working demonstration attached.

## Step 6: the LLVM dialect, measured — it carries everything rwat lacks

Measured 2026-10-15 by `python Tools/wasm_dialect_probe.py`. No GCC build is involved:
the subjects are **hand-written LLVM-dialect `.s`**, taken through `llvm-mc` (LLVM
23.1.3, the same release as the `wasm-ld` the linked pipeline already uses), then
`wasm-ld`, then node.

Step 5 left the Linking blocker entirely on the assembler side, and closing it means
either forking rwat or moving GCC onto llvm-mc's dialect. The second option's cost
rested on an estimate; the owner chose to buy a measurement first, on the argument
that an estimate is a bad thing to commit a 170-template rewrite to.

| Subject | Result | What it proves |
|---|---|---|
| `static` | **ran, 2040** | Data symbols, `segment info`, `R_WASM_MEMORY_ADDR_SLEB` |
| `funcptr` | **ran, 35** | `R_WASM_TABLE_INDEX_I32` *in a data section*, and elem-segment synthesis by wasm-ld |
| `two_tu` | **ran, 249** | `binding=local`, so a same-named internal symbol in two objects does not collide |

These are the two probes that are red through rwat, plus one hazard nothing measured
before. **All three are green, and the relocations are confirmed by name** in
`wasm-objdump -x` rather than inferred from the returned number — a green run alone
cannot tell "the dialect carries this" from "the bytes happened to land at address
zero", which is exactly the confusion that made step 3's `static` read as a codegen
bug rather than a missing linker.

Three findings beyond the table:

- **`--no-gc-sections` is retired on this path.** It exists in the rwat pipeline only
  because GCC's imported `__stack_pointer` carries no symbol. `llvm-mc` gives that
  global a real one, so the subjects link with collection *on*, which is the default
  the script uses in order to keep measuring it.
- **`llvm-mc` type-checks the operand stack.** `two_tu` first failed with "type
  mismatch, expected [i32, i32] but got [i32]". The folded s-expression form gets that
  checking free from nesting; a flat port would be hand-verified against an assembler
  that does at least catch arity errors.
- **Every external callee needs a forward `.functype`.** `call part_b` is rejected
  outright without one, which is extra bookkeeping for GCC's output layer — though it
  already has the deferred declaration-and-flush machinery that needs
  (`wasm_asm_file_end`).

### What this does and does not settle

It closes the open unknown: **the LLVM dialect is proven as a target**, so nothing
about that path is unmeasured except the `wasm.md` port itself, whose cost this section
estimated at ~170 folded templates rewritten to flat stack order. **That figure was
wrong** — `wasm.md` has 55 `define_*` of which ~35 carry a template, and they are nearly
all one shape; see "Step 7". The decision between
the two paths is now a cost call with no unknowns in it, which is what it was bought
for.

It does **not** make the choice, and three things still argue for the rwat fork:
the port is large and hand-verified with no compiler checking the result; it lands on
the churniest part of an out-of-tree series and is re-applied every snapshot bump, on a
patch stack that already breaks on drift; and it abandons the assembler `wasm.h`'s own
`ASM_SPEC` names. Against that, llvm-mc is the only path that also has DWARF and the EH
directives behind it — so the trigger for taking it is needing debug info or exceptions,
not a date.

Note the reference-text mitigation the plan preferred was not available: MSYS2's rustc
is not rustup-based, so there is no `core` for `wasm32` and
`rustc --target wasm32-unknown-unknown --emit asm` cannot produce LLVM-authored asm to
diff against. Structural read-back through `wasm-objdump -x` stood in for it, and each
subject asserts its required object facts by name so the corroboration is enforced
rather than eyeballed.

## Step 7: GCC emits the LLVM dialect, and the Linking blocker closes

Measured 2026-10-15, same compiler, after
`Tools/gcc-toolchain/wasm/0005-local-emit-llvm-mc-s-assembly-dialect.patch`. The owner
chose this over forking rwat on the argument step 6 ends with: llvm-mc is the only path
that also has DWARF and the EH directives behind it, and exceptions are the blocker we
have to reach anyway, so the rwat work would have been paid for twice.

| Probe | linked |
|---|---|
| `arith` | ran, 73 |
| `varargs` | ran, 100 |
| `local_linkage` | ran, 49 |
| `funcptr` | **ran, 35** (was `assemble failed`) |
| `static` | **ran, 2040** (was `assemble failed`) |
| `two_tu` | **ran, 249** (new probe) |
| `extern_varargs` | ran, 66 (new probe; see "Two things found on the way") |
| `sret` | ran, 68 (new probe) |

**Every probe is green, from GCC's own output.** `static` and `funcptr` are the two rows
the whole dialect question was about, and `two_tu` is the hazard nothing measured before:
two objects each defining an internal `scale`, which rwat would have collided because it
emitted every symbol global and strong.

### What the port actually cost, against the estimate

Step 6 put it at ~170 templates. The real shape, read out of the sources before any of it
was written:

- `wasm.md` has **55 `define_*`, ~35 with a template**, and nearly all are the one shape
  `"(%M0.set %0 (<types>.OP (%M1.get %1) (%M2.get %2)))"`. The port is therefore mostly a
  redefinition of the operand codes in `wasm_print_operand` plus a mechanical
  prefix-to-postfix reorder — not 170 bespoke rewrites.
- The risk was in the framing instead, and in two places the estimate did not mention:
  **locals become positional indices** (so `$local_7` has no spelling and the index space
  has to be built by the same walk that declares it) and **branches become relative
  depths** (so a `block_depth` counter has to track the dispatch staircase).

Three things were measured on hand-written subjects first, because each would otherwise
have been a guess baked into the patch. They are now permanent subjects in
`Tools/wasm_dialect_probe.py`:

| Subject | Answer |
|---|---|
| `blocktype` | **Green.** A `loop` carries a result type, `br_table` takes `{0, 1, …}`, and a branch names its target by a depth counted from the branch. The framing design stands. |
| `env_import` | **Green.** `.import_module`/`.import_name` make a function an `env` import that survives the link. |
| `late_functype` | **Red, deliberately.** A signature declared *after* the call that needs it is rejected outright. |

`late_functype` is the one that changed the design: every `.functype` the unit needs has
to be flushed into the module header ahead of the body, so the body buffering `0002`
introduced for host portability is now load-bearing for correctness.

### Two things found on the way

- **Upstream derives a function's signature from its type in two places that disagree**,
  and `0005` keeps only one. The import path (`print_type` on a `FUNCTION_TYPE`) listed
  the declared arguments and nothing else, and ran the list's trailing `void` through
  `pass_by_reference`, which answers yes and invents a parameter. The definition path
  (`print_func_type`) added the three parameters that are not in the argument list — a
  leading pointer for a result returned in memory, a trailing one to the outgoing-args
  block for a varargs or unprototyped function — and took the result through
  `aggregate_value_p`. Nothing compared them, because nothing type-checked a signature
  against a call until llvm-mc did. The case where it bites is an **external varargs
  callee**: header `(i32) -> (i32)`, two values pushed at the call site, module traps.
  `extern_varargs` is the probe, and it is red against the first version of `0005`.
- **Code and data cannot be interleaved.** llvm-mc emits a module's sections in the order
  it meets them and dies with "only data supported in data sections" on code reached after
  a data section, while GCC emits functions and objects in declaration order. Objects are
  now collected into a second buffer and appended after every function.

One thing worth knowing because it reads like a bug and is not: for a result returned in
memory the return register is still live, because that is where the hidden pointer is
handed back. So such a function declares a return local while its signature has no result,
and `return` must *not* read that local. Those are one question asked of two different
facts, and `result_on_stack` is now the only one asked — the signature's own result list.
The old spelling pushed a value the signature said was absent, which wasm discards rather
than rejecting, so it would never have surfaced as a failure. `sret` is the probe.

### What this costs and what it does not close

- The `unlinked` pipeline is **gone**, not ported. It assembled the emitted module text
  with wat2wasm, and the output is no longer module text. It was the thing that said "the
  codegen is right" independently of "the output can be linked", so a miscompile now
  surfaces only through the link. That was the right trade once linking works, but it is a
  real reduction in how far a red row narrows itself.
- **Static constructors do not run.** Upstream marked them with an `(init_prio N)`
  annotation no assembler read; the equivalent here is an `.init_array.N` section, which is
  not implemented. Nothing regressed — rwat skipped the annotation too — and no probe
  covers it, which is exactly why it is written down here.
- `0005` is **ours and not offerable upstream as it stands**: it swaps the assembler the
  series targets, which is a conversation to have with its author, not a patch to send.
  The demonstration to have it with now exists.

## Engine-side consequences

- **wasm32 is ILP32.** 32-bit pointers and `size_t`; the engine is x86-64 only. Note the
  shipped toolchain is `--disable-multilib`
  (`Tools/gcc-toolchain/recipe/PKGBUILD:287`), so there is no free `-m32` probe without a
  toolchain rebuild.
- **`-mavx2` is unconditional** (`Engine/cmake/GSEEngine.cmake:36`) against a `Math/:simd`
  partition, while wasm offers only 128-bit SIMD.
- **124 files use coroutines, exceptions or varargs.** Coroutines lower in the frontend to
  reducible state machines, which upstream's `br_table` dispatch handles trivially.
  Exceptions are the blocker — see "What actually gates us".
- **Around 111 files `catch`, and under step 12's unwinder every one of those paths
  terminates.** That is the engine-side job the unwinder does not do and cannot: a
  `catch` compiles, links and never runs, so the failure is an `abort` at the throw
  rather than a diagnostic anywhere. It is a real port — each site is a decision between
  an error channel the caller already has and a genuinely fatal condition — and it is the
  one engine-side item that is *not* gated on the compiler, since the shape of the answer
  does not depend on whether real exceptions arrive later.
- **Graphics has a usable seam.** `backend_kind` dispatches at runtime
  (`Gpu/Device/Device.cpp:52`) and shader codegen already branches SPIR-V vs DXIL
  (`GpuRecord/PipelineBuilder.cpp:165`). A WebGPU backend is a known-shaped third-backend
  job; DX12 proved the seam. Gated on the compiler, so do not start it.
- **Platform surface is contained.** 29 files touch Win32/Vulkan/D3D12 directly,
  concentrated in `External/`, `Vulkan/`, `Dx12/`, `Os/Win32/`.

## Sources

- `[RFC PATCH 0/3] wasm: New backend` — https://gcc.gnu.org/pipermail/gcc-patches/2026-May/715824.html
- Backend wiki and feature status — https://gcc.gnu.org/wiki/WebAssemblyBackend
- Steering committee decision — https://www.phoronix.com/news/GCC-Steering-WebAssembly
- Backend proposal coverage — https://www.phoronix.com/news/GCC-WASM-WebAssembly
- Ramsey, "Beyond Relooper: Recursive Translation of Unstructured Control Flow to
  Structured Control Flow", ICFP 2022 — the better structuring algorithm, named in
  discussion around the series

## Step 8: a wasm32 libstdc++ — libgcc, constructors, and wasi-libc

Measured 2026-10-15, same compiler and same snapshot, after re-cut
`0005-local-emit-llvm-mc-s-assembly-dialect.patch` and a new
`0006-local-host-libstdcxx-rather-than-libcxx.patch`. The step is four increments, each
gating the next, because none of them could be started before the one above it.

### The premise this step had to correct first

Step 1-2's table said libstdc++ was missing because "`libstdc++-v3/configure.host` has no
`wasm` stanza at all". The conclusion held; the reason did not, and it pointed at the wrong
work. `configure.host` has a `*)` fallthrough (`:313` → `os_include_dir=os/generic`) and
`cpu_include_dir` defaults to `cpu/generic` when `config/cpu/<cpu>` is absent, which for
`wasm32` it is — so libstdc++ configures on generic defaults with no stanza at all. What
actually blocked it was three things nobody had written down: no libgcc, no constructors,
and no libc for its configure tests to link against.

The `std` module *is* gated, and that part of the row stands:
`libstdc++-v3/src/c++23/Makefile.am:83` builds `std.cc` only `if GLIBCXX_HOSTED`. So the
engine needs a **hosted** libstdc++, not the freestanding subset, and that settles what
would otherwise have been this step's open question.

### 8a — libgcc, and the one line that had kept it unbuildable

`make all-target-libgcc` had never been run. It fails at the first compile, and the cause
is not in the backend at all:

```
C:/.../build/gcc/as: $original is unset
This probably means there is no 'as' for the target.
```

A probe stops at `-S` and is handed to llvm-mc by the script, so the driver never looks for
an assembler. A target library compiles to an object *through the driver*, which finds the
build tree's `gcc/as` exec-tool shim — before PATH, so no amount of PATH helps — and that
shim refuses unless top-level configure was told which assembler the target has. One
`--with-as` (and `--with-ld` beside it, for the same reason one layer out) is the whole
fix, and `libgcc/config/wasm/t-wasm` needed nothing added to it.

It then failed twice more, both times on *where the target's headers are*, and both are
worth recording because neither symptom names its cause.

The first: `stdio.h: No such file or directory`, while `xgcc -print-sysroot` reports
exactly the right sysroot. **A configure argument naming a POSIX-absolute path does not
survive an MSYS2 host.** `--with-native-system-header-dir=/include` reaches the Makefile
intact and then arrives at the *compile* as `C:/…msys64/include`, because the argument
converter reads a bare `/include` in a `-D` as a POSIX path; the sysroot is prepended to
*that*, and the result cannot exist. `xgcc -E -v` says so only in its *ignoring nonexistent
directory* lines, which come before the search list and are the easy part of that output
to cut off.

The second: having moved the macro into `wasm.h` as a string literal, `stmp-fixinc` then
failed on `BUILD_SYSTEM_HEADER_DIR … does not exist`, naming `<sysroot>/usr/include` —
because `fixincludes` reads the *Makefile's* value, which had gone back to the default.
So the directory has to be stated in both places, which is upstream's own pattern rather
than a redundancy: `config/mingw/mingw32.h` `#define`s the macro against the same bug
(PR 52947) while `config.gcc` sets `native_system_header_dir` beside it. `0006` does the
same for wasm, and `build_gcc_wasm.py` passes no configure flag for it at all.

Past the headers, two more things were missing and neither is in the series: `libgcc.a`
cannot be *archived* (`wasm32-unknown-none-ar: command not found` — there is no binutils
for this target, so `AR_FOR_TARGET` and `RANLIB_FOR_TARGET` have to name `llvm-ar` and
`llvm-ranlib`), and `libgcov` wraps `fork` and the six `exec` functions, which wasi-libc
does not have. The gcov wrappers cannot be dropped from a `t-` fragment —
`libgcc/Makefile.in` includes `tmake_file` at `:382` and assigns `LIBGCOV_INTERFACE` at
`:923`, so the assignment wins — but `--disable-gcov` exists and says exactly the right
thing.

**libgcc now builds, and the `libcall` probe runs.** Finding something that *calls* libgcc
took measuring rather than guessing, and the answer is interesting about the target: wasm
has i64 divide, modulo, shifts and every float conversion as single opcodes, `__int128` is
rejected outright on a 32-bit target, and `long double` is `double` here. So none of the
shapes that reach libgcc on a normal soft-float target do so on wasm. `__builtin_powi`
does — `__powidf2`, which no ISA has — and it is the `libcall` probe.

### Two backend bugs libgcc found that no probe could have

Both are upstream's, both were invisible until something type-checked the output, and both
are the same shape as step 7's "two derivations that disagreed":

- **Float constants were spelled in wat.** `f64.const +inf` — the assembler takes
  `infinity` and rejects both `inf` and a leading `+`. libgcc's complex multiply and
  divide (`_muldc3`, `_divdc3`, `_mulsc3`, `_divsc3`) are the first code to emit an
  infinity, which is why thirteen green probes had never reached it. NaN is worse than a
  spelling: a bare `nan` is the *only* NaN the dialect can express — no sign, no
  `:0x<payload>` — so a non-canonical one now calls `sorry ()` rather than silently
  emitting a different constant. `print_nan_payload` is deleted with the suffix it wrote.
  A stray `)` went with it, left over from the folded form on the one path through
  `CONST_DOUBLE` that no probe takes.
- **A libcall's signature was declared backwards.** `wasm_expand_call` says in its own
  comment that `cfun->machine->call->args` is in reverse argument order, and builds the
  call PARALLEL by reversing it; `record_libcall` read the same vector *forwards*. So
  `__powidf2 (double, int)` was declared `(i32, f64)` while the call site pushed
  `(f64, i32)`. It is invisible unless a callee has two parameters of **different** types,
  and then only against an assembler that type-checks the stack — which is to say, only
  since step 7. Nothing about the emitted call was wrong; only the declaration.

### 8b — static constructors now run

Step 7 recorded this as an absence with no probe. It stops being a footnote here, because
`ios_base::Init` and the locale tables are static constructors: a hosted libstdc++ without
this does not work and does not say so.

The spelling was settled on a hand-written subject before any of it was written, which is
now `init_array` in `Tools/wasm_dialect_probe.py`. `.section .init_array.<prio>,"",@` plus
a `.int32` of the function becomes an object init-function record, which `wasm-ld` collects
across objects into a synthesised `__wasm_call_ctors`. Two things that are easy to get
wrong: the section kind must be a bare `@`, because `@init_array` — how the directive reads
on an ELF target — is rejected by the parser outright; and the entry is data, so it goes
into the data buffer rather than beside the code.

`TARGET_ASM_CONSTRUCTOR` was a no-op lambda, with the priority riding on upstream's
`(init_prio N)` symbol decoration. There is deliberately no destructor hook: wasm has no
`.fini_array` and the linker synthesises nothing for one, so a destructor runs only by
having been registered with `__cxa_atexit` from a constructor.

### 8c — wasi-libc as the target libc

libstdc++ cannot be configured `--without-headers`, because its feature tests compile and
link. So a libc had to come first, and wasi-libc is the one that exists — pinned at
`WASI_SYSROOT_VERSION`, downloaded and flattened by `build_gcc_wasm.py`.

Three things made it far cheaper than it looks, and two of the three are step 7's doing:

- **Its objects are ordinary wasm objects**, so `wasm-ld` links them beside GCC's without
  anything knowing which compiler produced which. The annotated dialect could never have
  had this.
- **node hosts WASI itself** (`node:wasi`, preview1), so the harness grew four lines rather
  than an import table.
- **Upstream already aimed here.** `wasm.h`'s `STANDARD_STARTFILE_PREFIX_1` is
  `/lib/wasm32-wasi/` and its `LIB_SPEC` is `-lc`, which is why the sysroot is flattened
  into exactly that layout rather than renamed to something tidier.

Hosted probes link `crt1-reactor.o` rather than `crt1.o`: the command form defines `_start`
and expects to be the whole program, while a reactor exposes `_initialize` and stays
callable, which is what a probe whose `entry` the harness calls needs. `wasi.initialize`
runs `__wasm_call_ctors` itself, so the harness's two constructor paths are alternatives
rather than a sequence.

**This makes the measurement target WASI, not a browser.** A browser build needs either a
WASI shim in JS or the engine's own host layer in place of one. That is a known-shaped job
and the engine needs a host layer regardless, but nothing here has measured it.

### 8d — the hosted build

`--disable-libstdcxx` is gone, `--disable-threads` stays (wasi-libc's non-threads build has
no pthreads, and the threads variant of the sysroot is a later question than whether `std`
works at all), and no `configure.host` stanza turned out to be needed.

**`configure.host` needed nothing. `crossconfig.m4` did, and that is the correction that
matters.** The scoping read was half right: `configure.host` falls through to `os/generic`
and `cpu/generic` on its own. But `libstdc++-v3/crossconfig.m4` has a target case list with
no `wasm` arm and a final `*)` that is `AC_MSG_ERROR([No support for this host/target
combination.])` — so libstdc++ refused the target outright, and *that* is why there was
never a wasm32 libstdc++. `0006` adds `wasm*-*-*` to the existing `*-mingw32*` arm rather
than writing a new one: every line of that arm is a feature *check* rather than an
assertion, and the set is right here — a real libc with math and stdlib, no `/dev/random`,
no iconv, no `uselocale`. Both `crossconfig.m4` and the generated `configure` are patched,
because nothing in this build runs autoconf.

One hardcode had to go, as `0006`'s other half: `wasm.h` forced `-stdlib=libc++` onto every
C++ compile through `CC1PLUS_SPEC`. That was a reasonable guess while the target had no C++
runtime and is actively wrong once it has one — it makes the driver look for libc++'s
headers and refuse libstdc++'s. Removing it also retires the
`--with-gxx-libcxx-include-dir` pointing at a directory that never had to exist, which was
only ever there to make that option legal.

### wasi-libc's headers are not C++-clean, and the pin is how that is managed

Three of its headers use C keywords C++ does not have, and which ones depends on the
release — these are only as C++-clean as the clang they were last tested against:

| Release | What breaks C++ |
|---|---|
| 27.0 | `wasi/api.h`: bare `_Static_assert (_Alignof (…))`, reached by the first `<cstdio>` |
| 32.0 | `__errno.h`: `extern _Thread_local int errno`, reached by the first `<cerrno>` |
| 34.0 | the same `_Thread_local`, moved to `errno.h` |

32.0 is the pin, and `-D_Thread_local=__thread` covers the one remaining construct.
`__thread` rather than C++11's `thread_local` because libstdc++ compiles part of `src/` as
C++98, where `thread_local` is not a keyword either and the define would only move the
error. `signal.h` is a third case and a documented one: it is an `#error` unless
`-D_WASI_EMULATED_SIGNAL` is defined and `-lwasi-emulated-signal` is linked, which is
wasi-libc's own recipe.

### Where 8d actually stops: `<csetjmp>`, which is the Exceptions item

**Exceptions were left on deliberately**, and they are what this step ends on — earlier
than expected, and not where the plan predicted. The prediction was a link error naming
missing `_Unwind_*` symbols. The reality is a **compile** error, two layers out:

```
sysroot/include/setjmp.h:13:2: error: #error Setjmp/longjmp support requires Exception
handling support, which is [not yet standardized] …
```

wasi-libc refuses to declare `setjmp` at all without the wasm EH proposal, and libstdc++
precompiles `<csetjmp>` into `stdc++.h.gch`. Nothing in libstdc++ *calls* setjmp; it is the
declaration that cannot exist. Defining `__wasm_exception_handling__` to get past it would
be a lie about a capability, so it is not done.

Behind that there is a **second, separate** wall, measured by building `libsupc++`
directly: `libsupc++/tinfo.cc:55: error: alias definitions not supported in this
configuration`. The backend implements no `ASM_OUTPUT_DEF`, so `__attribute__((alias))` is
unsupported, and libsupc++ needs it. That one is not about exceptions at all and is new to
the gating list.

So the honest 8d result is: **libstdc++ configures and its headers install; the library
does not build.** `include_vector` and `import_std` ship as `blocked` probes carrying those
exact statuses, so each is a tripwire rather than a paragraph someone has to remember.

### The probe table

| Probe | Status | What it adds |
|---|---|---|
| `arith`, `varargs`, `local_linkage`, `funcptr`, `static`, `extern_varargs`, `sret`, `two_tu` | ran | unchanged from step 7 |
| `init_array` | **ran** (new) | static constructors run |
| `libcall` | **ran** (new) | libgcc exists and is called correctly |
| `libc` | **ran** (new) | malloc, strcpy and printf under a real WASI host |
| `include_vector` | blocked: compile failed | libstdc++ does not build — `<csetjmp>`, then aliases |
| `import_std` | blocked: std module failed | downstream of the row above |

`libc` running is the headline. A GCC-compiled wasm32 module links against wasi-libc,
instantiates under node's own WASI host, allocates, copies a string and writes to stdout.

## Step 9: one-only and aliases — what libstdc++ asks of a C++ target

Measured 2026-10-16, same snapshot, with a new `0007-local-one-only-and-aliases.patch` and
one edit to the flattened sysroot. Step 8 ended on two reds and read them as two separate
walls, one of them "the Exceptions item". Only one of those readings survived.

### The alias wall was never about aliases

`libsupc++/tinfo.cc:55: alias definitions not supported in this configuration` names the
symptom at the point it is noticed, four derivations below its cause. Each link is one line
in the tree:

| Where | What it says |
|---|---|
| `gcc/defaults.h:300` | `SUPPORTS_ONE_ONLY` is 1 iff `MAKE_DECL_ONE_ONLY` is defined |
| `gcc/c-family/c-cppbuiltin.cc:952` | `__GXX_WEAK__` comes from `SUPPORTS_ONE_ONLY` |
| `libstdc++-v3/libsupc++/typeinfo:77` | `__GXX_TYPEINFO_EQUALITY_INLINE` defaults to `!__GXX_WEAK__ ? 0 : 1` |
| `libstdc++-v3/libsupc++/tinfo.cc:33` | the `0` branch defines `type_info::__equal` as an `__attribute__((alias))` |

`0001` defines `SUPPORTS_WEAK 1` and no `MAKE_DECL_ONE_ONLY`, so the whole chain fires. **A
target without link-once semantics is a target whose libsupc++ cannot be compiled**, with
or without `ASM_OUTPUT_DEF` — which is why the fix is one-only and the alias support is a
second, independent thing that happens also to be cheap.

That it was mis-read is worth recording as a method note, not an apology: the step-8
measurement (build `libsupc++` directly, read the first error) was sound, and the error
message named a real missing feature. What it could not do is say which of the two
candidate causes it was, and nothing had checked whether the branch that needs the alias
was one the target chooses or one it is forced into.

### 9a — the dialect carries both, measured before writing either

Two subjects added to `Tools/wasm_dialect_probe.py`, both green, which is what settled the
shape of the patch before a line of it existed:

- **`comdat`** — two objects defining the same function into
  `.section .text.dup,"G",@,dup,comdat`, linked. The group survives into the object
  (`comdat groups [count=1]`) and one copy reaches the module.
- **`alias`** — `.set` on a function symbol and on a data symbol, both called and read
  through the alias from a second object. Both resolve to the target's own function index
  and segment offset, which is the claim; a symbol that merely existed would link too.

`comdat` carries a second fact its assertion pins down, and it cost an hour before it was
written down: **plain `.text` does not end a comdat group.** A function emitted after a
one-only one joins that group, and the linker discards the function along with the
duplicate copy — reported as `undefined symbol` for a symbol the object plainly defines,
which reads like a lost relocation and is not one. Hence every symbol getting a section of
its own, one-only or not, which is also what clang emits.

### 9b — `0007`, in four parts

`MAKE_DECL_ONE_ONLY` as `DECL_WEAK (DECL) = 1`, so `varasm.cc:make_decl_one_only` takes its
one-only branch and records a comdat group as well as making the decl weak; a per-symbol
section for every function and object, carrying `,"G",@,<group>,comdat` when there is a
group, keyed on GCC's own `DECL_COMDAT_GROUP` because several decls share one; `SET_ASM_OP`
`.set`, which is what gives `defaults.h`'s `ASM_OUTPUT_DEF` and with it
`TARGET_SUPPORTS_ALIASES`; and `ASM_OUTPUT_DEF_FROM_DECLS`, because a function alias needs
its own `.type` and a `.functype` in the module header — a `call` naming the alias is
rejected unless the signature is already in scope — plus its own binding directives, since
this target's `TARGET_ASM_GLOBALIZE_LABEL` and `TARGET_ASM_ASSEMBLE_VISIBILITY` are no-op
lambdas and nothing else writes them.

`__GXX_WEAK__` is 1 with the result, which is the one-line check that the chain above is
broken, and `tinfo.cc` no longer contains an alias to reject. Unlike `0005` this patch is
offerable upstream as it stands: it takes no side on any open question and it is what the
series needs to host a C++ runtime at all.

It carries one correction to `0005` as well, found by 9d and fixed here because
`wasm-asm.cc` is cut with this patch now: **a NaN can carry a sign.** `0005` decided from
reading that this dialect takes neither a sign nor a payload and `sorry`ed on both; the
sign is accepted and only the payload is not. `nan_sign` and `nan_payload` in the dialect
probe are the measurement, and the second is deliberately red. The cost of the wrong half
was exact: `fast_float`'s `-quiet_NaN ()` is what `src/c++17/floating_from_chars.cc` is
built from, so the target had no `std::from_chars` for a float. It is also the one place
in this series where something was concluded from reading instead of from a probe, which
is the argument for the probe scripts rather than a point about NaNs.

### 9c — the setjmp wall, cleared in the sysroot

The owner's call, taken over patching libstdc++ or waiting for wasm EH.
`build_gcc_wasm.py`'s `declare_setjmp` removes wasi-libc's exception-handling guard from
the flattened `setjmp.h`, leaving `setjmp` **declared and undefined**. That is the literal
truth for this sysroot — it contains no `setjmp.o` — so code that really calls one fails at
link naming the symbol, and nothing claims a capability the backend does not have.
`__wasm_exception_handling__` is deliberately not defined: that would get the header past
the preprocessor and buy a module that silently does not unwind.

Two notes on why this is one line of code and a paragraph of justification. The guard's
exact shape is *required*, so a wasi-libc release that words it differently fails there and
gets looked at rather than quietly keeping an `#error` the caller reads as the edit having
worked. And the edit is now part of the sysroot's cache key alongside
`WASI_SYSROOT_VERSION`, because an input that decides what was measured has to be in the
key or an existing workdir goes on measuring the unedited headers and reports that as the
result of the edit.

The reach of that one `#error` is the part worth remembering: `src/c++23/std.cc.in:26` is
`#include <bits/stdc++.h>`, so it blocked the `std` module as well as the PCH.
`--disable-libstdcxx-pch` would therefore not have been a way around it, which is why that
option is not used.

### 9d — what the build then did, and the three walls it found instead of exceptions

With one-only, aliases and `setjmp` out of the way, the libstdc++ build went from refusing
the target to **everything but two objects**. `libsupc++.a` is complete — including every
`eh_*.cc` — and `make -k` was used deliberately, because "where does it stop" is a question
one `make` answers with the *first* failure and the honest answer needed all of them:

| Wall | What it is | Whose |
|---|---|---|
| `src/c++17/floating_from_chars.cc` | `sorry`: a NaN this target cannot spell. **Fixed in 9b** — the dialect does take a sign, and `0005` had read that wrong | ours |
| `src/c++20/tzdb.cc` | `type mismatch, expected [i32 ×6] but got [i32 ×5]` on `call __atomic_compare_exchange_4` | backend codegen bug |
| `src/c++23/print.cc` | `'::dup' has not been declared` | wasi-libc |
| `std.cc` | `_Exit`, `atoll`, `llabs`, `lldiv`, `lldiv_t`, `strtoll`, `strtoull` not declared in `std` | wasi-libc |

**Exceptions never appeared.** Not as a `sorry`, not as an `_Unwind_` link error, not
anywhere in the build. That is the step's most load-bearing negative result, and it is why
the gating list's Exceptions item is now worded as *untested* rather than *blocking*: the
prediction that it would stop libstdc++ was wrong twice over — once about where it bites
(step 8) and once about whether it bites here at all.

The `tzdb` one deserves its shape recorded, because it is the only one that is ours to fix
and the lead is sharp. The call site materialises five arguments — `&flag`, `&expected`,
`7`, `5`, `5` — where `__atomic_compare_exchange_4 (mem, expected, desired, weak, success,
failure)` takes six, and the missing one is `weak`. The declaration in the module header
says six, so `record_libcall` and the emitted `USE`s disagree about a vector they both read
from `cfun->machine->call->args`. A four-line reproduction is enough to see it:
`__atomic_compare_exchange_n (&flag, &expected, 7, false, __ATOMIC_SEQ_CST,
__ATOMIC_SEQ_CST)`. Nothing was changed about it: it is a codegen bug in upstream's call
marshalling, it is a different animal from this step's subject, and the type-checking
assembler finding it is the third such bug `0005`'s move bought.

### A probe that could not have passed

`include_vector` was red through step 8 and 9 with its `blocked` reason naming `<csetjmp>`
and aliases — and it would have stayed red whatever libstdc++ did, because nothing on the
probe's command line pointed at libstdc++'s headers at all. Nothing in this build runs
`make install`, and the driver's default C++ include directory is under the *prefix*; the
headers are staged in the build tree. So the red was the include path wearing the
library's clothes, in the one place in this pipeline that must not have that.

Fixing it took naming the three directories libstdc++ compiles itself with, which also
turned up where `<compare>` lives: `libstdc++-v3/libsupc++`, not `include/std`, and it is
reached by the first `<vector>`. The tripwire earned its keep in the process — the
corrected `blocked` status was asserted from reading the build log, the probe disagreed,
and the disagreement was the include path rather than the claim.

`import_std` had the same disease one layer along, twice over. `std.cc` includes
`<strstream>`, which lives in a *fourth* directory, `include/backward`; and it reaches
`<csignal>` and `<cerrno>`, which wasi-libc guards with an `#error` and a `_Thread_local`
respectively, so `-D_WASI_EMULATED_SIGNAL` and `-D_Thread_local=__thread` -- already in
`CXXFLAGS_FOR_TARGET` for the library -- have to be said for the compiles outside it too.
With those, the module reaches the seven missing `std::` names, which is the real wall.

One wrong turn is worth recording because the plausible story was wrong: `<strstream>` is
removed in C++26, so compiling the module as `-std=c++26` *looked* like the cause and
`gnu++23` like the cure, which is how libstdc++ builds it. Measured one flag at a time,
the dialect turns out not to matter at all here — `c++26` compiles the module just as far
once `include/backward` is named. Each of the four directories and both defines was then
confirmed by a compile that failed without it and got further with it, rather than copied
from the library's command line as a block.

### The probe table

| Probe | Status | What it adds |
|---|---|---|
| `baseline`, `reflection`, `contracts` | compiled | unchanged |
| `arith`, `varargs`, `local_linkage`, `funcptr`, `static`, `extern_varargs`, `sret`, `two_tu`, `init_array`, `libcall`, `libc` | ran | unchanged from step 8 |
| `comdat_tu` | **ran** (new) | one-only decls in two TUs: an inline function and a vtable, deduplicated and called |
| `include_vector` | blocked: link failed | libstdc++.a is not archived while `tzdb` and `print` are missing — a *link* error now, where it was a compile error |
| `import_std` | blocked: std module failed | seven C library names wasi-libc does not declare |

`comdat_tu` running, and `include_vector`'s red moving from "compile failed" to "link
failed", are the two rows that say what this step bought: the compiler and the headers are
no longer what stops a hosted C++ translation unit. Two objects and seven declarations are.

## Step 10: an archived libstdc++, and the unwinder that is not there

This step set out to close the two objects and seven declarations step 9 ended on, and to
add a `throw` probe to measure the exceptions question for the first time. It closed the
two objects. The seven declarations and the `throw` both turned out to be something other
than what they were filed as, and the two findings are the step's real content.

### 10a — the two objects, and why neither diagnosis survived

`src/c++20/tzdb.cc` and `src/c++23/print.cc` both build, and `libstdc++.a` archives: 199
members, 4.9 MB. `0008-local-import-signatures.patch` is the fix, and the README there
carries the detail. What belongs here is that **both diagnoses in step 9 were wrong in the
same direction** — each named the call site where the declaration was at fault:

- The plan read `expected [i32 ×6] but got [i32 ×5]` as the call dropping `weak`, and
  named `record_libcall` on the strength of `0005` having fixed a reverse-order bug there.
  A `fprintf` in `record_libcall` never fired: it is not called for this symbol. The
  five-argument call is *correct* — `builtins.cc` drops `weak` deliberately, says so in a
  comment, and keeps the six-parameter decl — and the declaration emitted from that decl
  is the wrong half.
- `print.cc` was filed as a header gap to be unguarded like `setjmp`. wasi-libc has no
  `dup` at all, by stated design, and `print.cc` *calls* it where nothing calls `setjmp`,
  so declaring it would have archived the library and planted a link failure in every
  program that prints to a terminal. The code path is excluded instead.

A third bug had to be fixed to get here that no step had filed, because nothing could
reach it: **a function whose address is taken in data was never declared**, so llvm-mc
typed it as data and wasm-ld rejected it against the object that defines it as a function.
`eh_unex_handler.cc` is one line that stores `std::terminate` without calling it, and
every probe that links a function pointer calls it too.

### 10b — exceptions, measured: there is no unwinder

Step 9's load-bearing negative result was that exceptions "never appeared" anywhere in the
libstdc++ build, and it rewrote the gating list from *blocking* to *untested* on that
basis. The archive is what makes the test possible, and the answer is immediate:

```
libstdc++.a(eh_throw.o):     undefined symbol: _Unwind_RaiseException
libstdc++.a(eh_throw.o):     undefined symbol: _Unwind_Resume_or_Rethrow
libstdc++.a(eh_catch.o):     undefined symbol: _Unwind_DeleteException
libstdc++.a(stdexcept.o):    undefined symbol: _Unwind_Resume
libstdc++.a(cow-stdexcept.o): undefined symbol: _ITM_RU1, _ZGTtnam, _ITM_memcpyRtWn
```

`wasm.cc` sets `TARGET_EXCEPT_UNWIND_INFO` to `UI_NONE`, so libgcc builds no unwinder and
defines none of these. The step-9 reading was not wrong about what it saw — it was reading
a compile, and this is a link. **`-fno-exceptions` is not a way around it**, measured
rather than assumed: `eh_throw.cc`'s whole job is to call the unwinder, so the reference
belongs to the file and not to a flag on it, and a libstdc++ rebuilt with the flag still
carries thirteen `_Unwind_*` references. The `_ITM_*` and `_ZGTtnam` in `cow-stdexcept.o`
are libitm's, a second and much smaller gap.

So the Exceptions item goes back to **blocking**, for the third wording in three steps,
and this time off a link rather than off an absence. It is now the only thing between here
and a hosted C++ program that runs: `throw_catch` and `include_vector` both stop here and
nowhere else.

### 10c — the seven names are an ABI mismatch, not a libc gap

`import_std` still reports the same seven missing `std::` members, and the reason filed in
step 9 — that wasi-libc does not declare them — is simply not true. Its `<stdlib.h>`
declares all seven, unguarded; they were read there line by line.

What withholds them is `<cstdlib>`'s `_GLIBCXX_USE_C99_STDLIB`, left undef in the target's
`config.h` because libstdc++'s own configure link test for the seven failed. Reproduced
with configure's exact command line, that test fails on:

```
wasm-ld: error: libc.a(intscan.c.obj): undefined symbol: __multi3
wasm-ld: warning: function signature mismatch: strtold
>>> defined as (i32, i32) -> f64         in our object
>>> defined as (i32, i32, i32) -> void   in libc.a(strtod.c.obj)
```

Both are the same thing. `wasm.h` caps `MAX_FIXED_MODE_SIZE` at 64 and `wasm.cc`'s
`scalar_mode_supported_p` vetoes `TImode` outright, so GCC's wasm32 has no `__int128` and
`long double` is `double`; libgcc therefore builds no TImode or TFmode routine — the
`_fixtfdi.o`, `_multc3.o`, `_powitf2.o` members exist in `libgcc.a` and are empty.
clang's wasm32, which built this sysroot, has `__int128` and an IEEE binary128 `long
double` and calls for both. Nineteen compiler-rt symbols are missing for that reason
(`__multi3`, `__muloti4`, and the TFmode arithmetic, comparison and conversion set), and
`strtold` is an outright ABI disagreement that would be silently wrong rather than
unlinked.

This is a bigger finding than the item it replaces: every prebuilt wasm library in
existence was built by clang, so **GCC's wasm32 as upstream configures it is not
ABI-compatible with the wasm ecosystem's `long double` or `__int128`.** Nothing was
changed about it — it is a fork in the plan, not a bug with a patch.

### The probe table

| Probe | Status | What it adds |
|---|---|---|
| `baseline`, `reflection`, `contracts` | compiled | unchanged |
| the 12 `ran` rows of step 9 | ran | unchanged, re-measured after both backend fixes |
| `include_vector` | blocked: link failed | was *libstdc++.a is not archived*; it is archived now, and this is the unwinder |
| `throw_catch` | blocked: link failed | new: a throw caught in the same function, the narrowest ask for an unwinder |
| `import_std` | blocked: std module failed | same seven names, a different and larger cause |

One harness bug was uncovered on the way, latent for as long as the archive did not exist:
the hosted C++ link line named `-lsupc++` beside `-lstdc++` with one `-L`. libtool links
libsupc++ into libstdc++ as a convenience library, so all 65 of its members are already
among libstdc++.a's 199, and naming it twice produced `duplicate symbol` and a dozen
`symbol type mismatch` lines that were not about the target at all.

## Step 11: 32-bit cleanliness, engine-side — what a 64-bit host can and cannot police

Measured 2026-10-16. The owner took this ahead of both open compiler items, on the argument
this file has made for it since step 1: cheap, certain, and needed under every path. It is
also the first step here that is engine work rather than toolchain work, so what it had to
establish first was how much a *host* compile can be made to say about an ILP32 target. The
answer bounds the whole step: less than hoped, and exactly knowable.

### 11a — the gate that already existed, and the hole that cannot be closed

The engine was not starting from nothing. `archive_width_is_portable<T>()`
(`Engine/Engine/Source/Containers/Archive.cppm`) already rejects pointers, references,
member pointers, `long`, `unsigned long`, `long double` and `wchar_t`, recurses through
enums, arrays, members and unions, and is enforced by four `static_assert`s whose messages
name ILP32 wasm32 by hand. It is the only thing in the tree that says "data model", and it
exists because an archive records each field's native size in its own schema.

Its hole is `std::size_t`, which is the spelling this engine actually uses: on this LLP64
host `size_t` *is* `unsigned long long`, so by type identity a `size_t` field is
indistinguishable from a deliberate `std::uint64_t`. The plan's part 1 was to recover the
alias spelling through reflection. **It cannot be done, and the measurement is two
`static_assert`s rather than an argument:**

| Subject | Result |
|---|---|
| `std::meta::is_type_alias(^^std::size_t)` | **true** — an alias reflected directly is visible, and `identifier_of` gives "size_t" |
| `std::meta::is_type_alias(std::meta::type_of(^^size_t_field::count))` | **false** — `type_of` on a member declared `std::size_t` yields the dealiased type |

The spelling survives `^^` and does not survive `type_of`, which is the one place a gate
over a struct's fields can ask. Note what the first row also rules out: a
`template <typename T>` predicate could never have seen the alias either, because a
template argument is a type and not a spelling — the check had to be at the member level,
which is exactly where the dealiasing happens. Both halves had to be measured to know that.

A `gse::meta::is_data_model_alias` predicate and its call in the member walk were written,
built and then **reverted**: with `type_of` dealiasing they are a branch that cannot fire,
and a gate that looks stronger than it is costs more than the hole it fails to close. What
landed instead is the second row above, carrying the measurement as its assertion message:
it is the tripwire, host-independently, because the day `type_of` preserves the alias is the
day it fails and names the fix.

A third assertion was written beside it and **removed in review**, and the reason is a fact
this section had wrong: `archive_width_is_portable<size_t_field>()` is **true on this host
only**. `size_t` is `unsigned long long` under Windows LLP64, which the gate accepts, but it
is `unsigned long` under SysV LP64 and on wasm32 — and the gate rejects `unsigned long` by
name. So asserting the hole by *width* would have broken the build on the first wasm32
engine compile, which is the single event every open item below is deferred to, with a
message blaming alias preservation for what is really the target's spelling of `size_t`.
The hole is therefore not even uniform: a `size_t` field the host waves through is one
wasm32 would reject on sight, for a reason nothing in the gate intends.

One incidental fact worth not re-deriving: `^^std::uint64_t` does not compile at all —
"`^^` cannot be applied to a using-declaration" — because libstdc++ brings the C names into
`std` with using-declarations rather than typedefs. `^^std::size_t` and `^^std::ptrdiff_t`
are fine.

### 11b — the byte boundaries outside the archive, read, and clean

The worry was a second serialiser with no gate on it. There is none. Every raw-byte
boundary in the engine was read:

- `Sdk/Pack.cppm` is `std::uint32_t` and `std::uint64_t` throughout with explicit casts at
  the edges, which is what the archive's gate asks for and what no gate enforces for it.
- `Graphics/Capture/Mp4Muxer.cpp`, `Graphics/3D/Animations/SourceReader.cppm` and
  `Audio/Audio.cpp` move *byte vectors*, not structs. The MP4 boxes are built field by field
  to a spec-defined big-endian layout, which is the right way round: those layouts are fixed
  by their format, and making them "portable" would break them.
- `Containers/LinearVector.cppm`'s `size_type = std::size_t` is memory, not a format, and is
  correct as it stands under both data models.
- `Assets/AssetSystem.cppm`'s stamp check reads a `std::uint32_t header[3]`.

The one deliberate exclusion: the attached-editor messages in `Runtime/Engine.cppm` carry
`void*` surface and semaphore handles and are `memcpy`'d over a pipe in
`Runtime/AttachedLink.cpp`. Both ends are the same binary on the same machine, and under
wasm there is no pipe, no Win32 and no attached editor at all, so there is nothing for a
data-model rule to protect. They are named here so the next reader knows they were seen
rather than missed.

### 11c — the narrowing class, five sites, all one shape

The hazard that is real today is a 64-bit file size narrowed to `size_t` for a whole-file
allocation. On ILP32 it truncates, and the file is then read into a buffer of the wrong
length with nothing reporting it. Five sites, all fixed, each reporting through the channel
it already had rather than through a new one or an assert:

| Site | Channel |
|---|---|
| `Sdk/Pack.cppm` `read_bytes` | `std::unexpected(std::string)` |
| `Sdk/Pack.cppm` `extract_entry` — `entry.compressed` and `entry.size`, both read from the pack | `std::unexpected(std::string)` |
| `Audio/Audio.cpp` `bake` | warning log, `false` |
| `Graphics/3D/Animations/SourceReader.cppm` `open` | `std::unexpected(source_error::unreadable)` |
| `Save/SaveSystem.cppm` `read_file` | `std::unexpected(std::errc::file_too_large)` |

`std::in_range<std::size_t>` is the test at all five, which is also why none of them needed
a second branch: it rejects a negative `tellg()` in the same expression, and three of these
allocated `static_cast<std::size_t>(-1)` on a failed `tellg` before this step. That is a bug
on *this* host, found by looking for an ILP32 one.

### What this step did not reach

It reduced the hazard; it did not verify its absence, and the reason is structural rather
than a matter of effort. **No engine translation unit compiles for wasm32 today** — every
one does `import std`, and the `std` module is blocked on the ABI mismatch of step 10c —
and the host toolchain is `--disable-multilib`, so there is no `-m32` either. Deferred by
name to the first wasm32 engine compile, each for that same reason:

- **`size_t` arithmetic overflow.** A `count * stride` that cannot overflow in 64 bits can
  in 32. Nothing available here can see it.
- **Everything `-Wconversion` would say.** The engine does not build with it under either
  model, and turning it on for a 64-bit build reports a different set of truncations than
  the ones that matter.
- **The address space itself.** wasm32 caps at 4 GB and in practice far less; that is a
  budget question rather than a type question.
- **`-mavx2`** (`Engine/cmake/GSEEngine.cmake:36`), which is its own item in "Engine-side
  consequences" and is not a data-model question at all.

The lever that would turn this audit into something a compile checks is compiler-side, and
was deliberately not pulled here: the seven `std::` names of step 10c are withheld only
because libstdc++'s configure *link* test failed, so forcing `_GLIBCXX_USE_C99_STDLIB` into
the target's `config.h` would plausibly build the `std` module for wasm32 for syntax-only
purposes. It would leave `strtold` ABI-wrong, so it is a compile-only hack, and it belongs
with the ABI item rather than with this one.

## Step 12: the unwinder, the weak references, and the one symbol behind both

Measured 2026-10-16. The step set out to close the Exceptions item, which step 10 had
left as the only thing between here and a hosted C++ program that runs. Both halves of
that item are closed. The program runs — and not through the probe table, because a
*third* gap was behind the two, and it is the ABI mismatch step 10c had already found
somewhere else.

### 12a — a terminating unwinder, and the set is four

`libgcc/config/wasm/unwind-wasm.c` defines `_Unwind_RaiseException`, `_Unwind_Resume`,
`_Unwind_Resume_or_Rethrow` and `_Unwind_DeleteException`, each `abort ()`, built by one
line of `libgcc/config/wasm/t-wasm`. `0009-local-terminating-unwinder.patch`.

**Four is measured, not read off `unwind.h`.** This step's cheapest finding was that the
measurement needed no build at all: `libstdc++.a` and `libgcc.a` were already on disk
from step 10, so one `wasm-ld -error-limit=0` names the complete set. Step 10's recorded
list had been truncated by wasm-ld's default error limit, which is the sort of thing that
turns into a guess two steps later.

`_Unwind_ForcedUnwind` and `_Unwind_Backtrace` were written first, on the expectation that
a personality routine would want them, and removed again on that measurement: nothing
pulls `eh_personality.o` in, because with `UI_NONE` no function names a personality
routine, so the `_Unwind_Get*` accessors are not referenced either. Six symbols was the
estimate this file has carried since step 10; four is the number.

### 12b — the weak-reference gap, and where to repair it

`cow-stdexcept.o`'s `_ITM_RU1`, `_ITM_memcpyRtWn`, `_ITM_memcpyRnWt` and `_ZGTtnam` were
never a libitm gap. They are **weak references** — `__attribute__((weak))` on an extern
that nothing defines — and the target emitted nothing for one, so they came out as
ordinary undefined symbols. `c++config:755` derives `_GLIBCXX_USE_WEAK_REF` from
`__GXX_WEAK__`, which `0007` set to 1, so libstdc++ was believing a promise the target
had made and had nothing behind.

Two repairs were available and the choice was measured rather than argued:
`weak_undef` in `Tools/wasm_dialect_probe.py` assembles a `.weak` symbol nothing defines
and links it, and it is **green** — llvm-mc marks it `undefined binding=weak`, wasm-ld
leaves it unresolved. So the capability is real and the target was simply not emitting it,
which makes the backend the place to fix it: `0010-local-weak-references.patch`, one `if`
in `assemble_import`. The alternative was to withdraw the promise with a
`config/cpu/wasm/cpu_defines.h` setting `_GLIBCXX_USE_WEAK_REF 0`, the way nvptx and gcn
do, which would have left `__attribute__((weak))` quietly broken for everyone else's code.

### 12c — the hosted program runs, behind one symbol

With both patches in, `include_vector` and `throw_catch` fail at the link on exactly one
symbol: `__multi3`, wanted by wasi-libc's `intscan.c.obj`, which libstdc++'s locale setup
reaches. That is **step 10c's ABI mismatch**, and the finding is that it is no longer
confined to `import_std`: `wasm.h` caps `MAX_FIXED_MODE_SIZE` at 64 and
`scalar_mode_supported_p` vetoes `TImode`, so libgcc's `_multi3.o` member is empty, while
the clang-built sysroot calls for it. (**Step 13 corrects the second half of that**: there
is no `_multi3.o`, empty or otherwise. libgcc derives its word size from
`MIN_UNITS_PER_WORD`, which `wasm.h` sets to 4, so the member that was built is
`_muldi3.o`.)

A link error says nothing about what is behind it, so what is behind it was measured:
with a scratch `__multi3` object on the link line, off the pipeline and not in any patch,

| Probe | Result |
|---|---|
| `include_vector` | **ran, 385** — a hosted C++ program works, and `__multi3` is never actually called |
| `throw_catch` | **trapped**, in the `abort ()` inside `_Unwind_RaiseException` |

The second was confirmed to be that `abort` and not the stub by making the stub return
instead of trapping. So the step's claim stands on a measurement rather than on the probe
table: the unwinder works as designed, a throw terminates, and what remains is arithmetic
rather than exceptions.

`__multi3` is not an ABI *disagreement* the way `strtold` is — its signature is
unambiguous and in the object — so supplying it is separable from the fork. It was left
alone anyway, because it belongs to the nineteen compiler-rt symbols of 10c and taking
one of them in this step would blur what the probe table measures.

### 12d — real exceptions are reachable, and it is GCC-side work

`eh_instructions` in the dialect probe asks whether the WebAssembly exception-handling
proposal is expressible with these tools at all, before any step estimates it. It is
**green**: a tag defined rather than imported, a `throw`, and a `try_table` whose catch
clause branches out with the payload — assembled, linked, and caught at runtime under
node, returning 42.

Two facts worth not re-deriving. `-mattr=+exception-handling` is **required**, and its
absence does not present as a rejected instruction: llvm-mc parses `try_table` and then
type-checks it as though the catch clause were not there (`expected [i32] but got
[i32, i32]`), so a backend emitting these has to name the flag in `ASM_SPEC` the way
`0005` names the triple. And **a tag is defined by a plain label under its `.tagtype`** —
`.globl` and `.tagtype` alone leave the symbol undefined and wasm-ld refuses it, which is
what the stock spelling of `__cpp_exception` does too.

So the remaining work for real exceptions is entirely GCC's: `TARGET_EXCEPT_UNWIND_INFO`
is `UI_NONE`, which suppresses EH region generation, so nothing downstream has ever been
asked for a landing pad. That is a step rather than a patch, and this row says it is not a
dead end. The sjlj path remains *not* an alternative: it needs a `setjmp` a wasi-libc
built without the proposal does not have.

### The probe table

| Probe | Status | What changed |
|---|---|---|
| the 15 compiled/ran rows of step 10 | unchanged | re-measured against a libstdc++ rebuilt by the new compiler |
| `include_vector` | blocked: link failed | same status, third cause: the unwinder and the `_ITM_*` are closed, `__multi3` is what is left. Runs, at 385, with that symbol stubbed |
| `throw_catch` | blocked: link failed | same, and measured past it: it traps in the unwinder, which is this row's eventual status |
| `import_std` | blocked: std module failed | unchanged (the same ABI mismatch) |
| `weak_undef`, `eh_instructions` | proven | new, both in the dialect probe |

libstdc++ was rebuilt from scratch rather than incrementally, because `0010` changes what
the compiler emits and no libstdc++ object depends on the compiler's timestamp — a partial
rebuild would have produced a library half of whose objects measured the previous
compiler.

### A harness bug this step uncovered: `import_std` was reading a recovery artifact

Re-running the script turned `import_std` from `std module failed` — with the seven
missing `std::` names on it — into a bare `compile failed` about a `.gcm` that was never
written. Neither status was about the target.

`src/c++23/Makefile`'s `std.lo` recipe answers a failed module compile by **emptying the
source**: `echo "Cannot compile std module" && echo > std.cc.tmp && mv std.cc.tmp std.cc`,
so that the library still builds, without a module initialiser. The probe was reading that
generated `std.cc` through `include/bits`, and whether it saw the real one or the
one-byte one depended on whether `stamp-modules-bits` had copied the file before or after
the emptying — which differs between a fresh build and a re-run of `make`.

The probe now concatenates `std.cc.in` and `std-clib.cc.in` from the source tree itself,
which is exactly what that Makefile's own `std.cc` rule does and which no failure path
rewrites. Worth recording as the second time in this series that a red turned out to be
about the measurement: the first was step 9's `include_vector`, red for two steps on a
missing `-I`.

## Step 13: `__multi3`, a miscompile behind it, and `import std` runs

Measured 2026-10-16. The step set out to close step 12's one remaining link symbol. It
closed it, and on the way it found a **silent miscompile of every 64-bit constant too
large for 32 bits** that nothing in twelve steps had been positioned to see. With both
fixed, `include_vector` and **`import_std` both run**: `import std;` compiles, links and
returns the right answer on wasm32, which is the row this plan has called "the one that
says whether the engine-side port can start" since step 3.

The gating list's compiler section now holds one item, `long double`, and it is a fork
rather than a bug.

### 13a — the `__int128` route, attempted and measured as closed

The scope was to un-veto `TImode` so that libgcc builds its DWtype routines, `__multi3`
among them. Three lines reach it, and the second is the one worth keeping:

- `MAX_FIXED_MODE_SIZE` 128 rather than 64;
- `MIN_UNITS_PER_WORD 4` withheld from libgcc under `#ifndef IN_LIBGCC2`, because
  **libgcc derives its word size from `MIN_UNITS_PER_WORD`, not `UNITS_PER_WORD`**
  (`libgcc2.c:39-46` to `libgcc2.h:219`). `wasm.h` says `UNITS_PER_WORD 8` and
  `MIN_UNITS_PER_WORD 4`, so libgcc built at word 4 and `__NDW(mul,3)` spelled
  `__muldi3`. **This file's claim that `_multi3.o` "exists and is empty" was wrong** —
  it was never built. `_muldi3.o` is on disk and `_multi3.o` is not. The idiom is
  `i386.h:766`, `mips.h:1651`, `riscv.h:170`, `loongarch.h:132`;
- the `m != TImode` veto dropped from `TARGET_SCALAR_MODE_SUPPORTED_P`.

The compiler built. **libgcc did not.** Seven objects ICE — `_negdi2`, `_lshrdi3`,
`_ashldi3`, `_muldi3`, `_ashrdi3`, `_cmpdi2`, `_ucmpdi2` — in `emit_move_multi_word`
(`expr.cc:4545`, the `gcc_assert (xpart && ypart)`, i.e. `operand_subword` returned 0)
and in `extract_insn` on

```
(insn 2 (set (subreg:DI (reg:TI 27) 0) (reg:DI 24 [ a ])) "libgcc2.c":2050 -1)
```

**There is no move pattern for a word of a TImode value**, because
`constraints.md:68,80`'s `wasm_register_operand` and `wasm_subregister_operand` and
`wasm-cg.cc:410`'s `hard_regno_mode_ok` all answer false for `TImode`. **Step 13's scope
read those exclusions as "split it into halves" and they mean "reject it"** — consistent
with a `scalar_mode_supported_p` that was never meant to see one. `wasm-cg.cc:537,572`'s
TImode arms, which pass one as 2xi64 in LE order and say so in a comment, are an
unfinished start and not evidence that anything downstream works. Teaching the move
patterns and predicates about `subreg:DI` of `TImode` is real back-end work; it was not
started, and the three lines were reverted. A compiler that ICEs on ordinary `__int128`
code is worse than one that honestly has none.

### 13b — `__multi3` by hand, to a signature read off the object that calls it

`0011-local-multi3.patch`: a new `libgcc/config/wasm/multi3.c` and one `LIB2ADD` line in
`libgcc/config/wasm/t-wasm`. Same shape as `0009`.

**The ABI was measured, not assumed.** `llvm-ar x libc.a intscan.c.obj` and
`wasm-objdump -x` give it directly:

```
 - type[3] (i32, i64, i64, i64, i64) -> nil
 - func[2] sig=3 <env.__multi3>
```

A result pointer first, then each 128-bit argument as two i64s, low word first. Declared
in those terms — `void __multi3 (unsigned long long *, unsigned long long, ...)` — GCC
emits exactly that signature, with no `__int128` anywhere. `wasm-objdump` on the probe's
own object confirms the match, and `wasm-ld` would have said `function signature
mismatch` if it did not.

The implementation is the four-partial-product 64x64 to 128 multiply, and it was
**differential-tested against a native `unsigned __int128` over three million random
pairs plus the power-of-two and all-ones edges** before it was committed. That mattered
twice over: it is how the carry out of the two middle products got written correctly, and
it is why the miscompile in 13c was attributed to the toolchain rather than to the
routine.

### 13c — the miscompile, which is the step's real finding

`0012-local-host-wide-int-operands.patch`. One `fprintf` format string in
`wasm_print_operand`'s `CONST_INT` case:

```c
fprintf (stream, "%ld", INTVAL (value));   /* was */
```

`INTVAL` is a `HOST_WIDE_INT` and always 64-bit; `long` is 32-bit on an LLP64 host. So
`fprintf` read half the argument and printed it as a signed 32-bit number, and a DImode
`0xffffffff` was emitted as

```
i64.const	-1
```

**`x & 0xffffffff` therefore assembled as `x & ~0`.** The mask was in the source, the
`i64.and` was in the output, and the constant was wrong — a miscompile rather than a
rejected instruction, and invisible on a Linux host, which is where this series is
developed. `assemble_integer` immediately above already uses
`HOST_WIDE_INT_PRINT_UNSIGNED` for this exact reason; the operand path had been missed.
Same family as `0002`'s two bugs, and offerable with them.

Three things about how it surfaced are worth not re-deriving:

- **Only a computing probe could find it.** `__multi3` is built out of
  `a & 0xffffffff` and `a >> 32`, so all four of its partial products silently became
  full 64-bit products. A probe that only checked the link would have been green, and
  the symbol would have been quietly wrong in every program that parsed an integer.
- **The first debug probe was blind, and the blindness is instructive.**
  `__multi3 (p, 3, 0, 5, 0)` returns 15 under *any* permutation of its four arguments,
  so it cleared the routine and the ABI at once and proved neither. Four distinguishable
  operands (`1, 2, 3, 4`, giving `3010`) are what separate "the ABI is right" from "the
  operands happen to be symmetric".
- **The wrong answer named its own cause.** The module's result for
  `(a & 0xffffffff) * (b & 0xffffffff)` was bit-for-bit the low word of the *unmasked*
  `a * b` — which says the mask did nothing, rather than that the multiply was wrong, and
  that is a two-line read of the emitted `.s` rather than a bisect.

Because `0012` changes what the compiler emits, **libgcc and libstdc++ were both deleted
and rebuilt from scratch**: no object in either depends on the compiler's timestamp, so
`make` cannot notice, and a partial rebuild would have produced a library half of whose
objects measured the previous compiler.

### 13d — `import std` runs, and why

`import_std` was the plan's last red and the one it had been pointing at since step 3. It
is green, and the chain is exactly the one step 10c diagnosed, run backwards:
`__multi3` was an undefined symbol in libstdc++'s own configure link test for the seven
C99 `std::` names, so the test failed, so `_GLIBCXX_USE_C99_STDLIB` was left undef, so
`<cstdlib>` withheld `_Exit`, `atoll`, `llabs`, `lldiv`, `lldiv_t`, `strtoll` and
`strtoull`, so `std.cc` would not compile. With the symbol defined the test links;
`config.h` now carries `_GLIBCXX11_USE_C99_STDLIB 1` and `_GLIBCXX98_USE_C99_STDLIB 1`.

**`strtold` is still a disagreement and still does not block.** The warning is in this
build's `config.log` three times —

```
wasm-ld: warning: function signature mismatch: strtold
```

— and it is a *warning*, so the test succeeds in spite of it. That answers the question
the scope flagged as unknown: the hard error was `__multi3` alone. What remains is the
quiet half, which is the point about `long double` all along: nothing fails, and a
program that calls `strtold` gets an `f64` where libc returns a binary128.

### The probe table

| Probe | Status | What changed |
|---|---|---|
| the 15 compiled/ran rows of step 12 | unchanged | re-measured against a libgcc and libstdc++ rebuilt from scratch by the new compiler |
| `multi3` | **ran** | new. Checks both words of a 128-bit product through the measured ABI; it is what found `0012` |
| `include_vector` | **ran, 385** | was *blocked: link failed* on `__multi3`. `blocked` entry dropped |
| `import_std` | **ran, 123** | was *blocked: std module failed*. `blocked` entry dropped — `import std;` works on wasm32 |
| `throw_catch` | blocked: **trapped** | was *blocked: link failed*; now reaches its designed end state, the `abort ()` in `_Unwind_RaiseException`. Its `blocked` entry says `trapped` rather than excusing a link failure |

## Steps 14-17: reconstructed, and the originals are lost

**These four sections are a reconstruction, not the original prose.** On 2026-10-17,
during step 19, an edit of mine meant to replace step 18's trailing gating list sliced
this file from the *first* occurrence of the heading it searched for rather than the last,
and deleted roughly 709 lines: the gating list that followed step 13's probe table, and
the whole of steps 14, 15, 16 and 17.

Step 18 below was recovered byte-for-byte, because this session had read it in full and
the transcript kept it. Steps 14-17 had never been read back by any session, and this file
is **untracked in git**, so no commit, stash, worktree or phase diff contains them -- the
phase `diff.patch` artifacts are produced by `git diff` and silently omit it. The
reconstruction below is written from `project-wasm-pipeline` in the project memory, which
was itself written off these sections, so the *findings* survive with their reasoning; the
prose, the citations' exact line numbers and anything the memory did not consider worth
keeping do not.

**Two things to do about it.** Commit this file, so the next mistake costs a `git
checkout` instead of a reconstruction. And when a step's findings are needed in detail,
prefer the step's own entry in `Tools/gcc-toolchain/wasm/README.md`, which is tracked.

## Step 14: the engine's own code compiles, and `:simd` is the root

Reconstructed -- see the note above.

- **`:simd` is the ROOT of gse.math**, not its last item: `Vector.cppm` imports it. The
  closure is three modules -- gse.meta (15 partitions plus its interface), gse.assert's
  *interface*, then gse.math's 19 plus its interface. 36 translation units.
- **`SIMD.cppm`'s portable arm was nearly free.** 15 of its 16 entry points already ended
  in a scalar `else`, so the arm is an `#ifdef GSE_SIMD_X86` around the intrinsic chain.
  Only `mul_mat4` had no fallback and needed a body written.
- **`Import/Assert.cpp` is Win32-bound** -- gse.win32, gse.log, gse.stacktrace and
  `std::thread` -- so anything linking gse.math needs a stub for `assert_fail`,
  `fatal_exit` and `install_fatal_reporter`, and, since step 16, `handle_contract_violation`.
- **One error, fourteen cascades.** A probe stops at the first error, so counting what
  compiles needs a sweep that continues past failures. Do not read a red probe as a count.
  This is what step 18's sweep was built out of.
- **Direct `./xg++` from Git Bash exits 1 with no diagnostics** -- it needs the MSYS2 shell
  for cc1plus's DLLs. Go through `msys2.bash`, or a broken invocation reads as a result.

## Step 15: the snapshot bump

Reconstructed -- see the note above.

- **The pin moved to `20260809`, the snapshot the engine's own native toolchain is built
  from, and that is the standing rule** rather than a one-off: the wasm cross and the
  native toolchain move together, because a red engine row is only readable when the
  frontend under it is the one the engine builds with. Explicitly *not* the newest
  snapshot -- newest makes every red row ambiguous between a target gap and a trunk
  regression the engine has never seen.
- **It cost no patch work at all.** All ten patches applied to `20260809` with no rejects
  and no fuzz, and `all-gcc`, libgcc and libstdc++ all built with no source change. The
  reason is structural and worth keeping: **`0005` onwards edit only files `0001`
  creates**, so they cannot reject on a bump. The drift-exposed surface is 16 hunks in 14
  files, and the property that matters is not the count -- all of them are additions with
  full context.
- **All 36 `gse.math` units compile for wasm32**, `Hash.cppm` included, and with it
  `engine_meta` and `engine_math`. Step 14's blocker was exactly what it was read as:
  eleven weeks of frontend drift.
- **`build_gcc_wasm.py` had a stale-build-dir hazard and it fired on this run.** There is
  one `build/` and a tree per snapshot, and the `.configure-args` stamp did not name the
  source tree -- so a bump would have kept Makefiles pointing at the old tree and gone on
  building it. The tree is in the stamp now.
- **A COMPILE-mode probe passes as `compiled`, never as `ran`.** The table's pass test was
  `status == "ran"`, so the two engine rows that *started passing* were reported as
  failures. Fixed to the mode's own last stage. Same family as step 12's harness bug: the
  status model, not the target, produced the wrong row.
- `engine_math_leaves` and `MATH_LEAVES` were deleted, subsumed by a green `engine_math`.
  `engine_meta_parts` stays: it is the reflection row, not an artifact of the blocker.

## Step 16: the module initializer, and the bug was the front end's

Reconstructed -- see the note above.

- **`0013-local-module-initializer-decl.patch` is one line in `gcc/cp/module.cc`**, and the
  only one of ours in the C++ front end -- *not* the first outside `gcc/config/wasm/`,
  which `0006` and `0008` already were.
- `module_add_import_initializers` synthesizes the decl for each imported module's global
  initializer with `TREE_PUBLIC` but **never `DECL_EXTERNAL`**. `assemble_external` returns
  early for a non-external decl, and that is the only route to `ASM_OUTPUT_EXTERNAL`. So
  the backend was asking correctly and the decl did not qualify -- **step 15's
  "backend-side, `0008`-shaped" attribution was wrong**, and that correction is the step's
  main finding.
- **Latent on every target and offerable upstream with no wasm in the reproducer**: only a
  handful of targets define `ASM_OUTPUT_EXTERNAL`, and none needs a declaration ahead of a
  *call*.
- **The backend repair was rejected on purpose.** Declaring on reference in
  `wasm_print_operand` -- the `0008` shape -- would route *every* call through the import
  map, including comdat functions defined in the same unit, and `DECL_WEAK` holds for
  those, so `0010`'s `.weak` would be emitted for each.
- **A new patch costs no rebuild.** The build directory's cache key is configure arguments
  plus the source tree's name, not patch bytes, so adding `0013` applied at zero offset,
  kept all ten earlier stamps valid, and cost an incremental `cc1plus` relink. libgcc and
  libstdc++ were untouched and correctly so -- neither consumes a module.
- **`module_init` is the probe**: two freestanding units, `export module mi;` with a
  *dynamic* initializer plus a consumer that imports it. The dynamic initializer is
  load-bearing twice -- it makes the module's initializer non-empty so the consumer emits
  the call at all, and an unfoldable value separates "emitted" from "ran". Measured red
  before and green after.

## Step 17: errno stops being thread-local, and the engine's math runs

Reconstructed -- see the note above. Note that step 19 **reverses this step's sysroot
edit**; read 19c with it.

- **`engine_math_run` returns 361**: a `gse::length` unit conversion, a `vec3<length>`
  magnitude, and the only path through `simd::mul_mat4`'s portable arm. 26 probe rows, 25
  green, `throw_catch` still honestly `trapped`.
- **`__emutls_v.errno` was a lie in a header, not a missing symbol.** `__errno.h` says
  `extern _Thread_local int errno`; `llvm-nm --defined-only libc.a` says `00000000 D
  errno` -- ordinary data, because a non-threads clang-built wasi-libc has no TLS in it.
  `-D_Thread_local=__thread` turned that mismatch into GCC asking the *definition* side for
  an emutls control object nobody defines. `plain_errno` rewrote the declaration in the
  flattened sysroot. **Nothing was disabled**: libstdc++'s configure still reports TLS as
  coming from emutls, and that is fine -- the blocker was emutls applied to another
  compiler's variable.
- **The header rather than the `-D`, deliberately**: the define is C++-only, so a libgcc C
  file touching errno would have had the same bug, and it lives in `CXXFLAGS_FOR_TARGET`,
  **which is the build directory's cache key** -- changing it discards the directory and
  rebuilds `all-gcc` to say what one header line says free.
- **Rejected: supplying the control object from libgcc.** `__emutls_get_address` hands back
  a freshly allocated copy, so C++'s `errno` would stop being the integer libc writes and
  every failing libc call would report success. `errno_shared` is the probe against exactly
  that -- it clears errno, lets libc's own `strtol` set `ERANGE`, and checks that C++ sees
  it. A link-only probe would have been green against the bad fix.
- **`0014` is a correction to `0009`, and the finding is methodological.** `0009` wrote the
  `_Unwind_Get*` accessors, then removed them arguing that `UI_NONE` means nothing names a
  personality routine. A libstdc++ *locale* member references `__gxx_personality_v0` ->
  `eh_personality.o` -> all three. **The link is the authority on what libgcc must define,
  not an argument from a target hook.** `_Unwind_ForcedUnwind` and `_Unwind_Backtrace` stay
  out because nothing has named them.
- **Cost: no `all-gcc` rebuild.** A sysroot header change and a new libgcc source file need
  libgcc and libstdc++ deleted and remade -- step 13's rule, since `make` notices neither
  -- and nothing in the compiler.

## Step 18: the module graph stops being hand-written, and the engine is swept

Step 17 ended by saying the compiler was off the critical path and that the measurement
that mattered next was how much of the engine follows `gse.math` through. This step
answers it, and the answer is not a percentage: **121 of the engine's 425 interface units
compile for wasm32, eight are rejected, and 292 were never offered to the compiler at all
because something they import is a wrapper over a platform library.** That last number is
the finding. It is also not a measure of the engine's portability -- see 18d.

### 18a -- the graph is derived, and the compiler is what derives it

`build_gcc_wasm.py` carried four hand-written source lists, the longest of them nineteen
entries, under a comment arguing that the order "is a fact about the engine rather than
about this script, so it is written out here rather than derived". True when it was
written, and the reason the lists are gone: a sweep over 425 units cannot have one behind
it.

`Tools/module_graph.py` derives it from two sources, **joined on the logical module name
because each is authoritative about one half**:

- **The requires-edges come from CMake's own dependency scan** -- the `.ddi` files under
  `out/build/<config>`, which are the compiler's P1689 answer rather than a guess at one.
  This is load-bearing, not tidiness: an `import` line *can* sit inside a `#ifdef` and the
  Vulkan/DX12 selection is exactly that shape, so a line-based scan would report a module
  as reaching both backends when the build only ever takes one. `analyze_module_graph.py`
  already reads these files for the build-time critical path, which is where the format
  was proven readable.
- **The paths are read off disk**, because `.ddi` names logical modules and object files
  and never a source path. A module declaration cannot be conditional, so the one regex
  in the tool is exact — but it is *not* reliably the first line: it sits after the
  global module fragment, which reaches line 34 in `Source/External/DirectX.cppm`. The
  scan therefore reads whole files rather than a head, and reports any `.cppm` carrying
  no declaration instead of dropping it, because a dropped node reappears as an
  unresolved import or a host-bound verdict with its cause erased.

The join is also the staleness check, in both directions, and `--sweep` refuses to run
when it fails: a unit the scan has never seen has no edges, would sweep as a leaf, and
would pass for the wrong reason.

**The division of labour is worth stating because it looks like duplicated work and is
not.** The compiler supplies the edges; nothing in its output supplies a topological
order (`.ddi` is an unordered edge list, and CMake's sort is not available to an
out-of-tree cross compiler), a source path, or a driver. Those three are what the Python
is.

**`.cppm` against `.cpp`, not `export module` against `module`.** Four unexported
partitions -- `gse.runtime:attached_link` and its siblings -- are implementation
partitions in `.cppm` files, and they still produce a BMI that `gse.runtime:bootstrap`
imports. A graph that took `export` as the line reported three of bootstrap's edges as
unresolvable. What this measurement leaves out is the `.cpp` units, where the host lives.

### 18b -- the lists were the oracle for exactly one run

Before deleting them, the derived closures were compared against the recorded ones:
`gse.meta` 16 units, `gse.assert` 1, `gse.math` 36, **equal as sets in all three**, with
the derived order topologically valid against every edge in the graph. Then they were
deleted, and the probe table re-run on the derived closures alone: **25 green rows and
`throw_catch` honestly `trapped`, unchanged, `engine_math_run` still returning 361.**

One row moved on purpose. `engine_meta_parts` was the fourteen partitions without
`Hash.cppm`; the split existed because Hash was the one file the previous snapshot
rejected, and step 15 measured that gap closed. Derived, the row is all fifteen
partitions, and it still asks what it always asked -- P2996 over annotations, fields,
enums and dispatch with no primary interface in front of it.

### 18c -- a harness bug the first sweep found, which is this step's methodological finding

The first sweep reported 27 rejected units. Ten of them were **the harness's fault**:
`Engine/cmake/GSEEngine.cmake` compiles the engine with `-Wno-changes-meaning
-Wno-error=changes-meaning`, `ENGINE_CXXFLAGS` here did not, and `-Wchanges-meaning` is
an error by default. The engine's own idiom is what fires it -- a member named for the
type it holds, `contact_point::separation`, `camera::request::target`,
`buffer::handle()` -- so gse.graphics, gse.physics, gse.gpu_backend and gse.runtime all
read as target failures while compiling natively.

This is the same family as the include-path hazard the probe table's comments already
warn about, pointing at flags instead of headers: **a red that looks like the target and
is really something the engine passes and the harness did not.** Any flag the engine
relies on belongs in `ENGINE_CXXFLAGS`, or the sweep measures a compiler the engine never
uses.

**Three more bugs were in the classifier rather than the flags, and all three were the
same mistake: a verdict that names one cause where there are several.** A unit behind a
*rejected* unit was recorded `failed` rather than `blocked`, putting seven files that
were never offered to the compiler into a list headed "units the compiler rejected". A
unit importing both a host-bound module and a rejected one took whichever came first in
`requires` order, so a file nobody can compile could be reported as merely blocked on a
fixable one. And the frontier attributed each module to the nearest root rather than all
of them, which is what produced 18d's first, wrong table. The first was caught by reading
the output; the other two by review. **The lesson generalises past this tool**: when a
measurement collapses a set of causes to one, the number it prints will be confidently
wrong rather than visibly missing.

### 18d -- the table, and why "43 host-bound" is not a portability figure

| | units | modules |
|---|---|---|
| compiled for wasm32 | 121 | 16 translate whole |
| rejected by the compiler | 8 | 6 modules carry one |
| blocked behind a rejected unit | 4 | |
| never offered: host-bound | 292 | 43 |

**The 292 are arithmetic the target forces, not a verdict on the code.** A dependent
cannot be compiled while a module it imports has no BMI, so one wrapper under
`Source/External` stops everything behind it from being measured at all.

**No single wrapper is the sole thing in front of anything.** This is the step's most
useful number and the draft of this section got it backwards, by attributing each gated
module to the *nearest* root a breadth-first walk found. Attributed properly -- every
root a module's imports reach, not the first one -- the per-root column reads:

| host root | sole root for | reached by |
|---|---|---|
| `gse.win32` | 0 | 33 |
| `gse.moodycamel` | 0 | 30 |
| `gse.win32.environment` | 0 | 19 |
| `gse.aftermath`, `gse.directx` | 0 | 14 each |
| `gse.nsight_perf` | 0 | 13 |
| `gse.slang` | 0 | 7 |
| `gse.freetype`, `gse.msdfgen` | 0 | 5 each |
| `gse.cxxabi`, `gse.libjpeg`, `gse.libpng`, `gse.miniaudio`, `gse.sockets`, `gse.winhttp` | 0 | 0 |

Every `sole` is zero, so **"port this one and N modules become measurable" has the answer
zero for all fifteen of them**. The sets are what move, and they are:

| modules | behind |
|---|---|
| 13 | `gse.moodycamel` + `gse.win32` |
| 6 | those two, plus `gse.aftermath`, `gse.directx`, `gse.nsight_perf`, `gse.win32.environment` |
| 5 | those six, plus `gse.freetype`, `gse.msdfgen`, `gse.slang` |
| 3 | `gse.win32` + `gse.win32.environment` |

**The cheapest real move is the first row**, and it is a good one: moodycamel and win32
together unblock gse.time, gse.concurrency, gse.diag, gse.ecs, gse.fs, gse.gpu_backend,
gse.http, gse.save, gse.scenario, gse.sdk, gse.system_manifest and the two test modules
over them -- the engine's spine.

**The "diagnostics gate the GPU layer" reading is dead.** `gse.gpu` reaches gse.win32
through `Gpu/Context.cppm`'s gse.os, gse.vulkan and gse.dx12 through its two device
backends, and gse.nsight_perf through `Graph/RenderGraph.cppm` -- porting Nsight alone
makes none of its 21 units measurable. The same correction applies to Aftermath and
`gse.vulkan`.

The sixteen that translate whole, for the record: gse.assert, gse.config, gse.containers,
gse.core, gse.format, gse.graph, gse.introspection, gse.json, gse.log, gse.math,
gse.meta, gse.shell, gse.stacktrace, gse.syntax, gse.test and gse.tests.contracts.
**gse.log and gse.stacktrace are among them**, which was not obvious: both are interfaces
over `std` alone, and everything Win32 about them is in their `.cpp`.

### 18e -- the eight rejections are two causes, and neither is a codegen gap

- **Six are `std::mutex` and `std::shared_mutex` not existing**: `WorkStealingQueue.cppm`,
  `Ecs/AccessToken.cppm`, `GpuBackend/Bindless.cppm`, `FrameResourceBin.cppm`,
  `WaitStation.cppm`, `Gui/Loading.cppm`. libstdc++ here is built against a threadless
  wasi-libc, so `<mutex>` declares nothing. This is the threads-to-workers item on the
  gating list arriving exactly where it was predicted to, and it is the item that decides
  how much more of the engine can be measured.
- **Two are exceptions**: `Math/Tests/Units.cppm` and `Math/Tests/Math.cppm` instantiate a
  throwing path in `Quantity.cppm:887` and fail with "exception handling disabled". The
  math *module* compiles and runs; its *tests* do not, which is the ~111-catch-sites item
  showing its first concrete instance.

Both are on the gating list already. **Nothing in the eight is the backend's.**

## Step 19: threads, and the two backend gaps nobody had measured

Step 18 named threads the largest engine-side unblocker and left the choice of *how* open:
a single-threaded gthread model that only makes `std::mutex` declare, or real pthreads.
**The owner chose real pthreads**, on the argument that it is the only path to actual
workers. That decision turned out to cost two backend patches rather than a configure
flag, because reading the backend first found something the gating list had never
recorded: **`gcc/config/wasm/` had no thread-local storage and no atomics at all.**

- `grep -i 'tls\|thread'` across the backend matched one unrelated comment.
- The only `atomic` hits were `wasm-asm.cc:492`'s `__atomic_compare_exchange_N`
  declaration fix-up, which is about a *declaration*, not an instruction.

Both are load-bearing for pthreads and neither is optional, so the honest shape of the
step was 19a-19d below: every cheap measurement first, the irreversible rebuild last.

### 19a -- the dialect, and `llc` as reference text

Three new subjects in `Tools/wasm_dialect_probe.py`, measured before a line of either
patch was written, which is steps 6, 7 and 12's order applied again.

| Subject | Result | What it settles |
|---|---|---|
| `tls` | **ran, 42** | `.tdata` + `"T"`, `global.get __tls_base`, `i32.const sym@TLSREL`, `R_WASM_MEMORY_ADDR_TLS_SLEB` |
| `atomics` | **ran, 42** | `i32.atomic.rmw.add`, `atomic.fence`, `i32.atomic.rmw.cmpxchg` over a shared memory |
| `thread_spawn` | **ran, 42** | `pthread_create`/`pthread_join` against the real threads archives, joined and observed |

**The reference text this file's step 6 called unavailable turned out to be available.**
Step 6 noted that MSYS2's rustc is not rustup-based, so there is no `core` for wasm32 and
no LLVM-authored asm to diff against. MSYS2's llvm package ships **`llc`**, so tiny LLVM
IR through `llc -mtriple=wasm32-unknown-unknown -mattr=+atomics,+bulk-memory
--thread-model=posix` gives LLVM's own canonical asm for a `thread_local` and an
`atomicrmw`. The `tls` and `atomics` subjects are that output reduced to what they
measure, so **the spellings `0015` and `0016` emit were read off LLVM rather than
guessed.** Worth remembering for the next instruction family.

**`thread_spawn` is the row the step was sequenced around, and it is not about the
dialect.** It asks whether anything can *host* threads for this target, because
`--enable-threads=posix` buying declarations that could never run would have been a bad
trade found late. It links `crt1-reactor.o` plus the real `libc.a` and `libpthread.a`
from the threads sysroot, and it is green: a spawned thread atomically adds to shared
memory, the main thread joins and reads the result. Three facts fall out of it:

- **The threads archives need no symbol GCC cannot supply** -- step 19's one unverified
  scoping assumption, closed. The link has no undefined symbols at all beyond the WASI
  imports and `wasi.thread-spawn`.
- **The spawn import is `wasi.thread-spawn`**, and the host owes the module only a fresh
  instance over the same memory with `wasi_thread_start (tid, arg)` called on it.
  `pthread_create` allocates the thread's stack and descriptor in linear memory itself,
  so the host needs to know nothing about either.
- **`--export-memory` is required and is not obvious**: `node:wasi` binds to
  `instance.exports.memory` and refuses a module that merely imports one, so a
  shared-memory module has to export the memory it imports.

**And one finding that deleted an option from the plan.** `wasm-ld` refuses thread-local
storage outright unless the object declares both the atomics and bulk-memory features --
and `-mattr` does *not* record them, it only tells `llvm-mc` what to accept. A subject
with TLS, `-mattr` and no `target_features` section is rejected; the same subject links
once the section is there. **So TLS and atomics cannot be delivered one at a time**, and
the scope's stop line -- `0015` landing alone behind `--disable-threads` -- does not
exist. Both patches or neither.

**The two halves are not symmetric, and getting that wrong cost a latent harness bug.**
TLS *assembles* with no `-mattr` at all and wants the features only at the link; an
atomic instruction without them is **rejected outright**, `instruction requires:
atomics`. `0015`'s first draft claimed `llvm-mc` "mis-type-checks rather than rejecting"
for both, carried over by analogy from `+exception-handling`, where that is what step 12
actually measured. The consequence was real: `build_gcc_wasm.py` assembled every probe
with no `-mattr` for the whole step and stayed green, because no probe's own translation
unit emitted an atomic -- the thread probes get theirs from libstdc++ objects the driver
assembled through `ASM_SPEC`. Found in review, and the repair is a guardrail rather than
a flag: `ASM_FEATURES` is the one copy of the list, `check_asm_features` refuses to run
when it disagrees with `ASM_SPEC` in the patched `wasm.h`, and `cmpxchg_fail` is a probe
whose own unit emits an atomic so the path is covered by a row and not by a comment.

### 19b -- `0015` native TLS and `0016` the atomic instructions

Full rationale for both is in `Tools/gcc-toolchain/wasm/README.md`. What matters here:

- **emutls is a different object model, not a slower TLS.** A `__thread` variable becomes
  a control object plus a per-thread copy from `__emutls_get_address`, and two compilers
  cannot share a variable across that boundary. Every prebuilt wasm library is built by
  clang, which gives a thread-local a real `.tdata` segment and no control object -- so
  the choice was never emutls-or-native, it was native-or-cannot-see-libc's-errno.
- **`0015` needs no new pattern in `wasm.md` and no `legitimize_address` hook**, which was
  the part expected to be expensive and was not. A thread-local's address takes three
  instructions where an ordinary symbol takes one, and that is still a drop-in
  replacement, because the operand code is emitting text for a stack machine and may push
  its value however it likes provided it leaves exactly one. This is the same property
  that made the whole llvm-mc port tractable in step 7.
- **`0016` is what `--enable-threads=posix` costs, and it was invisible before it.**
  libstdc++ uses `__gnu_cxx::__atomic_add_single` -- plain non-atomic arithmetic -- until
  a gthread model exists, then switches to the `__atomic` builtins. With no patterns those
  expanded to libatomic's ABI, libatomic has no wasm32 port and is configured off, so
  every one was an undefined symbol. Predicted in scoping and confirmed.
- **`atomic_compare_and_swap` is the only shape mismatch**: wasm's `cmpxchg` returns the
  value it found, the optab wants that and a boolean, so it is an expand over an insn with
  the boolean from `emit_store_flag`. Everything else maps onto the optabs directly.
- **Its memory set has to be opaque, and the first cut of `0016` got that wrong.** A
  compare-exchange writes memory only on success, so the second set cannot say
  `(set mem desired)` -- that is false for the failing case. It is an `unspec_volatile`
  over mem, expected and desired, which is the shape riscv's `atomic_cas_value_strong`
  and aarch64's `@aarch64_compare_and_swap` use. The sibling `atomic_exchange` *does*
  spell its memory set plainly and is right to: an exchange always writes.

  **Worth recording precisely, because the honest status is narrower than "miscompile".**
  Built against the false form, a failing compare-exchange followed by a load of the same
  address still returns the right answer at `-O2`, with a real load in the output, for a
  plain load as well as an atomic one. The reason is the sibling `unspec_volatile` in the
  same insn: it makes the whole pattern side-effecting, so no pass uses the second set's
  claim. So this was a false assertion to the middle end that happened to be masked, not
  an observed wrong answer -- and it is fixed anyway, because the masking is GCC's
  conservatism and not something the pattern is entitled to rely on. What is *not*
  available is a probe that would catch it coming back; `cmpxchg_fail` returns 7 either
  way and says so in its own comment.
- The memory-model operand is matched and ignored throughout. wasm has one ordering and it
  is sequential consistency, so a weaker model is served by a stronger guarantee, which is
  allowed. Same argument retires `is_weak`.

Both patches apply to the pinned `20260809` tree with no rejects and no fuzz, and both are
confined to `gcc/config/wasm/` -- so they keep the property step 15 identified as the
reason a snapshot bump is cheap: **`0005`-`0016` edit only files `0001` creates.** The
drift-exposed surface is unchanged at 16 hunks in 14 files.

### 19c -- the sysroot variant, and `plain_errno` deleted rather than inverted

`WASI_TARGET` is now `wasm32-wasip1-threads` and joins `WASI_SYSROOT_VERSION` in the
sysroot's cache key, because it decides what the measurement is *of* and not merely where
the files come from. `fetch_sysroot` splits: `stage_sysroot` extracts the multi-target
tarball once, and the flatten copies one target out of it -- which is also what lets
`wasm_dialect_probe.py` reach the threads archives directly, rather than whichever target
the cross happens to be configured for.

**`plain_errno` is gone, and that is the step's sharpest reversal.** Step 17 added it
because a non-threads wasi-libc's header claims `extern _Thread_local int errno` while its
clang-built `libc.a` puts errno in ordinary data; the rewrite made the declaration match
the definition. The threads variant ships a **byte-identical header** and a `libc.a` where
errno really *is* a TLS symbol, so on this variant the header is telling the truth and the
rewrite becomes step 17's own bug pointed the other way: C++ would read a non-thread-local
address libc never writes, and every thread but the first would see another thread's
errno. A green link bought with a silently wrong answer -- exactly what `errno_shared` was
built to catch. `0015` is what let the rewrite be deleted instead of inverted, and
`errno_shared` is the row that says the TLS addressing reaches the same storage libc does.

`-D_Thread_local=__thread` in `CXXFLAGS_FOR_TARGET` **stops being inert and becomes
load-bearing**. `_Thread_local` is C11 and not a C++ keyword, so without the define the
declaration `<cerrno>` reaches does not parse as thread-local at all -- and on this
variant it has to. Step 17's objection was to mapping it onto a target with no TLS; the
target has TLS now. The comment that said "the next reconfigure for any other reason is
where it goes" named this step, and the answer is that it stays.

### 19d -- a latent bug in the harness, found by being the first to reach it

`--enable-threads=posix` replaces `--disable-threads`. It is a configure argument, so it
discarded the build directory and rebuilt `all-gcc`, libgcc and libstdc++ -- which is why
19a and 19b went first.

**And it immediately died in `main()` with `UnboundLocalError: cannot access local
variable 'stale'`.** Step 18's graph staleness check had introduced a local named `stale`,
which shadows the `stale` *function* for the whole of `main` -- including the reconfigure
branch that calls it. Nothing had changed a configure argument since that line was
written, so the bug waited for the first run that did. Renamed to `unscanned`.

This is the same family as step 18c's and step 15's harness bugs and the third instance of
one rule: **a measurement harness's own untaken paths are untested code, and the step that
first takes one pays for it.** The reconfigure branch is the least-exercised path in this
script by construction -- it runs only when the toolchain's shape changes -- and it is
also the one whose failure is most expensive, because it fails after the build directory
has already been discarded.

### The probe table, and the layer 19c still had to pay for

`--enable-threads=posix` did not land in one go. Two things stood behind it that were
nobody's prediction, and both were in libgcc's gthreads layer rather than the backend:

- **First, `pthread_cancel`.** gthr-posix.h takes a weak reference to it and uses it as
  the proxy symbol `__gthread_active_p` tests. wasi-libc has no cancellation support by
  stated design, so libgcc's `emutls.c` would not compile. The file already had the shape
  of the answer -- bionic does not provide `pthread_cancel` either and there is a
  `__BIONIC__` arm at both sites -- so both arms gained `__wasi__`, which the backend does
  predefine. **That patch no longer exists**: see below.
- **Then the weak references themselves, which is `0017`.** With `pthread_cancel` fixed,
  every other `__gthrw` alias failed to assemble: "symbol pthread_key_create: missing
  .functype". A weakref alias is *referenced* without GCC ever declaring the target symbol
  external, so `ASM_OUTPUT_EXTERNAL` is never reached for it and the call arrives with no
  signature. The fix is again the file's own idiom -- `GTHREAD_USE_WEAK 0` for `__wasi__`,
  beside the `__MINGW32__` and glibc-2.34 arms -- and it is *right* rather than expedient:
  **wasi-libc's `libpthread.a` is an empty 8-byte archive and every pthread_* API is in
  `libc.a`**, measured with `llvm-ar t`, which is exactly the glibc-2.34 case that file
  already handles. There is no separate library for a weak reference to be weak against.
- **And the `pthread_cancel` patch was then deleted as dead, which review caught.** Both
  of its hunks live inside `#if SUPPORTS_WEAK && GTHREAD_USE_WEAK` regions
  (gthr-posix.h:116, and :188--299 for the proxy block), so with `GTHREAD_USE_WEAK` at 0
  the `__gthrw(pthread_cancel)` line expands to nothing and the proxy block is skipped
  entirely. Measured rather than argued: with it reverted and `0017` applied, `emutls.o`
  compiles. So **`0017` alone fixes the libgcc build**, the series is four patches and not
  five, and the note for whoever re-enables `GTHREAD_USE_WEAK` lives in `0017`'s header.
  It is a small instance of a rule this file keeps relearning: a repair written while a
  build is red is not automatically a repair the finished configuration needs.

  **What it leaves open, named rather than forgotten:** a weak reference that is actually
  *called* still has no signature on this target. `0010` covers the case libstdc++ uses --
  a weak reference never called, which `weak_undef` measures -- so nothing needs it today.
  Teaching the backend to chase weakref aliases into the import map is the real fix and is
  not done.

| Probe | Status | What changed |
|---|---|---|
| the 24 green rows of step 18 | unchanged | re-measured against a libgcc and libstdc++ rebuilt from scratch with threads |
| `errno_shared` | **ran** | the tripwire for 19c. errno is a real TLS symbol on this variant, so this row now says GCC's TLS addressing reaches the storage libc writes -- it was passing against a rewritten header before |
| `thread_local_shared` | **ran, 42** | new. A `thread_local` defined in one TU and read from another, which one TU cannot ask |
| `std_mutex` | **ran, 42** | new. The six rejected engine units' blocker, gone: `<mutex>` declares, and the lock resolves and runs |
| `std_thread` | **ran, 42** | new. A thread created, joined, and its result read back -- the row a declarations-only gthread model could never have reached |
| `cmpxchg_fail` | **ran, 7** | added in revision. A failing compare-exchange, and the only probe whose own unit emits an atomic — so it is the row that covers `ASM_FEATURES`, which nothing did |
| `throw_catch` | blocked: **trapped** | unchanged, still honest |
| `engine_math_run` | **ran, 361** | unchanged, which is the no-regression row for `0015`-`0017` |

**Two of this step's three harness bugs were in passing `shared` to the harness**, and
they are the same bug twice: the memory descriptor cannot be derived from the module --
`WebAssembly.Module.imports` reports a memory import's kind but never its flags -- so it
has to be passed, and it was wired into the dialect probe's `measure` and not into
build_gcc_wasm's. The probe failed with "memory import has no maximum limit", which names
the symptom and not the missing argument. The third was a comment: `CXX_RUNTIME_DEFINES`
was documented as carrying `-D_Thread_local=__thread` again and did not, so every probe
reaching `<cerrno>` failed on `'_Thread_local' does not name a type` -- a comment that
contradicted the code, written by the same edit that deleted `plain_errno`.

### The sweep, re-run: +10 units, and the rejection list is down to exceptions

| | step 18 | step 19 |
|---|---|---|
| units compiled for wasm32 | 121 | **131** |
| units the compiler rejected | 8 | **2** |
| units blocked behind a rejected one | 4 | **0** |
| units never offered: host-bound | 292 | 292 |
| modules that translate whole | 16 | 16 |

**The predicted delta and the measured one agree**, which is worth saying because this is
the step's only prediction that could have been checked against a number: the scope said
"the six std::mutex rejections plus the four units blocked behind them, so about ten
units, not thirteen modules." It is ten.

**The rejection list is now two files, and both are the exceptions item.**
`Math/Tests/{Units,Math}.cppm` instantiate a throwing path in `Quantity.cppm:887`. Every
one of step 18's six `std::mutex` rejections -- `WorkStealingQueue.cppm`,
`Ecs/AccessToken.cppm`, `GpuBackend/Bindless.cppm`, `FrameResourceBin.cppm`,
`WaitStation.cppm`, `Gui/Loading.cppm` -- compiles. So **nothing the compiler rejects is
about threads any more, and nothing it rejects is the backend's.**

**292 host-bound is unchanged, and that is the correct result rather than a
disappointment.** Threads were never what gated those: they are behind wrappers over
platform libraries, and 18d's arithmetic has not moved. What changed is that the first
move in that order -- `gse.moodycamel` + `gse.win32`, thirteen modules including the whole
spine -- is no longer waiting on anything, because moodycamel's queue needs `std::atomic`
and `std::mutex` and both now exist.

The module counts shift in a way that looks like a regression and is not: host-bound
modules read 47 where step 18 read 43, and failed modules 2 where it read 6. Those are the
same 49 modules; six of them carried a rejected unit before and now carry only host-bound
imports, so they move buckets. `translates=16` is unchanged, for the reason 18d gives --
a module translates whole only if nothing it imports is host-bound, and no wrapper was
ported here.

### What the gating list looks like now

1. **`long double` as binary128** -- unchanged, a fork rather than a bug. Now the oldest
   item on the list.
2. **`__int128`**, if wanted for its own sake; blocked on TImode move patterns (step 13).
3. **Real exceptions**: GCC-side EH region generation, with the two math test modules as
   its engine-side instance. **Now the largest item**, and its position is what step 19
   changed about this list rather than anything in its own text.
4. ~~**Threads**~~ **CLOSED, 2026-10-17 -- see "Step 19".** It was named the largest
   engine-side unblocker by step 18 and it is now a property of the toolchain: native TLS
   (`0015`), the atomic instructions (`0016`), `--enable-threads=posix` against the
   threads sysroot, and a Worker-backed `wasi.thread-spawn` in the harness. `std::mutex`
   declares, locks and runs; a `std::thread` is created, joined and its result read back.
5. **The engine-side port proper**, and it is now the *only* thing between the engine and
   a browser that is not exceptions. Still a *set* at a time, because no wrapper is the
   sole root of anything: `gse.moodycamel` + `gse.win32` is the first move and unblocks
   thirteen modules including the whole spine. Step 19 removes the reason to postpone it
   -- moodycamel's queue needs `std::atomic` and `std::mutex`, and both now exist.

---

## Step 20: the first wrapper is ported, and the engine's whole rejection list is one `throw`

The first step of the engine-side port proper, and it measured three things that were not
expected to be the finding: a false tripwire in this script's own patch stamps, a recorded
number that does not reproduce, and -- once `gse.moodycamel` stopped being invisible -- a
single `throw` statement standing in front of the entire spine.

### 20a -- `.gitattributes`, and a stamp that demanded a rebuild it did not need

The sweep would not start. `apply_patches` reported

    0001-wasm-New-backend.patch has changed since it was applied to ...
    Delete that source tree and re-run

which is a multi-hour toolchain rebuild, and the patch had not changed. The stamp holds
the LF digest of the file; the working tree held CRLF. `.gitattributes` pins
`Tools/gcc-toolchain/recipe/**` to LF for exactly this reason -- makepkg's sha256sums --
and never covered `Tools/gcc-toolchain/wasm/**`, so `core.autocrlf` rewrites all seventeen
patches on checkout and every digest with them. Checked against all fifteen applied
stamps: **every one matches its LF digest and none matches its on-disk bytes**, which is
the whole diagnosis in one table.

Fixed by extending `.gitattributes` and normalising the working tree back to LF -- a
`git diff` over the patch directory is empty afterwards, so no patch was re-cut and the
stamps match again. Worth recording because of the shape of the failure rather than its
size: the stamp is keyed on bytes deliberately, and keying on bytes in a repository that
rewrites bytes on checkout makes the tripwire fire on the checkout instead of on a re-cut
patch. A clean clone would also have fed CRLF patches to GCC's LF sources.

### 20b -- 131/2/0 does not reproduce; the measured baseline is 121/3/298

Re-measured before anything was changed, which is the only reason the rest of this step is
readable:

| | step 19, recorded | step 20, measured |
|---|---|---|
| units compiled | 131 | **121** |
| units rejected | 2 | **3** |
| units blocked | 0 | 0 |
| units never offered: host-bound | 292 | **298** |
| interface units in the sweep | 425 | **422** |
| modules that translate whole | 16 | **14** |

The arithmetic closes exactly -- `131 - 3 (interface units since deleted) - 6 (newly
host-bound) - 1 (newly rejected) = 121` -- so there is no unexplained regression, and the
tree having moved accounts for all of it but one row. **The third rejection is the one that
matters and it is not new work**: `Time/Timer.cppm` fails on
`Math/Units/Quantity.cppm:887`, and both files last changed in `18306057`, which predates
the step 19 sweep. So the recorded "2 rejections, both math test modules" was not
reproducible from the tree it was recorded against. Step 19's conclusion survives it --
nothing rejected is threads, codegen or the backend -- but its count does not, which is
what re-measuring a read number is for.

### 20c -- `gse.moodycamel` translates, and the measurement had to change to say so

`module_graph.py`'s host roots are structural: a `.cppm` under `Source/External` is a
wrapper, is never offered to the compiler, and propagates host-boundness to its
dependents. That premise -- "there is no such thing as porting one" -- is true of a
wrapper over a platform API and false of a wrapper over a *portable* vendored library.
`Moodycamel.cppm` is twelve lines over a header whose `GetCurrentThreadId` arm
(`concurrentqueue.h:97`) is one of four, and the arm wasm32 selects is the `thread_local`
address at `:158`. So the sweep could not have reported this module green no matter what
the compiler did.

`PORTED_HOST_ROOTS` is the mechanism: a wrapper named there is swept as an ordinary unit
and its verdict propagates like any other. It is kept honest by compiling the file every
run -- a wrapper that stops translating reports `failed`, which is news, rather than
`host-bound`, which is arithmetic. `gse.win32` is deliberately *not* in it: an empty module
behind `#ifdef _WIN32` still gates its callers' `win32::` uses, and that is the OS seam's
work rather than a measurement trick.

Two flag-parity notes, both the hazard 18c already recorded:
- there was no `-I Engine/External`, because until now no unit that `#include`s a
  vendored header was ever compiled. Without it the row reads FAILED on a header the
  engine finds. It belongs in `target_cxxflags` and not beside `ENGINE_CXXFLAGS` at the
  sweep's call site: the probe table reads its include flags from that same function, and
  an engine probe whose closure reaches moodycamel -- `closure_of="gse.time"` is the next
  one anybody would write -- has to be compiled with what the sweep used. Putting it on
  one side only is 18c's bug with the sides swapped, and the re-run confirms the shared
  spelling changes no row: every probe and every module verdict is identical.
- nothing else was needed. `-fno-exceptions` leaves `__EXCEPTIONS` undefined, so
  `MOODYCAMEL_EXCEPTIONS_ENABLED` is never defined and the header's `MOODYCAMEL_TRY/CATCH`
  degrade to `if constexpr (true) / else if constexpr (false)` on their own.

`Moodycamel.cppm` compiles. `Time/FrameSync.cppm`, the engine's only importer of it,
compiles behind it.

### 20d -- the win32 edge out of `gse.time:system_clock`, and what it exposed

Seven of `gse.time`'s nine interface units import nothing host-bound. `SystemClock.cppm`
imported `gse.win32` for **two calls in one function**, so the body of
`from_query_performance_counter` moved to `Time/Win32/SystemClock.cpp` as
`gse.time:system_clock_impl`, matching `Os/Win32/Window.cpp`. The declaration and its two
callers are untouched, and CMake globs impl units, so this is a refactor that stands
natively on its own. CMake's refreshed scan is the check: `gse.time:system_clock` now
requires `gse.time:clock, gse.math, gse.log` and the `gse.win32` edge sits on the `.cpp`,
which the graph does not scan.

Then the unit was offered to the compiler for the first time, and **failed on
`Quantity.cppm:887` again** -- `report_pacing`'s `"refresh={:.3f:ms}"` at
`SystemClock.cppm:174`.

### The sweep, re-run

| | before | after |
|---|---|---|
| units compiled | 121 | **123** |
| units rejected | 3 | **4** |
| units blocked behind a rejected one | 0 | **70** |
| units never offered: host-bound | 298 | **225** |
| modules that translate whole | 14 | **15** |
| modules host-bound | 46 | **37** |

Exactly three units changed verdict, all three from never-offered:
`External/Moodycamel.cppm` to ok, `Time/FrameSync.cppm` to ok,
`Time/SystemClock.cppm` to failed. The other 70 moved from `host-bound` to `blocked`, which
is the point: they were arithmetic and they are now a measurement with a named cause.

**`gse.moodycamel` is off the frontier entirely**, and it took the 13-module group with it:

| | before | after |
|---|---|---|
| `gse.win32` | sole 0, reached 33 | **sole 4**, reached 24 |
| `gse.moodycamel` | sole 0, reached 30 | gone |
| largest gated group | 13 modules behind `gse.moodycamel, gse.win32` | 6 modules behind five roots |
| | | **9 modules behind `gse.win32` (+`.environment`) alone** |

`sole 4` is the first non-zero `sole` column this engine has ever printed -- 18d recorded
that it "answers zero every time", and the answer was zero because every module behind
`gse.win32` was also behind moodycamel. Step 2 of the port is now exactly `gse.win32` and
`gse.win32.environment`.

### The finding: four rejections, one `throw`

`Math/Tests/Units.cppm`, `Math/Tests/Math.cppm`, `Time/Timer.cppm` and
`Time/SystemClock.cppm` are the engine's entire wasm32 rejection list, and all four are
the same statement:

    Quantity.cppm:887: error: exception handling disabled, use '-fexceptions' to enable
      throw format_error("gse quantity: dynamic unit must be spelled {}");

the only `throw` in all of `gse.math`. It is the malformed-spec branch of
`std::formatter<quantity>::parse`, unreachable for every format string that reaches it --
`"{:.3f:ms}"` takes the `else` arm -- but `-fno-exceptions` rejects the statement at
instantiation regardless of reachability. `gse.math` itself still translates whole, because
the throw is only instantiated when a quantity is formatted with a unit spec.

So **one line gates the spine**: eight modules and 70 units are blocked on
`gse.time:system_clock`, and `gse.time` is blocked on this. It is not the exceptions item
on the gating list -- no EH region generation is needed to compile a program that has no
reachable throw -- it is one site in engine code with three plausible conversions, each of
which changes what a native build does with a malformed format spec. That makes it the
owner's call rather than this step's.

### What the gating list looks like now

1. **`long double` as binary128** -- unchanged, a fork rather than a bug.
2. **`__int128`**, if wanted for its own sake; blocked on TImode move patterns (step 13).
3. **Real exceptions**: GCC-side EH region generation. **No longer has an engine-side
   instance.** All four engine rejections are one unreachable `throw`, which is an engine
   decision and not an EH gap -- this item is now wanted for `catch` sites the port has yet
   to reach, not for anything measured today.
4. **`Quantity.cppm:887`** -- new, and the whole of the engine's rejection list. Blocks
   `gse.time` and the eight modules behind it.
5. **`gse.win32` + `gse.win32.environment`** -- the port's next move, and the first one the
   frontier can price on its own: 9 modules behind those two, 4 behind `gse.win32` alone.
6. ~~**`gse.moodycamel`**~~ **CLOSED, 2026-10-18.** Translates; off the frontier.

## Step 21: the rejection list goes to zero, and `-fno-exceptions` turns out to be an instantiation rule

Step 20 ended with the engine's entire wasm32 rejection list being one `throw`. Three
conversions later the list is empty, and the blocked pool with it.

| | step 20 | step 21 |
|---|---|---|
| interface units offered and compiled | 152 | **197** |
| units rejected | 1 | **0** |
| units blocked behind a rejected one | 44 | **0** |
| units never offered: host-bound | 225 | 225 |
| modules that translate whole | 20 | **26** |
| modules host-bound | 37 | 37 |

Three sites, each converted on its own merits and each with the same spelling --
`#if __cpp_exceptions` / today's statements / `#else` / a non-throwing equivalent / `#endif`,
so that `__cpp_exceptions` being on natively makes every native arm *textually* unchanged:

1. `Math/Units/Quantity.cppm:887`, the malformed-spec branch of
   `std::formatter<quantity>::parse`; `#else` arm `__builtin_abort()`, which is no more a
   constant expression than `throw` is, so a malformed *literal* spec stays a compile error on
   both arms. Took rejections 4 to 1 and unblocked `gse.time`.
2. `Concurrency/Task.cppm:856-884`, `run_job`'s two log-a-stacktrace-and-continue handlers --
   a diagnostic boundary, not an error channel: no recovery, no rethrow, no value propagated.
3. `Core/ScopeExit.cppm:67-72`, `try { invoke } catch (...) { std::terminate(); }` in
   `~scope_exit() noexcept`. An escaping exception from a `noexcept` destructor already
   terminates, so the handler is redundant natively and the `#if` removes nothing. This is the
   one that closed the list: `failed` 1 to 0 and `blocked` 44 to 0 in a single edit.

`std::exception_ptr`, `std::current_exception` and `std::rethrow_exception` are declared
unconditionally in libstdc++, so `AsyncTask.cppm:259/277`'s bare `rethrow_exception` calls were
never rejections and needed nothing.

### The finding: only instantiated `catch` sites are blockers

`Concurrency/AsyncTask.cppm` and `Meta/Parse.cppm` both still hold `catch` sites and both sweep
`ok`. GCC rejects `try`/`catch` under `-fno-exceptions` at *instantiation*, not at parse.
`run_job` was an ordinary function and `~scope_exit` is instantiated by `make_scope_exit` at
`Task.cppm:821`; `async::sync_wait` and `parser<T>::parse`'s floating-point branch are
uninstantiated template bodies in every unit offered. So the engine's ~111 `catch` sites are not
a backlog to work through -- the blockers are only the ones something on the path instantiates,
and three of them existed.

Two sites therefore remain undecided rather than decided: `AsyncTask.cppm:383`'s
`ep = std::current_exception()` (a genuine, if function-local, error channel) and
`Parse.cppm:115`'s `std::stod` + `catch` (the only site where dropping the handler would change
an answer rather than remove an unreachable branch -- its `#else` arm would be `std::from_chars`,
the shape the integral branch twelve lines below already uses). Neither was converted
pre-emptively. A link step that instantiates them will have to answer them.

### What the gating list looks like now

1. **`long double` as binary128** -- unchanged, a fork rather than a bug.
2. **`__int128`**, if wanted for its own sake; blocked on TImode move patterns (step 13).
3. **Real exceptions**: GCC-side EH region generation. Still no engine-side instance.
4. ~~**`Quantity.cppm:887`**~~ **CLOSED.** With it, the whole rejection list.
5. **`gse.win32` + `gse.win32.environment`** -- now the only frontier. `gse.win32` sole 4,
   reached 24; `.environment` reached 19; 4 modules behind `gse.win32` alone, 5 behind the pair.
6. ~~**`gse.moodycamel`**~~ **CLOSED, 2026-10-18.**

Native stayed green throughout and `scene_reset_cpu` reprints `world-state hash
0xe836d31697879e27` after each conversion, which is the cheap proof that "the native arm is
textually unchanged" is a fact rather than an argument.
